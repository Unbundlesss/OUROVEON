//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"
#include "base/fio.h"
#include "base/instrumentation.h"
#include "base/text.h"
#include "base/text.transform.h"

#include "data/yamlutil.h"
#include "data/uuid.h"

#include "math/rng.h"
#include "spacetime/chronicle.h"

#include "app/module.frontend.fonts.h"

#include "endlesss/core.types.h"
#include "endlesss/core.constants.h"
#include "endlesss/cache.jams.h"
#include "endlesss/toolkit.warehouse.h"
#include "endlesss/api.h"

#include "app/core.h"

// carray.c
extern "C" { int sqlite3_carray_init( sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi ); }

namespace endlesss {
namespace toolkit {

// ---------------------------------------------------------------------------------------------------------------------
std::string Warehouse::createExportFilenameForJam( const types::JamCouchID& jamCouchID, const std::string_view jamName, const std::string_view fileExtension )
{
    std::string sanitisedJamName;
    base::sanitiseNameForPath( jamName, sanitisedJamName, '_', false );

    return fmt::format( FMTX( "orx.{}.{}.{}" ), jamCouchID, base::StrToLwrExt( sanitisedJamName ), fileExtension );
}


std::string Warehouse::m_databaseFile;

using StemSet   = absl::flat_hash_set< endlesss::types::StemCouchID >;

using Task      = std::unique_ptr<Warehouse::ITask>;
using TaskQueue = mcc::ConcurrentQueue<Task>;

// ---------------------------------------------------------------------------------------------------------------------
// worker thread task abstract interface; it owns a connection to the API routing
struct Warehouse::ITask
{
    ITask() {}
    virtual ~ITask() {}

    virtual bool usesNetwork() const { return false; }
    virtual bool shouldTriggerContentReport() const { return false; }

    // task can automatically increment the 'jam change' index that denotes the client app might want to refresh
    // jam views if their own change index no longer matches
    virtual bool shouldIncrementJamChangeIndex( types::JamCouchID& jamID ) const { return false; }

    virtual const char* getTag() const = 0;
    virtual std::string Describe() const = 0;
    virtual bool Work( TaskQueue& currentTasks ) = 0;
};

// version of the above for network-driven tasks
struct Warehouse::INetworkTask : Warehouse::ITask
{
    INetworkTask( const api::NetConfiguration& ncfg )
        : ITask()
        , m_netConfig( ncfg )
    {}
    ~INetworkTask() {}

    bool usesNetwork() const override { return true; }
    const api::NetConfiguration& m_netConfig;


    // some network tasks add in delay to help avoid attacking the endlesss servers too hard
    // TODO data drive these pauses
    void addNetworkPause() const
    {
        math::RNG32 rng;
        const auto networkPauseTime = std::chrono::milliseconds( rng.genInt32( 250, 750 ) );

        blog::database( "[{}] pausing for {}", getTag(), networkPauseTime );
        std::this_thread::sleep_for( networkPauseTime );
    }
};


// ---------------------------------------------------------------------------------------------------------------------
struct EmptyTask final : Warehouse::ITask
{
    static constexpr std::string_view Tag = "----";

    EmptyTask() = default;

    const char* getTag() const override { return Tag.data(); }
    std::string Describe() const override { return fmt::format( "flushing schedule" ); }
    bool Work( TaskQueue& currentTasks ) override { return true; }
};

// ---------------------------------------------------------------------------------------------------------------------
struct ContentsReportTask final : Warehouse::ITask
{
    static constexpr std::string_view Tag = "CONTENTS";

    ContentsReportTask( const Warehouse::ContentsReportCallback& callbackOnCompletion )
        : ITask()
        , m_reportCallback( callbackOnCompletion )
    {}

    Warehouse::ContentsReportCallback m_reportCallback;

    const char* getTag() const override { return Tag.data(); }
    std::string Describe() const override { return fmt::format( "[{}] creating database contents report", Tag ); }
    bool Work( TaskQueue& currentTasks ) override;
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamSliceTask final : Warehouse::ITask
{
    static constexpr std::string_view Tag = "JAMSLICE";

    JamSliceTask( const types::JamCouchID& jamCID, const Warehouse::JamSliceCallback& callbackOnCompletion )
        : ITask()
        , m_jamCID( jamCID )
        , m_reportCallback( callbackOnCompletion )
    {}

    types::JamCouchID               m_jamCID;
    Warehouse::JamSliceCallback     m_reportCallback;

    const char* getTag() const override { return Tag.data(); }
    std::string Describe() const override { return fmt::format( "[{}] extracting jam data for [{}]", Tag, m_jamCID.value() ); }
    bool Work( TaskQueue& currentTasks ) override;
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamSnapshotTask final : Warehouse::INetworkTask
{
    static constexpr std::string_view Tag = "SNAPSHOT";

    JamSnapshotTask( const api::NetConfiguration& ncfg, base::EventBusClient& eventBus, const types::JamCouchID& jamCID, const base::OperationID opID, Warehouse::RiffIDConflictHandling conflictHandling )
        : Warehouse::INetworkTask( ncfg )
        , m_eventBusClient( eventBus )
        , m_jamCID( jamCID )
        , m_operationID( opID )
        , m_conflictHandling( conflictHandling )
    {}

    // always trigger a contents update after snapping a new jam
    bool shouldTriggerContentReport() const override { return true; }

    // always pump the jam change index on snapshot, even if we got nothing new
    bool shouldIncrementJamChangeIndex( types::JamCouchID& jamID ) const override
    { 
        jamID = m_jamCID;
        return true;
    }


    base::EventBusClient                m_eventBusClient;
    types::JamCouchID                   m_jamCID;
    base::OperationID                   m_operationID;
    Warehouse::RiffIDConflictHandling   m_conflictHandling;

    const char* getTag() const override { return Tag.data(); }
    std::string Describe() const override { return fmt::format( "[{}] fetching Jam snapshot of [{}]", Tag, m_jamCID ); }
    bool Work( TaskQueue& currentTasks ) override;
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamPurgeTask final : Warehouse::ITask
{
    static constexpr std::string_view Tag = "PURGE";

    JamPurgeTask( const types::JamCouchID& jamCID )
        : Warehouse::ITask()
        , m_jamCID( jamCID )
    {}

    // always trigger a contents update after wiping out data
    bool shouldTriggerContentReport() const override { return true; }

    // jam change index up as we just nuked the data
    bool shouldIncrementJamChangeIndex( types::JamCouchID& jamID ) const override
    {
        jamID = m_jamCID;
        return true;
    }


    types::JamCouchID m_jamCID;

    const char* getTag() const override { return Tag.data(); }
    std::string Describe() const override { return fmt::format( "[{}] deleting all records for [{}]", Tag, m_jamCID ); }
    bool Work( TaskQueue& currentTasks ) override;
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamSyncAbortTask final : Warehouse::ITask
{
    static constexpr std::string_view Tag = "SYNC-ABORT";

    JamSyncAbortTask( const types::JamCouchID& jamCID )
        : Warehouse::ITask()
        , m_jamCID( jamCID )
    {}

    // always trigger a contents update after wiping out data
    bool shouldTriggerContentReport() const override { return true; }

    types::JamCouchID m_jamCID;

    const char* getTag() const override { return Tag.data(); }
    std::string Describe() const override { return fmt::format( "[{}] purging empty riff records for [{}]", Tag, m_jamCID ); }
    bool Work( TaskQueue& currentTasks ) override;
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamExportTask final : Warehouse::ITask
{
    static constexpr std::string_view Tag = "EXPORT";

    JamExportTask( base::EventBusClient& eventBus, const types::JamCouchID& jamCID, const fs::path& exportFolder, std::string_view jamName, const base::OperationID opID )
        : Warehouse::ITask()
        , m_eventBusClient( eventBus )
        , m_jamCID( jamCID )
        , m_exportFolder( exportFolder )
        , m_jamName( jamName )
        , m_operationID( opID )
    {}

    base::EventBusClient    m_eventBusClient;
    types::JamCouchID       m_jamCID;
    fs::path                m_exportFolder;
    std::string             m_jamName;
    base::OperationID       m_operationID;

    const char* getTag() const override { return Tag.data(); }
    std::string Describe() const override { return fmt::format( "[{}] exporting jam to disk", Tag ); }
    bool Work( TaskQueue& currentTasks ) override;
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamImportTask final : Warehouse::ITask
{
    static constexpr std::string_view Tag = "IMPORT";

    JamImportTask( base::EventBusClient& eventBus, const fs::path& fileToImport, const base::OperationID opID )
        : Warehouse::ITask()
        , m_eventBusClient( eventBus )
        , m_fileToImport( fileToImport )
        , m_operationID( opID )
    {}

    base::EventBusClient    m_eventBusClient;
    fs::path                m_fileToImport;
    base::OperationID       m_operationID;

    // rebuild after add
    bool shouldTriggerContentReport() const override { return true; }

    const char* getTag() const override { return Tag.data(); }
    std::string Describe() const override { return fmt::format( "[{}] importing jam from disk", Tag ); }
    bool Work( TaskQueue& currentTasks ) override;
};

// ---------------------------------------------------------------------------------------------------------------------
struct GetRiffDataTask final : Warehouse::INetworkTask
{
    static constexpr std::string_view Tag = "RIFFDATA";

    GetRiffDataTask( const api::NetConfiguration& ncfg, const types::JamCouchID& jamCID, const std::vector< types::RiffCouchID >& riffCIDs )
        : Warehouse::INetworkTask( ncfg )
        , m_jamCID( jamCID )
        , m_riffCIDs( riffCIDs )
    {}

    types::JamCouchID                 m_jamCID;
    std::vector< types::RiffCouchID > m_riffCIDs;

    const char* getTag() const override { return Tag.data(); }
    std::string Describe() const override { return fmt::format( "[{}] pulling {} riff details", Tag, m_riffCIDs.size() ); }
    bool Work( TaskQueue& currentTasks ) override;
};

// ---------------------------------------------------------------------------------------------------------------------
struct GetStemData final : Warehouse::INetworkTask
{
    static constexpr std::string_view Tag = "STEMDATA";

    GetStemData( const api::NetConfiguration& ncfg, const types::JamCouchID& jamCID, const std::vector< types::StemCouchID >& stemCIDs )
        : Warehouse::INetworkTask( ncfg )
        , m_jamCID( jamCID )
        , m_stemCIDs( stemCIDs )
    {}

    types::JamCouchID                 m_jamCID;
    std::vector< types::StemCouchID > m_stemCIDs;

    const char* getTag() const override { return Tag.data(); }
    std::string Describe() const override { return fmt::format( "[{}] pulling {} stem details", Tag, m_stemCIDs.size() ); }
    bool Work( TaskQueue& currentTasks ) override;
};



struct Warehouse::TaskSchedule
{
    TaskQueue                           m_taskQueue;
    moodycamel::LightweightSemaphore    m_workerWaitSema;

    template< typename TObject, typename... Args >
    void enqueueWorkTask( Args&&... args )
    {
        m_taskQueue.enqueue( std::make_unique< TObject >( std::forward< Args >( args )... ) );
        signal();
    }

    void signal()
    {
        // signal for 2 passes, the latter to include updating state via m_cbWorkUpdate once everything is cleared out
        // of the task queue
        m_workerWaitSema.signal(2);
    }
};

namespace sql {

#define DEPRECATE_INDEX     R"( DROP INDEX IF EXISTS )"

// ---------------------------------------------------------------------------------------------------------------------
namespace jams {

    static constexpr char createTable[] = R"(
        CREATE TABLE IF NOT EXISTS "Jams" (
            "JamCID"        TEXT NOT NULL UNIQUE,
            "PublicName"    TEXT NOT NULL,
            PRIMARY KEY("JamCID")
        );)";
    static constexpr char createIndex_0[] = R"(
        CREATE UNIQUE INDEX IF NOT EXISTS "Jams_IndexJam"       ON "Jams" ( "JamCID" );)";

    static constexpr char deprecated_0[] = { DEPRECATE_INDEX "IndexJam" };

    // -----------------------------------------------------------------------------------------------------------------
    static void runInit()
    {
        Warehouse::SqlDB::query<createTable>();

        Warehouse::SqlDB::query<createIndex_0>();

        // deprecate
        Warehouse::SqlDB::query<deprecated_0>();
    }

    // -----------------------------------------------------------------------------------------------------------------
    static bool getPublicNameForID( const types::JamCouchID& jamCID, std::string& publicName )
    {
        static constexpr char _sqlGetJamByID[] = R"(
            select PublicName from jams where JamCID is ?1;
        )";

        auto query = Warehouse::SqlDB::query<_sqlGetJamByID>( jamCID.value() );

        return query( publicName );
    }

} // namespace jams

// ---------------------------------------------------------------------------------------------------------------------
namespace riffs {

    static constexpr char createTable[] = R"(
        CREATE TABLE IF NOT EXISTS "Riffs" (
            "RiffCID"       TEXT NOT NULL UNIQUE,
            "OwnerJamCID"   TEXT NOT NULL,
            "CreationTime"  INTEGER,
            "Root"          INTEGER,
            "Scale"         INTEGER,
            "BPS"           REAL,
            "BPMrnd"        REAL,
            "BarLength"     INTEGER,
            "AppVersion"    INTEGER,
            "Magnitude"     REAL,
            "UserName"      TEXT,
            "StemCID_1"     TEXT,
            "StemCID_2"     TEXT,
            "StemCID_3"     TEXT,
            "StemCID_4"     TEXT,
            "StemCID_5"     TEXT,
            "StemCID_6"     TEXT,
            "StemCID_7"     TEXT,
            "StemCID_8"     TEXT,
            "GainsJSON"     TEXT,
            PRIMARY KEY("RiffCID")
        );)";
    static constexpr char unpackSingleRiff[] = R"(
        SELECT 
            RiffCID,
            OwnerJamCID,
            CreationTime,
            Root,
            Scale,
            BPS,
            BPMrnd,
            BarLength,
            AppVersion,
            Magnitude,
            UserName,
            StemCID_1,
            StemCID_2,
            StemCID_3,
            StemCID_4,
            StemCID_5,
            StemCID_6,
            StemCID_7,
            StemCID_8,
            GainsJSON
            FROM Riffs where RiffCID is ?1;
        )";
    static constexpr char createIndex_0[] = R"(
        CREATE UNIQUE INDEX IF NOT EXISTS "Riff_IndexRiff"       ON "Riffs" ( "RiffCID" );)";
    static constexpr char createIndex_1[] = R"(
        CREATE INDEX        IF NOT EXISTS "Riff_IndexOwner"      ON "Riffs" ( "OwnerJamCID" );)";
    static constexpr char createIndex_2[] = R"(
        CREATE INDEX        IF NOT EXISTS "Riff_IndexTime"       ON "Riffs" ( "CreationTime" DESC );)";
    static constexpr char createIndex_3[] = R"(
        CREATE INDEX        IF NOT EXISTS "Riff_IndexUser"       ON "Riffs" ( "UserName" );)";
    static constexpr char createIndex_4[] = R"(
        CREATE INDEX        IF NOT EXISTS "Riff_IndexBPM"        ON "Riffs" ( "BPMrnd" );)";
    static constexpr char createIndex_5[] = R"(
        CREATE INDEX        IF NOT EXISTS "Riff_IndexStems"      ON "Riffs" ( "StemCID_1", "StemCID_2", "StemCID_3", "StemCID_4", "StemCID_5", "StemCID_6", "StemCID_7", "StemCID_8" );)";
    static constexpr char createIndex_6[] = R"(
        CREATE INDEX        IF NOT EXISTS "Riff_IndexOwner2Time" ON "Riffs" ( "OwnerJamCID", "CreationTime" );)";  // w. stem index, accelerates contents report a bunch (300 -> 70ms)
    static constexpr char createIndex_7[] = R"(
        CREATE INDEX        IF NOT EXISTS "Riff_IndexOwner2Ver"  ON "Riffs" ( "OwnerJamCID", "AppVersion" );)";  // new stem indexing
    static constexpr char createIndex_8[] = R"(
        CREATE INDEX        IF NOT EXISTS "Riff_RootScaleHash"   ON "Riffs" ( ( ( root << 8 ) | scale ) );)";  // finding riffs by bitwise merged root/scale value
    static constexpr char createIndex_9[] = R"(
        CREATE INDEX        IF NOT EXISTS "Riff_BPMAndOwner"   ON "Riffs" ( round(BPMrnd), "OwnerJamCID" );)";  // procgen searching without root/scale


    static constexpr char deprecated_0[] = { DEPRECATE_INDEX "IndexRiff" };
    static constexpr char deprecated_1[] = { DEPRECATE_INDEX "IndexOwner" };
    static constexpr char deprecated_2[] = { DEPRECATE_INDEX "IndexTime" };
    static constexpr char deprecated_3[] = { DEPRECATE_INDEX "IndexUser" };
    static constexpr char deprecated_4[] = { DEPRECATE_INDEX "IndexBPM" };
    static constexpr char deprecated_5[] = { DEPRECATE_INDEX "IndexStems" };

    // -----------------------------------------------------------------------------------------------------------------
    static void runInit()
    {
        Warehouse::SqlDB::query<createTable>();

        Warehouse::SqlDB::query<createIndex_0>();
        Warehouse::SqlDB::query<createIndex_1>();
        Warehouse::SqlDB::query<createIndex_2>();
        Warehouse::SqlDB::query<createIndex_3>();
        Warehouse::SqlDB::query<createIndex_4>();
        Warehouse::SqlDB::query<createIndex_5>();
        Warehouse::SqlDB::query<createIndex_6>();
        Warehouse::SqlDB::query<createIndex_7>();
        Warehouse::SqlDB::query<createIndex_8>();
        Warehouse::SqlDB::query<createIndex_9>();

        // deprecate
        Warehouse::SqlDB::query<deprecated_0>();
        Warehouse::SqlDB::query<deprecated_1>();
        Warehouse::SqlDB::query<deprecated_2>();
        Warehouse::SqlDB::query<deprecated_3>();
        Warehouse::SqlDB::query<deprecated_4>();
        Warehouse::SqlDB::query<deprecated_5>();
    }

    // -----------------------------------------------------------------------------------------------------------------
    static int64_t countRiffsInJam( const types::JamCouchID& jamCID )
    {
        static constexpr char _sqlCountTotalRiffsInJam[] = R"(
            select count(*) from riffs where OwnerJamCID is ?1;
        )";
    
        auto countRiffsRow = Warehouse::SqlDB::query<_sqlCountTotalRiffsInJam>( jamCID.value() );
        int64_t totalRiffsInJam = 0;
        countRiffsRow( totalRiffsInJam );

        return totalRiffsInJam;
    }

    // -----------------------------------------------------------------------------------------------------------------
    static int64_t countPopulated( const types::JamCouchID& jamCID, bool populated )
    {
        static constexpr char _sqlCountTotalRiffsInJamUnpopulated[] = R"(
            select count(*) from riffs where OwnerJamCID is ?1 and AppVersion is null;
        )";
        static constexpr char _sqlCountTotalRiffsInJamPopulated[] = R"(
            select count(*) from riffs where OwnerJamCID is ?1 and AppVersion is not null;
        )";

        int64_t totalSpecificRiffs = 0;

        if ( populated )
        {
            auto countRiffsRow = Warehouse::SqlDB::query<_sqlCountTotalRiffsInJamPopulated>( jamCID.value() );
            countRiffsRow( totalSpecificRiffs );
        }
        else
        {
            auto countRiffsRow = Warehouse::SqlDB::query<_sqlCountTotalRiffsInJamUnpopulated>( jamCID.value() );
            countRiffsRow( totalSpecificRiffs );
        }

        return totalSpecificRiffs;
    }

    // -----------------------------------------------------------------------------------------------------------------
    // find a single empty riff in a jam; from there we can maybe find many more to batch up
    static bool findUnpopulated( types::JamCouchID& jamCID, types::RiffCouchID& riffCID )
    {
        static constexpr char findEmptyRiff[] = R"(
            select OwnerJamCID, riffCID from riffs where AppVersion is null limit 1 )";

        std::string _jamCID, _riffCID;

        auto query = Warehouse::SqlDB::query<findEmptyRiff>();
        query( _jamCID, _riffCID );

        new (&jamCID) types::JamCouchID{ _jamCID };
        new (&riffCID) types::JamCouchID{ _riffCID };

        return !jamCID.empty();
    }

    // -----------------------------------------------------------------------------------------------------------------
    static bool findUnpopulatedBatch( const types::JamCouchID& jamCID, const int32_t maximumRiffsToFind, std::vector<types::RiffCouchID>& riffCIDs )
    {
        static constexpr char findEmptyRiffsInJam[] = R"(
            select riffCID from riffs where OwnerJamCID is ?1 and AppVersion is null limit ?2 )";

        auto query = Warehouse::SqlDB::query<findEmptyRiffsInJam>( jamCID.value(), maximumRiffsToFind );

        riffCIDs.clear();
        riffCIDs.reserve( maximumRiffsToFind );

        std::string_view riffCID;
        while ( query( riffCID ) )
        {
            riffCIDs.emplace_back( riffCID );
        }

        return !riffCIDs.empty();
    }

    // -----------------------------------------------------------------------------------------------------------------
    static void findUniqueJamIDs( std::vector<types::JamCouchID>& jamCIDs )
    {
        static constexpr char findDistinctJamCIDs[] = R"(
            select distinct OwnerJamCID from riffs )";

        auto query = Warehouse::SqlDB::query<findDistinctJamCIDs>();

        jamCIDs.clear();

        std::string_view jamCID;
        while ( query( jamCID ) )
        {
            jamCIDs.emplace_back( jamCID );
        }
    }

    // -----------------------------------------------------------------------------------------------------------------
    static bool getSingleByID( const types::RiffCouchID& riffCID, endlesss::types::Riff& outRiff )
    {
        auto query = Warehouse::SqlDB::query<unpackSingleRiff>( riffCID.value() );

        std::string_view riffID, jamID;
        std::string_view stem1, stem2, stem3, stem4, stem5, stem6, stem7, stem8, gainsJson;

        if ( !query( riffID,
                     jamID,
                     outRiff.creationTimeUnix,
                     outRiff.root,
                     outRiff.scale,
                     outRiff.BPS,
                     outRiff.BPMrnd,
                     outRiff.barLength,
                     outRiff.appVersion,
                     outRiff.magnitude,
                     outRiff.user,
                     stem1,
                     stem2,
                     stem3,
                     stem4,
                     stem5,
                     stem6,
                     stem7,
                     stem8,
                     gainsJson ) )
            return false;

        outRiff.couchID = types::RiffCouchID{ riffID };
        outRiff.jamCouchID = types::JamCouchID{ jamID };

        outRiff.stems[0] = endlesss::types::StemCouchID{ stem1 };
        outRiff.stems[1] = endlesss::types::StemCouchID{ stem2 };
        outRiff.stems[2] = endlesss::types::StemCouchID{ stem3 };
        outRiff.stems[3] = endlesss::types::StemCouchID{ stem4 };
        outRiff.stems[4] = endlesss::types::StemCouchID{ stem5 };
        outRiff.stems[5] = endlesss::types::StemCouchID{ stem6 };
        outRiff.stems[6] = endlesss::types::StemCouchID{ stem7 };
        outRiff.stems[7] = endlesss::types::StemCouchID{ stem8 };
        
        for ( size_t stemI = 0; stemI < 8; stemI++ )
        {
            outRiff.stemsOn[stemI] = !( outRiff.stems[stemI].empty() );
        }

        try 
        {
            nlohmann::json gains = nlohmann::json::parse( gainsJson );

            outRiff.gains[0] = gains[0];
            outRiff.gains[1] = gains[1];
            outRiff.gains[2] = gains[2];
            outRiff.gains[3] = gains[3];
            outRiff.gains[4] = gains[4];
            outRiff.gains[5] = gains[5];
            outRiff.gains[6] = gains[6];
            outRiff.gains[7] = gains[7];
        }
        catch ( const nlohmann::json::parse_error& pe )
        {
            blog::error::app( "json parse error [{}] in {}", pe.what(), __FUNCTION__ );
            return false;
        }

        return true;
    }
} // namespace riffs

// ---------------------------------------------------------------------------------------------------------------------
namespace tags {

    static constexpr bool bVerboseLog = true;

    static constexpr char createTable[] = R"(
        CREATE TABLE IF NOT EXISTS "Tags" (
            "RiffCID"       TEXT NOT NULL UNIQUE,
            "OwnerJamCID"   TEXT NOT NULL,
            "Ordering"      INTEGER,
            "Timestamp"     INTEGER,
            "Favour"        INTEGER,
            "Note"          TEXT,
            PRIMARY KEY("RiffCID")
        );)";
    static constexpr char createIndex_0[] = R"(
        CREATE UNIQUE INDEX IF NOT EXISTS "Riff_IndexRiff"       ON "Tags" ( "RiffCID" );)";
    static constexpr char createIndex_1[] = R"(
        CREATE INDEX        IF NOT EXISTS "Riff_IndexOwner"      ON "Tags" ( "OwnerJamCID" );)";
    static constexpr char createIndex_2[] = R"(
        CREATE INDEX        IF NOT EXISTS "Riff_IndexOrdering"   ON "Tags" ( "Ordering" );)";
    static constexpr char createIndex_3[] = R"(
        CREATE INDEX        IF NOT EXISTS "Riff_IndexTimestamp"  ON "Tags" ( "Timestamp" );)";


    // -----------------------------------------------------------------------------------------------------------------
    static void runInit()
    {
        Warehouse::SqlDB::query<createTable>();

        Warehouse::SqlDB::query<createIndex_0>();
        Warehouse::SqlDB::query<createIndex_1>();
        Warehouse::SqlDB::query<createIndex_2>();
    }

    // -----------------------------------------------------------------------------------------------------------------
    // version of upsert without inline transaction guard - so other functions can choose how to wrap or batch
    namespace details
    {
        static void upsert_unguarded( endlesss::types::RiffTag tag, Warehouse::TagUpdateCallback& tagUpdateCb )
        {
            int32_t orderingValue = tag.m_order;

            // append ordering requested
            if ( orderingValue < 0 )
            {
                int32_t newHighestOrderingValue = 0;

                static constexpr char _findHighestCurrentOrdering[] = R"(
                select Ordering from Tags where OwnerJamCID = ?1 order by Ordering desc limit 1;
                )";

                auto findHighestOrdering = Warehouse::SqlDB::query<_findHighestCurrentOrdering>( tag.m_jam.value() );
                findHighestOrdering( newHighestOrderingValue );

                orderingValue = newHighestOrderingValue + 1;

                if ( bVerboseLog )
                {
                    blog::database( FMTX( "tag upsert [{}] with append-ordering value [{}]" ), tag.m_riff, orderingValue );
                }

                tag.m_order = orderingValue;
            }
            else
            {
                if ( bVerboseLog )
                {
                    blog::database( FMTX( "tag upsert [{}] with specific ordering value [{}]" ), tag.m_riff, orderingValue );
                }
            }

            static constexpr char _insertOrUpdateTagData[] = R"(
                INSERT INTO Tags( OwnerJamCID, riffCID, Ordering, Timestamp, Favour, Note ) VALUES( ?1, ?2, ?3, ?4, ?5, ?6 )
                ON CONFLICT(riffCID) DO UPDATE SET
                    Ordering = ?3, Timestamp = ?4, Favour = ?5, Note = ?6
                )";

            Warehouse::SqlDB::query<_insertOrUpdateTagData>(
                tag.m_jam.value(),
                tag.m_riff.value(),
                tag.m_order,
                tag.m_timestamp,
                tag.m_favour,
                tag.m_note
            );

            if ( tagUpdateCb != nullptr )
                tagUpdateCb( tag );
        }
    }

    static void upsert( const endlesss::types::RiffTag& tag, Warehouse::TagUpdateCallback& tagUpdateCb )
    {
        Warehouse::SqlDB::TransactionGuard txn;
        details::upsert_unguarded( tag, tagUpdateCb );
    }

    // -----------------------------------------------------------------------------------------------------------------
    static void remove( const endlesss::types::RiffTag& tag, Warehouse::TagRemovedCallback& tagRemoveCb )
    {
        if ( bVerboseLog )
        {
            blog::database( FMTX( "tag deletion [{}]" ), tag.m_riff );
        }

        static constexpr char _deleteTagData[] = R"(
            delete from Tags where riffCID = ?1;
            )";

        Warehouse::SqlDB::query<_deleteTagData>( tag.m_riff.value() );

        if ( tagRemoveCb != nullptr )
            tagRemoveCb( tag.m_riff );
    }

    // -----------------------------------------------------------------------------------------------------------------
    static bool isRiffTagged( const endlesss::types::RiffCouchID& riffID, endlesss::types::RiffTag* tagOutput /*= nullptr */ )
    {
        static constexpr char findSingleTagForRiffID[] = R"(
            select OwnerJamCID, riffCID, Ordering, Timestamp, Favour, Note from Tags where riffCID is ?1 
            )";

        auto query = Warehouse::SqlDB::query<findSingleTagForRiffID>( riffID.value() );

        std::string_view outJamCID;
        std::string_view outRiffCID;
        int32_t          outOrdering;
        uint64_t         outTimestamp;
        int32_t          outFavour;
        std::string_view outNote;

        if ( query( outJamCID, outRiffCID, outOrdering, outTimestamp, outFavour, outNote ) )
        {
            if ( tagOutput != nullptr )
            {
                *tagOutput = endlesss::types::RiffTag(
                    types::JamCouchID( outJamCID ),
                    types::RiffCouchID( outRiffCID ),
                    outOrdering,
                    outTimestamp,
                    outFavour,
                    outNote );
            }
            return true;
        }
        return false;
    }

    // -----------------------------------------------------------------------------------------------------------------
    static std::size_t forJam( const types::JamCouchID& jamCID, std::vector<types::RiffTag>& outputTags )
    {
        static constexpr char findAllTagsForJam[] = R"(
            select OwnerJamCID, riffCID, Ordering, Timestamp, Favour, Note from Tags where OwnerJamCID is ?1 order by Ordering asc
            )";

        auto query = Warehouse::SqlDB::query<findAllTagsForJam>( jamCID.value() );

        outputTags.clear();

        std::string_view outJamCID;
        std::string_view outRiffCID;
        int32_t          outOrdering;
        uint64_t         outTimestamp;
        int32_t          outFavour;
        std::string_view outNote;

        while ( query( outJamCID, outRiffCID, outOrdering, outTimestamp, outFavour, outNote ) )
        {
            outputTags.emplace_back(
                types::JamCouchID( outJamCID ),
                types::RiffCouchID( outRiffCID ),
                outOrdering,
                outTimestamp,
                outFavour,
                outNote );
        }
        return outputTags.size();
    }

    // -----------------------------------------------------------------------------------------------------------------
    static void batchUpdate( const std::vector<endlesss::types::RiffTag>& inputTags, Warehouse::TagUpdateCallback& tagUpdateCb )
    {
        Warehouse::SqlDB::TransactionGuard txn;
        for ( const auto& tag : inputTags )
        {
            details::upsert_unguarded( tag, tagUpdateCb );
        }
    }

    // -----------------------------------------------------------------------------------------------------------------
    static void batchRemoveAll( const endlesss::types::JamCouchID& jamCID )
    {
        static constexpr char _deleteAllTags[] = R"(
            delete from Tags where OwnerJamCID = ?1;
            )";

        Warehouse::SqlDB::query<_deleteAllTags>( jamCID.value() );
    }

} // namespace tags

// ---------------------------------------------------------------------------------------------------------------------
namespace stems {

    static constexpr char createTable[] = R"(
        CREATE TABLE IF NOT EXISTS "Stems" (
            "StemCID"           TEXT NOT NULL UNIQUE,
            "OwnerJamCID"       TEXT NOT NULL,
            "CreationTime"      INTEGER,
            "FileEndpoint"      TEXT,
            "FileBucket"        TEXT,
            "FileKey"           TEXT,
            "FileMIME"          TEXT,
            "FileLength"        INTEGER,
            "BPS"               REAL,
            "BPMrnd"            REAL,
            "Instrument"        INTEGER,
            "Length16s"         REAL,
            "OriginalPitch"     REAL,
            "BarLength"         REAL,
            "PresetName"        TEXT,
            "CreatorUserName"   TEXT,
            "SampleRate"        INTEGER,
            "PrimaryColour"     TEXT,
            PRIMARY KEY("StemCID")
        );)";
    static constexpr char unpackSingleStem[] = R"(
        SELECT 
            StemCID,
            OwnerJamCID,
            CreationTime,
            FileEndpoint,
            FileBucket,
            FileKey,
            FileMIME,
            FileLength,
            BPS,
            BPMrnd,
            Instrument,
            Length16s,
            OriginalPitch,
            BarLength,
            PresetName,
            CreatorUserName,
            SampleRate,
            PrimaryColour
            FROM Stems where StemCID is ?1;
        )";
    static constexpr char createIndex_0[] = R"(
        CREATE UNIQUE INDEX IF NOT EXISTS "Stems_IndexStem"       ON "Stems" ( "StemCID" );)";
    static constexpr char createIndex_1[] = R"(
        CREATE INDEX        IF NOT EXISTS "Stems_IndexPreset"     ON "Stems" ( "PresetName" );)";
    static constexpr char createIndex_2[] = R"(
        CREATE INDEX        IF NOT EXISTS "Stems_IndexTime"       ON "Stems" ( "CreationTime" DESC );)";
    static constexpr char createIndex_3[] = R"(
        CREATE INDEX        IF NOT EXISTS "Stems_IndexUser"       ON "Stems" ( "CreatorUserName" );)";
    static constexpr char createIndex_4[] = R"(
        CREATE INDEX        IF NOT EXISTS "Stems_IndexBPM"        ON "Stems" ( "BPMrnd" );)";
    static constexpr char createIndex_5[] = R"(
        CREATE INDEX        IF NOT EXISTS "Stems_IndexOwner"      ON "Stems" ( "OwnerJamCID" );)";
    static constexpr char createIndex_6[] = R"(
        CREATE INDEX        IF NOT EXISTS "Stems_IndexOwner2Time" ON "Stems" ( "OwnerJamCID", "CreationTime" );)";  // w. riff index, accelerates contents report a bunch (300 -> 70ms)
    static constexpr char createIndex_7[] = R"(
        CREATE INDEX        IF NOT EXISTS "Stems_IndexOwnerSlice" ON "Stems" ( "OwnerJamCID", "CreationTime" is not null );)";  // accelerates jam slice task filtering considerably

    static constexpr char deprecated_0[] = { DEPRECATE_INDEX "IndexStem" };
    static constexpr char deprecated_1[] = { DEPRECATE_INDEX "IndexPreset" };
    static constexpr char deprecated_2[] = { DEPRECATE_INDEX "IndexTime" };
    static constexpr char deprecated_3[] = { DEPRECATE_INDEX "IndexUser" };
    static constexpr char deprecated_4[] = { DEPRECATE_INDEX "IndexBPM" };

    // -----------------------------------------------------------------------------------------------------------------
    static void runInit()
    {
        Warehouse::SqlDB::query<createTable>();

        Warehouse::SqlDB::query<createIndex_0>();
        Warehouse::SqlDB::query<createIndex_1>();
        Warehouse::SqlDB::query<createIndex_2>();
        Warehouse::SqlDB::query<createIndex_3>();
        Warehouse::SqlDB::query<createIndex_4>();
        Warehouse::SqlDB::query<createIndex_5>();
        Warehouse::SqlDB::query<createIndex_6>();
        Warehouse::SqlDB::query<createIndex_7>();

        // deprecate
        Warehouse::SqlDB::query<deprecated_0>();
        Warehouse::SqlDB::query<deprecated_1>();
        Warehouse::SqlDB::query<deprecated_2>();
        Warehouse::SqlDB::query<deprecated_3>();
        Warehouse::SqlDB::query<deprecated_4>();
    }

    // -----------------------------------------------------------------------------------------------------------------
    static int64_t countPopulated( const types::JamCouchID& jamCID, bool populated )
    {
        static constexpr char _sqlCountTotalStemsInJamUnpopulated[] = R"(
            select count(*) from stems where OwnerJamCID is ?1 and CreationTime is null;
        )";
        static constexpr char _sqlCountTotalStemsInJamPopulated[] = R"(
            select count(*) from stems where OwnerJamCID is ?1 and CreationTime is not null;
        )";

        int64_t totalSpecificStems = 0;

        if ( populated )
        {
            auto countStemsRow = Warehouse::SqlDB::query<_sqlCountTotalStemsInJamPopulated>( jamCID.value() );
            countStemsRow( totalSpecificStems );
        }
        else
        {
            auto countStemsRow = Warehouse::SqlDB::query<_sqlCountTotalStemsInJamUnpopulated>( jamCID.value() );
            countStemsRow( totalSpecificStems );
        }

        return totalSpecificStems;
    }

    // -----------------------------------------------------------------------------------------------------------------
    // find a single empty stem in a jam; from there we can maybe find many more to batch up
    static bool findUnpopulated( types::JamCouchID& jamCID, types::StemCouchID& stemCID )
    {
        static constexpr char findEmptyStem[] = R"(
            select OwnerJamCID, StemCID from stems where CreationTime is null limit 1 )";

        std::string _jamCID, _stemCID;

        auto query = Warehouse::SqlDB::query<findEmptyStem>();
        query( _jamCID, _stemCID );

        new (&jamCID) types::JamCouchID{ _jamCID };
        new (&stemCID) types::JamCouchID{ _stemCID };

        return !jamCID.empty();
    }

    // -----------------------------------------------------------------------------------------------------------------
    // find a single stem that needs filling
    static bool findUnpopulatedBatch( const types::JamCouchID& jamCID, const int32_t maximumStemsToFind, std::vector<types::StemCouchID>& stemCIDs )
    {
        static constexpr char findEmptyStems[] = R"(
            select StemCID from stems where OwnerJamCID is ?1 and CreationTime is null limit ?2 )";

        auto query = Warehouse::SqlDB::query<findEmptyStems>( jamCID.value(), maximumStemsToFind );

        stemCIDs.clear();
        stemCIDs.reserve( maximumStemsToFind );

        std::string_view stemCID;
        while ( query( stemCID ) )
        {
            stemCIDs.emplace_back( stemCID );
        }

        return !stemCIDs.empty();
    }

    // -----------------------------------------------------------------------------------------------------------------
    static bool getSingleStemByID( const types::StemCouchID& stemCID, endlesss::types::Stem& outStem )
    {
        auto query = Warehouse::SqlDB::query<unpackSingleStem>( stemCID.value() );

        std::string_view riffID, jamID;
        int32_t instrumentFlags;

        if ( !query( riffID,
                     jamID,
                     outStem.creationTimeUnix,
                     outStem.fileEndpoint,
                     outStem.fileBucket,
                     outStem.fileKey,
                     outStem.fileMIME,
                     outStem.fileLengthBytes,
                     outStem.BPS,
                     outStem.BPMrnd,
                     instrumentFlags,
                     outStem.length16s,
                     outStem.originalPitch,
                     outStem.barLength,
                     outStem.preset,
                     outStem.user,
                     outStem.sampleRate,
                     outStem.colour ) )
            return false;

        outStem.couchID = types::StemCouchID{ riffID };
        outStem.jamCouchID = types::JamCouchID{ jamID };

        outStem.isDrum = (instrumentFlags & (1 << 1)) == (1 << 1);
        outStem.isNote = (instrumentFlags & (1 << 2)) == (1 << 2);
        outStem.isBass = (instrumentFlags & (1 << 3)) == (1 << 3);
        outStem.isMic  = (instrumentFlags & (1 << 4)) == (1 << 4);

        return true;
    }

} // namespace stems

// ---------------------------------------------------------------------------------------------------------------------
namespace ledger {


    static constexpr char createStemTable[] = R"(
        CREATE TABLE IF NOT EXISTS "StemLedger" (
            "StemCID"       TEXT NOT NULL UNIQUE,
            "Type"          INTEGER,
            "Note"          TEXT NOT NULL,
            PRIMARY KEY("StemCID")
        );)";
    static constexpr char createStemIndex_0[] = R"(
        CREATE UNIQUE INDEX IF NOT EXISTS "Ledger_IndexStem"       ON "StemLedger" ( "StemCID" );)";

    static constexpr char deprecated_0[] = { DEPRECATE_INDEX "IndexStem" };

    // -----------------------------------------------------------------------------------------------------------------
    static void runInit()
    {
        Warehouse::SqlDB::query<createStemTable>();

        Warehouse::SqlDB::query<createStemIndex_0>();

        // deprecate
        Warehouse::SqlDB::query<deprecated_0>();
    }

    // -----------------------------------------------------------------------------------------------------------------
    static void storeStemNote( const types::StemCouchID& stemCID, const Warehouse::StemLedgerType& type, const std::string& note )
    {
        static constexpr char _sqlAddLedger[] = R"(
            INSERT OR IGNORE INTO StemLedger( StemCID, Type, Note ) VALUES( ?1, ?2, ?3 );
        )";

        Warehouse::SqlDB::query<_sqlAddLedger>( stemCID.value(), (int32_t)type, note );
    }

    // -----------------------------------------------------------------------------------------------------------------
    static bool getStemNoteType( const types::StemCouchID& stemCID, Warehouse::StemLedgerType& typeResult )
    {
        static constexpr char _sqlGetLedger[] = R"(
            select Type, Note from StemLedger where StemCID = ?1
        )";

        auto query = Warehouse::SqlDB::query<_sqlGetLedger>( stemCID.value() );

        int32_t          noteType = 0;
        std::string_view noteText;

        if ( query( noteType, noteText ) )
        {
            ABSL_ASSERT( noteType > 0 && noteType < 4 ); // just check the range before we cast
            typeResult = static_cast<Warehouse::StemLedgerType>(noteType);

            return true;
        }
        return false;
    }

} // namespace ledger

namespace constants {
} // namespace constants

} // namespace sql

// ---------------------------------------------------------------------------------------------------------------------
// add a custom seeded RANDOM function to sqlite, allowing us to feed through specific random sequences
//
static void sqlite_SEEDED_RANDOM_dtor( void* p )
{
    math::RNG32* pRNG = static_cast<math::RNG32*>(p);
    delete pRNG;
}

static void sqlite_SEEDED_RANDOM( sqlite3_context* context, int argc, sqlite3_value** argv )
{
    if ( argc == 1 && sqlite3_value_type( argv[0] ) == SQLITE_INTEGER )
    {
        // fetch the current RNG state from db context
        math::RNG32* pRNG = static_cast<math::RNG32*>( sqlite3_get_auxdata( context, 0 ) );
        if ( !pRNG )
        {
            // .. first time through, create the RNG state, stash it
            const int32_t seed = sqlite3_value_int( argv[0] );
            pRNG = new math::RNG32( static_cast<uint32_t>(seed) );
            sqlite3_set_auxdata( context, 0, pRNG, sqlite_SEEDED_RANDOM_dtor );
        }

        // produce the next number in the sequence
        const int64_t result = pRNG->genInt32();

        sqlite3_result_int64( context, result );
    }
    else
    {
        sqlite3_result_error( context, "Invalid", 0 );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
Warehouse::Warehouse( const app::StoragePaths& storagePaths, api::NetConfiguration::Shared& networkConfig, base::EventBusClient eventBus )
    : m_networkConfiguration( networkConfig )
    , m_eventBusClient( eventBus )
    , m_workerThreadPaused( false )
{
    blog::database( FMTX("sqlite version {}"), SQLITE_VERSION );

    // create the lockless queues that manages the warehouse TODO list
    {
        m_taskSchedule = std::make_unique<TaskSchedule>();
        m_taskSchedule->signal();
        m_taskSchedulePriority = std::make_unique<TaskSchedule>();
    }

    m_databaseFile = ( storagePaths.cacheCommon / "warehouse.db3" ).string();
    SqlDB::post_connection_hook = []( sqlite3* db_handle )
    {
        blog::database( FMTX( "post_connection_hook( 0x{:x} )" ), (uint64_t)db_handle );

        // https://www.sqlite.org/pragma.html#pragma_temp_store
        sqlite3_exec( db_handle, "pragma temp_store = memory", nullptr, nullptr, nullptr );

        // add our RANDOM variant that takes a seed to allow for deterministic random queries
        int32_t seededRes = sqlite3_create_function( db_handle, "SEEDED_RANDOM", 1, SQLITE_UTF8, NULL, &sqlite_SEEDED_RANDOM, NULL, NULL );
        blog::database( FMTX( "sqlite3_create_function(SEEDED_RANDOM) = {} ({})" ), seededRes == SQLITE_OK ? "OK" : "Error", seededRes );
        
        // bolt in carray extension
        int32_t carrayRes = sqlite3_carray_init( db_handle, nullptr, nullptr );
        blog::database( FMTX( "sqlite3_carray_init = {} ({})" ), carrayRes == SQLITE_OK ? "OK" : "Error", carrayRes );
    };

    // set the database up; creating tables & indices if we're starting fresh
    {
        Warehouse::SqlDB::TransactionGuard txn;

        sql::jams::runInit();
        sql::riffs::runInit();
        sql::tags::runInit();
        sql::stems::runInit();
        sql::ledger::runInit();
    }


    // optimize on startup
    {
        spacetime::ScopedTimer stemTiming( "warehouse [optimize]" );
        static constexpr char sqlOptimize[] = R"(pragma optimize;)";
        SqlDB::query<sqlOptimize>();
    }

    m_workerThreadAlive = true;
    m_workerThread      = std::make_unique<std::thread>( &Warehouse::threadWorker, this );

    APP_EVENT_BIND_TO( RiffTagAction );

#if OURO_PLATFORM_WIN
    ::SetThreadPriority( m_workerThread->native_handle(), THREAD_PRIORITY_BELOW_NORMAL );
#endif
}

// ---------------------------------------------------------------------------------------------------------------------
Warehouse::~Warehouse()
{
    APP_EVENT_UNBIND( RiffTagAction );

    m_workerThreadAlive = false;

    // unblock the thread, wait for it to die out
    m_taskSchedule->signal();
    m_workerThread->join();
    m_workerThread.reset();
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::setCallbackWorkReport( const WorkUpdateCallback& cb )
{
    {
        std::scoped_lock<std::mutex> cbLock( m_cbMutex );
        m_cbWorkUpdateToInstall = cb;
    }
    m_taskSchedule->signal();
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::setCallbackContentsReport( const ContentsReportCallback& cb )
{
    {
        std::scoped_lock<std::mutex> cbLock( m_cbMutex );
        m_cbContentsReportToInstall = cb;
    }
    m_taskSchedule->signal();
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::setCallbackTagUpdate( const TagUpdateCallback& cbUpdate, const TagBatchingCallback& cbBatch )
{
    {
        std::scoped_lock<std::mutex> cbLock( m_cbMutex );
        m_cbTagUpdate = cbUpdate;
        m_cbTagBatching = cbBatch;
    }
    m_taskSchedule->signal();
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::setCallbackTagRemoved( const TagRemovedCallback& cb )
{
    {
        std::scoped_lock<std::mutex> cbLock( m_cbMutex );
        m_cbTagRemoved = cb;
    }
    m_taskSchedule->signal();
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::clearAllCallbacks()
{
    {
        std::scoped_lock<std::mutex> cbLock( m_cbMutex );
        m_cbWorkUpdateToInstall = nullptr;
        m_cbContentsReportToInstall = nullptr;
        m_cbTagUpdate = nullptr;
        m_cbTagBatching = nullptr;
        m_cbTagRemoved = nullptr;
    }
    m_taskSchedule->signal();
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::requestContentsReport()
{
    m_taskSchedulePriority->enqueueWorkTask<ContentsReportTask>( m_cbContentsReport );

    // we only track the semaphore in the main queue, kick it to unblock the scheduler loop
    m_taskSchedule->enqueueWorkTask<EmptyTask>();
    m_taskSchedule->signal();
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::upsertSingleJamIDToName( const endlesss::types::JamCouchID& jamCID, const std::string& displayName )
{
    static constexpr char _insertOrUpdateJamData[] = R"(
        INSERT INTO jams( JamCID, PublicName ) VALUES( ?1, ?2 )
        ON CONFLICT(JamCID) DO UPDATE SET PublicName = ?2;
    )";

    Warehouse::SqlDB::query<_insertOrUpdateJamData>( jamCID.value(), displayName );
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::upsertJamDictionaryFromCache( const cache::Jams& jamCache )
{
    Warehouse::SqlDB::TransactionGuard txn;

    jamCache.iterateAllJams( [this]( const cache::Jams::Data& jamData )
        {
            upsertSingleJamIDToName( jamData.m_jamCID, jamData.m_displayName );
        });
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::upsertJamDictionaryFromBNS( const config::endlesss::BandNameService& bnsData )
{
    Warehouse::SqlDB::TransactionGuard txn;

    std::string resultJamName;

    for ( const auto& bnsPair : bnsData.entries )
    {
        if ( bnsPair.second.sync_name.empty() )
            resultJamName = bnsPair.second.link_name;
        else
            resultJamName = bnsPair.second.sync_name;

        upsertSingleJamIDToName( bnsPair.first, resultJamName );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::extractJamDictionary( types::JamIDToNameMap& jamDictionary ) const
{
    static constexpr char _extractJamData[] = R"(
                select JamCID, PublicName from jams;
            )";

    auto query = Warehouse::SqlDB::query<_extractJamData>();

    jamDictionary.clear();

    std::string_view jamCID;
    std::string_view publicName;

    while ( query( jamCID,
                   publicName ) )
    {
        jamDictionary.emplace( jamCID, publicName );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
base::OperationID Warehouse::addOrUpdateJamSnapshot( const types::JamCouchID& jamCouchID )
{
    const auto operationID = base::Operations::newID( OV_AddOrUpdateJamSnapshot );

    if ( jamCouchID.empty() )
    {
        blog::error::database( "cannot add empty jam ID to warehouse" );

        return base::OperationID::invalid();
    }
    if ( !hasFullEndlesssNetworkAccess() )
    {
        blog::error::database( "cannot call Warehouse::addOrUpdateJamSnapshot() with no active Endlesss network" );

        return base::OperationID::invalid();
    }

    m_taskSchedule->enqueueWorkTask<JamSnapshotTask>( *m_networkConfiguration, m_eventBusClient, jamCouchID, operationID, m_riffIDConflictHandling );

    return operationID;
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::addJamSliceRequest( const types::JamCouchID& jamCouchID, const JamSliceCallback& callbackOnCompletion )
{
    if ( jamCouchID.empty() )
    {
        blog::error::database( "empty Jam ID passed to warehouse for slice request" );
        return;
    }

    m_taskSchedule->enqueueWorkTask<JamSliceTask>( jamCouchID, callbackOnCompletion );
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::requestJamPurge( const types::JamCouchID& jamCouchID )
{
    if ( jamCouchID.empty() )
    {
        blog::error::database( "empty Jam ID passed to warehouse for purge" );
        return;
    }

    m_taskSchedule->enqueueWorkTask<JamPurgeTask>( jamCouchID );
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::requestJamSyncAbort( const types::JamCouchID& jamCouchID )
{
    if ( jamCouchID.empty() )
    {
        blog::error::database( "empty Jam ID passed to warehouse for sync abort" );
        return;
    }

    m_taskSchedule->enqueueWorkTask<JamSyncAbortTask>( jamCouchID );
}

// ---------------------------------------------------------------------------------------------------------------------
base::OperationID Warehouse::requestJamDataExport( const types::JamCouchID& jamCouchID, const fs::path exportFolder, std::string_view jamTitle )
{
    const auto operationID = base::Operations::newID( OV_ExportAction );

    if ( jamCouchID.empty() )
    {
        blog::error::database( "empty Jam ID passed to warehouse for export" );
        return base::OperationID::invalid();
    }

    m_taskSchedule->enqueueWorkTask<JamExportTask>( m_eventBusClient, jamCouchID, exportFolder, jamTitle, operationID );

    return operationID;
}

// ---------------------------------------------------------------------------------------------------------------------
base::OperationID Warehouse::requestJamDataImport( const fs::path pathToData )
{
    const auto operationID = base::Operations::newID( OV_ImportAction );

    m_taskSchedule->enqueueWorkTask<JamImportTask>( m_eventBusClient, pathToData, operationID );

    return operationID;
}

// ---------------------------------------------------------------------------------------------------------------------
bool Warehouse::fetchSingleRiffByID( const endlesss::types::RiffCouchID& riffID, endlesss::types::RiffComplete& result ) const
{
    if ( !sql::riffs::getSingleByID( riffID, result.riff ) )
        return false;

    result.jam.couchID = result.riff.jamCouchID;

    if ( sql::jams::getPublicNameForID( result.jam.couchID, result.jam.displayName ) == false )
    {
        // on failure, check if the couch ID does NOT start with "band..." indicating this is probably a personal jam
        // (at time of writing there's no other variants we support at this level)
        // .. and if it is, copy the couch ID as the display name
        if ( result.jam.couchID.value().rfind( "band", 0 ) != 0 )
        {
            result.jam.displayName = result.jam.couchID.value();
        }
        else
        {
            blog::error::database( FMTX( "unable to resolve display name for jam [{}], may cause export issues" ), result.jam.couchID );
            // #TODO maybe assign it something like "unknown"?
        }
    }

    for ( size_t stemI = 0; stemI < 8; stemI++ )
    {
        if ( result.riff.stemsOn[stemI] )
        {
            if ( !sql::stems::getSingleStemByID( result.riff.stems[stemI], result.stems[stemI] ) )
                return false;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
bool Warehouse::patchRiffStemRecord(
    const types::JamCouchID& jamCouchID,
    const endlesss::types::RiffCouchID& riffID,
    const int32_t stemIndex,
    const endlesss::types::StemCouchID& newStemID )
{
    // welcome to goofy town
    static constexpr char updateRiffStem1[] = R"( UPDATE riffs SET StemCID_1=?2 WHERE riffCID=?1 )";
    static constexpr char updateRiffStem2[] = R"( UPDATE riffs SET StemCID_2=?2 WHERE riffCID=?1 )";
    static constexpr char updateRiffStem3[] = R"( UPDATE riffs SET StemCID_3=?2 WHERE riffCID=?1 )";
    static constexpr char updateRiffStem4[] = R"( UPDATE riffs SET StemCID_4=?2 WHERE riffCID=?1 )";
    static constexpr char updateRiffStem5[] = R"( UPDATE riffs SET StemCID_5=?2 WHERE riffCID=?1 )";
    static constexpr char updateRiffStem6[] = R"( UPDATE riffs SET StemCID_6=?2 WHERE riffCID=?1 )";
    static constexpr char updateRiffStem7[] = R"( UPDATE riffs SET StemCID_7=?2 WHERE riffCID=?1 )";
    static constexpr char updateRiffStem8[] = R"( UPDATE riffs SET StemCID_8=?2 WHERE riffCID=?1 )";

    switch ( stemIndex )
    {
        case 0: Warehouse::SqlDB::query<updateRiffStem1>( riffID.value(), newStemID.value() ); break;
        case 1: Warehouse::SqlDB::query<updateRiffStem2>( riffID.value(), newStemID.value() ); break;
        case 2: Warehouse::SqlDB::query<updateRiffStem3>( riffID.value(), newStemID.value() ); break;
        case 3: Warehouse::SqlDB::query<updateRiffStem4>( riffID.value(), newStemID.value() ); break;
        case 4: Warehouse::SqlDB::query<updateRiffStem5>( riffID.value(), newStemID.value() ); break;
        case 5: Warehouse::SqlDB::query<updateRiffStem6>( riffID.value(), newStemID.value() ); break;
        case 6: Warehouse::SqlDB::query<updateRiffStem7>( riffID.value(), newStemID.value() ); break;
        case 7: Warehouse::SqlDB::query<updateRiffStem8>( riffID.value(), newStemID.value() ); break;

        default:
            blog::error::database( FMTX( "patchRiffStemRecord on index {} is invalid" ), stemIndex );
            return false;
    }

    // add new stem entry to get filled in (assuming it doesn't exist already)
    static constexpr char insertOrIgnoreNewStemSkeleton[] = R"(
        INSERT OR IGNORE INTO stems( stemCID, OwnerJamCID ) VALUES( ?1, ?2 );
    )";

    Warehouse::SqlDB::query<insertOrIgnoreNewStemSkeleton>( newStemID.value(), jamCouchID.value() );
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
std::size_t Warehouse::filterRiffsByBPM( const endlesss::constants::RootScalePairs& keySearchPairs, const BPMCountSort sortOn, std::vector< BPMCountTuple >& bpmCounts ) const
{
    Warehouse::SqlDB::TransactionGuard txn;

    bpmCounts.clear();

    // basic no rules mode where we don't match on any key at all, just group by BPM
    if ( keySearchPairs.searchMode == endlesss::constants::HarmonicSearch::NoRules )
    {
        static constexpr char _bpmGroupsByScaleAndRoot_ByRiffCount_NoRules[] = R"(
            select 
              round(BPMrnd) as BPM,
              count(1) as RiffCount,
              count(distinct OwnerJamCID) as UniqueJamCount
            from 
              riffs 
            group by 
              BPM 
            order by 
              RiffCount desc;
        )";
        static constexpr char _bpmGroupsByScaleAndRoot_ByBPM_NoRules[] = R"(
            select 
              round(BPMrnd) as BPM,
              count(1) as RiffCount,
              count(distinct OwnerJamCID) as UniqueJamCount
            from 
              riffs 
            group by 
              BPM 
            order by 
              BPM desc;
        )";

        {
            float bpmRange;
            uint32_t bpmCount;
            uint32_t jamCount;

            if ( sortOn == BPMCountSort::ByBPM )
            {
                auto query = Warehouse::SqlDB::query<_bpmGroupsByScaleAndRoot_ByBPM_NoRules>();
                while ( query( bpmRange, bpmCount, jamCount ) )
                {
                    bpmCounts.emplace_back( BPMCountTuple{ (uint32_t)std::round( bpmRange ), bpmCount, jamCount } );
                }
            }
            else
            {
                auto query = Warehouse::SqlDB::query<_bpmGroupsByScaleAndRoot_ByRiffCount_NoRules>();
                while ( query( bpmRange, bpmCount, jamCount ) )
                {
                    bpmCounts.emplace_back( BPMCountTuple{ (uint32_t)std::round( bpmRange ), bpmCount, jamCount } );
                }
            }
        }

    }
    else
    {
        static constexpr char _bpmGroupsByScaleAndRoot_ByRiffCount[] = R"(
            select 
              round(BPMrnd) as BPM,
              ( ( root << 8 ) | scale ) as rootScale,
              count(1) as RiffCount,
              count(distinct OwnerJamCID) as UniqueJamCount
            from 
              riffs 
            where 
              rootScale in carray( ?1, ?2 )
            group by 
              BPM 
            order by 
              RiffCount desc;
        )";
        static constexpr char _bpmGroupsByScaleAndRoot_ByBPM[] = R"(
            select 
              round(BPMrnd) as BPM,
              ( ( root << 8 ) | scale ) as rootScale,
              count(1) as RiffCount,
              count(distinct OwnerJamCID) as UniqueJamCount
            from 
              riffs 
            where 
              rootScale in carray( ?1, ?2 )
            group by 
              BPM 
            order by 
              BPM desc;
        )";

        absl::InlinedVector< int32_t, 16 > rootScaleHashList;

        // create the merged root/scale values to pass in as a search list, matching how they are encoded in the SQL : ((root << 8) | scale)
        for ( const auto rspair : keySearchPairs.pairs )
        {
            const int32_t rshash = (rspair.root << 8) | rspair.scale;
            rootScaleHashList.emplace_back( rshash );
        }

        const int32_t* rootScalePtr = rootScaleHashList.data();
        const int32_t rootScaleCount = static_cast<int32_t>(rootScaleHashList.size());

        {
            float bpmRange;
            int32_t _hash;
            uint32_t bpmCount;
            uint32_t jamCount;

            if ( sortOn == BPMCountSort::ByBPM )
            {
                auto query = Warehouse::SqlDB::query<_bpmGroupsByScaleAndRoot_ByBPM>( rootScalePtr, rootScaleCount );
                while ( query( bpmRange, _hash, bpmCount, jamCount ) )
                {
                    bpmCounts.emplace_back( BPMCountTuple{ (uint32_t)std::round( bpmRange ), bpmCount, jamCount } );
                }
            }
            else
            {
                auto query = Warehouse::SqlDB::query<_bpmGroupsByScaleAndRoot_ByRiffCount>( rootScalePtr, rootScaleCount );
                while ( query( bpmRange, _hash, bpmCount, jamCount ) )
                {
                    bpmCounts.emplace_back( BPMCountTuple{ (uint32_t)std::round( bpmRange ), bpmCount, jamCount } );
                }
            }
        }
    }

    return bpmCounts.size();
}

// ---------------------------------------------------------------------------------------------------------------------
bool Warehouse::fetchRandomRiffBySeed( const endlesss::constants::RootScalePairs& keySearchPairs, const uint32_t BPM, const int32_t seedValue, endlesss::types::RiffComplete& result ) const
{
    Warehouse::SqlDB::TransactionGuard txn;

    static constexpr char _randomFilteredRiff[] = R"(
            select 
              round(BPMrnd) as BPM,
              ( ( root << 8 ) | scale ) as rootScale,
              RiffCID 
            from 
              riffs 
            where 
              rootScale = ?1
              and BPM = ?2
              and (OwnerJamCID is not ?3) 
            order by 
              SEEDED_RANDOM(?4) 
            limit 
              1
        )";
    static constexpr char _randomFilteredRiff_NoRules[] = R"(
            select 
              round(BPMrnd) as BPM,
              RiffCID 
            from 
              riffs 
            where 
              BPM = ?1
              and (OwnerJamCID is not ?2) 
            order by 
              SEEDED_RANDOM(?3) 
            limit 
              1
        )";


    if ( keySearchPairs.searchMode == endlesss::constants::HarmonicSearch::NoRules )
    {
        auto query = Warehouse::SqlDB::query<_randomFilteredRiff_NoRules>( BPM, cVirtualJamName.data(), seedValue );

        float bpmRange;
        std::string_view riffCID;

        if ( query( bpmRange, riffCID ) )
        {
            return fetchSingleRiffByID( endlesss::types::RiffCouchID{ riffCID }, result );
        }
    }
    else
    {
        math::RNG32 rsRng( seedValue );

        // pick a random key/root pair, create the hash value to pass in
        const int32_t chosenPairIndex = rsRng.genInt32( 0, static_cast<int32_t>(keySearchPairs.pairs.size() - 1) );
        const auto rspair = keySearchPairs.pairs[chosenPairIndex];
        
        const int32_t rshash = (rspair.root << 8) | rspair.scale;

#if 0
        for ( const auto& paired : keySearchPairs.pairs )
        {
            blog::database( FMTX( "fetchRandomRiffBySeed | {} {}" ),
                endlesss::constants::cRootNames[paired.root],
                endlesss::constants::cScaleNames[paired.scale] );
        }
        blog::database( FMTX( "                      | {} | {} {}" ),
            rshash,
            endlesss::constants::cRootNames[rspair.root],
            endlesss::constants::cScaleNames[rspair.scale] );
#endif

        {

            auto query = Warehouse::SqlDB::query<_randomFilteredRiff>( rshash, BPM, cVirtualJamName.data(), seedValue );

            float bpmRange;
            int32_t _hash;
            std::string_view riffCID;

            if ( query( bpmRange, _hash, riffCID ) )
            {
                return fetchSingleRiffByID( endlesss::types::RiffCouchID{ riffCID }, result );
            }
        }
    }

    return false;
}

// ---------------------------------------------------------------------------------------------------------------------
bool Warehouse::batchFindJamIDForStem( const endlesss::types::StemCouchIDs& stems, endlesss::types::JamCouchIDs& result ) const
{
    static constexpr char _ownerJamForStemID[] = R"(
            select 
              OwnerJamCID 
            from 
              stems 
            where 
              stemCID = ?1;
        )";

    endlesss::types::JamCouchID emptyResult;

    Warehouse::SqlDB::TransactionGuard txn;
    for ( const auto& stemID : stems )
    {
        auto query = Warehouse::SqlDB::query<_ownerJamForStemID>( stemID.value() );

        std::string_view jamCID;
        if ( query( jamCID ) )
        {
            result.emplace_back( jamCID );
        }
        else
        {
            result.emplace_back( emptyResult );
        }
    }

    return !result.empty();
}

// ---------------------------------------------------------------------------------------------------------------------
bool Warehouse::fetchAllStemsForJam( const types::JamCouchID& jamCouchID, endlesss::types::StemCouchIDs& result, std::size_t& estimatedTotalFileSize ) const
{
    // try and get a count so we can prime the output
    int64_t stemCount = 0;
    {
        static constexpr char _countStemsInJam[] = R"(
            select count(*) from stems where OwnerJamCID = ?1;
        )";

        auto query = Warehouse::SqlDB::query<_countStemsInJam>( jamCouchID.value() );
        if ( !query( stemCount ) )
        {
            blog::database( FMTX( "unable to get stem count from jam [{}]" ), jamCouchID );
        }
    }
    
    // reset and prepare if we can
    result.clear();
    if ( stemCount > 0 )
        result.reserve( stemCount );

    estimatedTotalFileSize = 0;

    // pull all stem IDs out
    {
        static constexpr char _allStemsInJam[] = R"(
            select StemCID, FileLength from stems where OwnerJamCID = ?1 order by CreationTime asc;
        )";

        auto query = Warehouse::SqlDB::query<_allStemsInJam>( jamCouchID.value() );

        std::string_view stemCID;
        uint64_t stemFileLengthBytes = 0;

        while ( query( stemCID, stemFileLengthBytes ) )
        {
            result.emplace_back( endlesss::types::StemCouchID( stemCID ) );
            estimatedTotalFileSize += stemFileLengthBytes;
        }
    }

    return !result.empty();
}

// ---------------------------------------------------------------------------------------------------------------------
bool Warehouse::fetchSingleStemByID( const types::StemCouchID& stemCouchID, endlesss::types::Stem& result ) const
{
    return sql::stems::getSingleStemByID( stemCouchID, result );
}

// ---------------------------------------------------------------------------------------------------------------------
// this isn't really for normal use. potentially this could return 100k+ stem IDs on a decently populated warehouse
//
bool Warehouse::fetchAllStems( endlesss::types::StemCouchIDs& result, std::size_t& estimatedTotalFileSize ) const
{
    // try and get a count so we can prime the output
    int64_t stemCount = 0;
    {
        static constexpr char _countAllStems[] = R"(
            select count(*) from stems;
        )";

        auto query = Warehouse::SqlDB::query<_countAllStems>();
        if ( !query( stemCount ) )
        {
            blog::database( FMTX( "unable to get stem count for entire database" ) );
        }
    }

    // reset and prepare if we can
    result.clear();
    if ( stemCount > 0 )
        result.reserve( stemCount );

    estimatedTotalFileSize = 0;

    // pull all stem IDs out
    {
        static constexpr char _allStems[] = R"(
            select StemCID, FileLength from stems;
        )";

        auto query = Warehouse::SqlDB::query<_allStems>();

        std::string_view stemCID;
        uint64_t stemFileLengthBytes = 0;

        while ( query( stemCID, stemFileLengthBytes ) )
        {
            result.emplace_back( endlesss::types::StemCouchID( stemCID ) );
            estimatedTotalFileSize += stemFileLengthBytes;
        }
    }

    return !result.empty();
}

// ---------------------------------------------------------------------------------------------------------------------
bool Warehouse::getNoteTypeForStem( const types::StemCouchID& stemCID, StemLedgerType& typeResult )
{
    return sql::ledger::getStemNoteType( stemCID, typeResult );
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::upsertTag( const endlesss::types::RiffTag& tag )
{
    sql::tags::upsert( tag, m_cbTagUpdate );
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::removeTag( const endlesss::types::RiffTag& tag )
{
    sql::tags::remove( tag, m_cbTagRemoved );
}

// ---------------------------------------------------------------------------------------------------------------------
bool Warehouse::isRiffTagged( const endlesss::types::RiffCouchID& riffID, endlesss::types::RiffTag* tagOutput /*= nullptr */ ) const
{
    return sql::tags::isRiffTagged( riffID, tagOutput );
}

// ---------------------------------------------------------------------------------------------------------------------
std::size_t Warehouse::fetchTagsForJam( const endlesss::types::JamCouchID& jamCID, std::vector<endlesss::types::RiffTag>& outputTags ) const
{
    return sql::tags::forJam( jamCID, outputTags );
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::batchUpdateTags( const std::vector<endlesss::types::RiffTag>& inputTags )
{
    blog::database( FMTX( "tags : batch updating {} items" ), inputTags.size() );
    m_cbTagBatching( true );
    sql::tags::batchUpdate( inputTags, m_cbTagUpdate );
    m_cbTagBatching( false );
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::batchRemoveAllTags( const endlesss::types::JamCouchID& jamCID )
{
    blog::database( FMTX( "tags : removing all tags for {}" ), jamCID );
    m_cbTagBatching( true );
    sql::tags::batchRemoveAll( jamCID );
    m_cbTagBatching( false );
}

// ---------------------------------------------------------------------------------------------------------------------
bool Warehouse::isRiffIDVirtual( const endlesss::types::RiffCouchID& riffID )
{
    if ( riffID.size() > 2 &&
         riffID.c_str()[0] == 'V' &&
         riffID.c_str()[1] == 'R' )
    {
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------
endlesss::types::RiffIdentity Warehouse::createNewVirtualRiff( const endlesss::types::VirtualRiff& vriff )
{
    Warehouse::SqlDB::TransactionGuard txn;

    // create a new UUID for our virtual riff
    std::string baseRiffCID = data::generateUUID_V1( false );

    // although these are technically hex strings, we never deal with them as numeric in the app;
    // therefore sticking some very unhex characters in there as distinctions is fine as these IDs are entirely ouro-centric
    baseRiffCID[0] = 'V';
    baseRiffCID[1] = 'R';
    ABSL_ASSERT( isRiffIDVirtual( endlesss::types::RiffCouchID{ baseRiffCID } ) );  // test the isRiffIDVirtual() fn flags this correctly

    const std::chrono::seconds timestampUnix = spacetime::getUnixTimeNow();


    static constexpr char injectFullRiff[] = R"(
            INSERT OR IGNORE INTO riffs(
                riffCID,
                OwnerJamCID,
                CreationTime,
                Root,
                Scale,
                BPS,
                BPMrnd,
                BarLength,
                AppVersion,
                Magnitude,
                UserName,
                StemCID_1,
                StemCID_2,
                StemCID_3,
                StemCID_4,
                StemCID_5,
                StemCID_6,
                StemCID_7,
                StemCID_8,
                GainsJSON
                ) VALUES( ?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18, ?19, ?20 );
        )";

    const auto gainsJsonText = fmt::format( R"([ {} ])", fmt::join( vriff.gains, ", " ) );

    Warehouse::SqlDB::query<injectFullRiff>(
        baseRiffCID.c_str(),
        cVirtualJamName.data(),
        static_cast<uint64_t>(timestampUnix.count()),
        vriff.root,
        vriff.scale,
        vriff.BPMrnd / 60.0f,   // bps
        vriff.BPMrnd,
        vriff.barLength,
        9999,   // app v
        1.0f,   // magnitude; not really used anywhere
        vriff.user.c_str(),
        vriff.stemsOn[0] ? vriff.stems[0].c_str() : "",
        vriff.stemsOn[1] ? vriff.stems[1].c_str() : "",
        vriff.stemsOn[2] ? vriff.stems[2].c_str() : "",
        vriff.stemsOn[3] ? vriff.stems[3].c_str() : "",
        vriff.stemsOn[4] ? vriff.stems[4].c_str() : "",
        vriff.stemsOn[5] ? vriff.stems[5].c_str() : "",
        vriff.stemsOn[6] ? vriff.stems[6].c_str() : "",
        vriff.stemsOn[7] ? vriff.stems[7].c_str() : "",
        gainsJsonText.c_str()
    );

    return { 
        endlesss::types::JamCouchID{ cVirtualJamName },
        endlesss::types::RiffCouchID{ baseRiffCID }
    };
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::clearOutVirtualJamStorage()
{
    static constexpr char deleteRiffs[] = R"(
        delete from riffs where OwnerJamCID = ?1;
    )";

    {
        Warehouse::SqlDB::TransactionGuard txn;
        Warehouse::SqlDB::query<deleteRiffs>( cVirtualJamName.data() );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::workerTogglePause()
{
    m_workerThreadPaused = !m_workerThreadPaused;

    if ( m_workerThreadPaused && m_cbWorkUpdate )
        m_cbWorkUpdate( false, "Work paused" );
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::threadWorker()
{
    OuroveonThreadScope ots( OURO_THREAD_PREFIX "Warehouse::Work" );

    // mechanism for occasionally getting reports enqueued as work rolls on
    int32_t workCyclesBeforeNewReport = 0;
    const auto tryEnqueueReport = [this, &workCyclesBeforeNewReport]( bool force )
    {
        workCyclesBeforeNewReport--;
        if ( workCyclesBeforeNewReport <= 0 || force )
        {
            requestContentsReport();
            workCyclesBeforeNewReport = 3;
        }
    };

    // lock access to the to-install callbacks and move them live if we have new ones to play with
    const auto checkLockAndInstallNewCallbacks = [this, &tryEnqueueReport]()
    {
        std::scoped_lock<std::mutex> cbLock( m_cbMutex );
        if ( m_cbContentsReportToInstall != nullptr )
        {
            m_cbContentsReport = m_cbContentsReportToInstall;
            m_cbContentsReportToInstall = nullptr;

            tryEnqueueReport( true );
        }
        if ( m_cbWorkUpdateToInstall != nullptr )
        {
            m_cbWorkUpdate = m_cbWorkUpdateToInstall;
            m_cbWorkUpdateToInstall = nullptr;
        }
    };
    checkLockAndInstallNewCallbacks();


    if ( m_workerThreadPaused && m_cbWorkUpdate )
        m_cbWorkUpdate( false, "Work paused" );

    bool scrapingIsRunning = false;

    while ( m_workerThreadAlive )
    {
        base::instr::ScopedEvent wte( "WORKER", base::instr::PresetColour::Pink );

        // let the thread idle for N seconds until we take a cursory glance at anything
        // all operations that enqueue to the task queue or futz with callbacks will signal() this sema to 
        // instantly unblock this wait
        static constexpr auto cWorkerThreadSpinTime = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::seconds( 10 ) );
        m_taskSchedule->m_workerWaitSema.wait( cWorkerThreadSpinTime.count() );

        checkLockAndInstallNewCallbacks();

        // cycle round if paused
        if ( m_workerThreadPaused )
        {
            continue;
        }

        Task nextTask;
        
        // something to do? check the priority pile first in case we have stuff that needs running before
        // the rest of the queue (usually stuff like a contents report)
        bool bGotValidTask = m_taskSchedulePriority->m_taskQueue.try_dequeue( nextTask );
        if ( !bGotValidTask )
        {
            bGotValidTask = m_taskSchedule->m_taskQueue.try_dequeue( nextTask );
        }

        if ( bGotValidTask )
        {
            if ( m_cbWorkUpdate )
                m_cbWorkUpdate( true, nextTask->Describe() );

            const auto taskDescription = nextTask->Describe();
            blog::database( taskDescription );
            {
                base::instr::ScopedEvent se( "TASK", nextTask->getTag(), base::instr::PresetColour::Indigo );

                const bool taskOk = nextTask->Work( m_taskSchedule->m_taskQueue );
                if ( !taskOk )
                {
                    if ( m_cbWorkUpdate )
                        m_cbWorkUpdate( false, "Paused due to task error" );

                    m_eventBusClient.Send<::events::AddToastNotification>(
                        ::events::AddToastNotification::Type::Error,
                        "Warehouse Update Halted",
                        fmt::format( FMTX("Task [{}] failed"), nextTask->getTag() ) );

                    m_workerThreadPaused = true;
                    continue;
                }
            }

            {
                if ( nextTask->shouldTriggerContentReport() )
                    tryEnqueueReport( true );

                types::JamCouchID jamToIncrement;
                if ( nextTask->shouldIncrementJamChangeIndex( jamToIncrement ) )
                {
                    incrementChangeIndexForJam( jamToIncrement );
                }
            }
        }
        // go looking for holes to fill
        else
        {
            const bool hasEndlesssNetwork = hasFullEndlesssNetworkAccess();

            // fill in empty stems
            if ( hasEndlesssNetwork )
            {
                base::instr::ScopedEvent se( "FILL", "Stems", base::instr::PresetColour::Orange );

                types::JamCouchID owningJamCID;
                types::StemCouchID emptyStemCID;
                if ( sql::stems::findUnpopulated( owningJamCID, emptyStemCID ) )
                {
                    if ( m_cbWorkUpdate )
                        m_cbWorkUpdate( true, "Finding unpopulated stems..." );

                    // how about some stems?
                    std::vector<types::StemCouchID> emptyStems;
                    if ( sql::stems::findUnpopulatedBatch( owningJamCID, 40, emptyStems ) )
                    {
                        incrementChangeIndexForJam( owningJamCID );

                        // stem me up
                        m_taskSchedule->enqueueWorkTask<GetStemData>( *m_networkConfiguration, owningJamCID, emptyStems );

                        tryEnqueueReport( false );

                        scrapingIsRunning = true;
                        continue;
                    }
                    else
                    {
                        blog::error::database( FMTX( "stems::findUnpopulatedBatch() failed" ) );
                    }
                }
            }
            // scour for empty riffs
            if ( hasEndlesssNetwork )
            {
                base::instr::ScopedEvent se( "FILL", "Riffs", base::instr::PresetColour::Red );

                types::JamCouchID owningJamCID;
                types::RiffCouchID emptyRiffCID;
                if ( sql::riffs::findUnpopulated( owningJamCID, emptyRiffCID ) )
                {
                    if ( m_cbWorkUpdate )
                        m_cbWorkUpdate( true, "Finding unpopulated riffs..." );

                    // can we find some juicy new riffs?
                    std::vector<types::RiffCouchID> emptyRiffs;
                    if ( sql::riffs::findUnpopulatedBatch( owningJamCID, 40, emptyRiffs ) )
                    {
                        incrementChangeIndexForJam( owningJamCID );

                        // off to riff town
                        m_taskSchedule->enqueueWorkTask<GetRiffDataTask>( *m_networkConfiguration, owningJamCID, emptyRiffs );

                        tryEnqueueReport( false );

                        scrapingIsRunning = true;
                        continue;
                    }
                    else
                    {
                        const std::string errorReport = fmt::format( FMTX( "we found one empty riff ({}, in jam {}) but failed during batch?" ), emptyRiffCID, owningJamCID );
                        blog::error::database( FMTX("Riff Sync Error : {}"), errorReport );

                        m_eventBusClient.Send<::events::AddToastNotification>(
                            ::events::AddToastNotification::Type::Error,
                            "Warehouse Riff Sync Error",
                            errorReport );
                    }
                }
            }


            // if we were running scraping tasks and we just finished, kick off a final report generation
            if ( scrapingIsRunning )
            {
                scrapingIsRunning = false;
                tryEnqueueReport( true );
            }

            if ( m_cbWorkUpdate )
                m_cbWorkUpdate( false, "Database Idle" );
        }
    }

    if ( m_cbWorkUpdate )
        m_cbWorkUpdate( false, "" );
}

// ---------------------------------------------------------------------------------------------------------------------
Warehouse::ChangeIndex Warehouse::getChangeIndexForJam( const endlesss::types::JamCouchID& jamID ) const
{
    const auto cIt = m_changeIndexMap.find( jamID );
    if ( cIt == m_changeIndexMap.end() )
        return ChangeIndex::invalid();

    return cIt->second;
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::incrementChangeIndexForJam( const ::endlesss::types::JamCouchID& jamID )
{
    const auto cIt = m_changeIndexMap.find( jamID );
    if ( cIt == m_changeIndexMap.end() )
    {
        m_changeIndexMap.emplace( jamID, ChangeIndex::defaultValue() );
    }
    else
    {
        m_changeIndexMap[jamID] = ChangeIndex( cIt->second.get() + 1 );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::event_RiffTagAction( const events::RiffTagAction* eventData )
{
    switch ( eventData->m_action )
    {
        case events::RiffTagAction::Action::Upsert:
        {
            sql::tags::upsert( eventData->m_tag, m_cbTagUpdate );
        }
        break;

        case events::RiffTagAction::Action::Remove:
        {
            sql::tags::remove( eventData->m_tag, m_cbTagRemoved );
        }
        break;

        default:
            ABSL_ASSERT( false );
            break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
bool JamSnapshotTask::Work( TaskQueue& currentTasks )
{
    OperationCompleteOnScopeExit( m_operationID );

    blog::database( FMTX( "[{}] requesting full riff manifest" ), Tag );

    endlesss::api::JamFullSnapshot jamSnapshot;
    if ( !jamSnapshot.fetch( m_netConfig, m_jamCID ) )
    {
        blog::error::database( FMTX( "[{}] Failed to fetch snapshot for jam [{}]" ), Tag, m_jamCID );
        return false;
    }

    const auto countBeforeTx = sql::riffs::countRiffsInJam( m_jamCID );
    const bool bJamIsPersonal = m_jamCID.value().rfind( "band", 0 ) != 0;   // true if this jam is not a "band####" style ID, representing a user's own solo jam

    // the snapshot is only for getting the root couch IDs for all riffs in a jam; further details
    // are then plucked one-by-one as we fill the warehouse.
    
    // we have two modes of operation when it comes to dealing with riff values, either we ignore conflicts or
    // overwrite any existing Owner Jam value with the new one coming in. Check the RiffIDConflictHandling enum for
    // more details
    static constexpr char insertOrIgnore_RiffSkeleton[] = R"(
        INSERT OR IGNORE INTO riffs( riffCID, OwnerJamCID ) VALUES( ?1, ?2 );
    )";
    static constexpr char insertWithUpdate_RiffSkeleton[] = R"(
        INSERT INTO riffs( riffCID, OwnerJamCID, CreationTime ) VALUES( ?1, ?2, ?3 )
        ON CONFLICT( riffCID ) DO UPDATE SET OwnerJamCID = ?2, CreationTime = ?3
    )";

    {
        // bundle into single transaction
        Warehouse::SqlDB::TransactionGuard txn;

        // choose which conflict mode we're using for this
        bool bIgnoreConflicts = true;
        switch ( m_conflictHandling )
        {
            default:
                ABSL_ASSERT( 0 );
            case Warehouse::RiffIDConflictHandling::IgnoreAll:
                break;

            case Warehouse::RiffIDConflictHandling::Overwrite:
                bIgnoreConflicts = false;
                break;

            case Warehouse::RiffIDConflictHandling::OverwriteExceptPersonal:
                {
                    if ( !bJamIsPersonal )
                        bIgnoreConflicts = false;
                }
                break;
        }

        // run chosen queries against the fetched rows
        if ( bIgnoreConflicts )
        {
            blog::database( FMTX( "[{}] riff ID [{}] : conflicts will be ignored" ), Tag, m_jamCID );

            for ( const auto& jamData : jamSnapshot.rows )
            {
                const auto& riffCID = jamData.id;

                Warehouse::SqlDB::query<insertOrIgnore_RiffSkeleton>( riffCID.value(), m_jamCID.value() );
            }
        }
        else
        {
            blog::database( FMTX( "[{}] riff ID [{}] : conflicts will be overwritten" ), Tag, m_jamCID );

            for ( const auto& jamData : jamSnapshot.rows )
            {
                const auto& riffCID          = jamData.id;
                const auto  riffCreationTime = jamData.key;

                Warehouse::SqlDB::query<insertWithUpdate_RiffSkeleton>(
                    riffCID.value(),
                    m_jamCID.value(),
                    riffCreationTime / 1000 ); // from unix nano
            }
        }
    }

    const auto countAfterTx = sql::riffs::countRiffsInJam( m_jamCID );

    blog::database( FMTX( "[{}] {} online, added {} to Db" ), Tag, jamSnapshot.rows.size(), countAfterTx - countBeforeTx );
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
bool JamPurgeTask::Work( TaskQueue& currentTasks )
{
    // remove jam
    // delete from riffs where OwnerJamCID = "band#####""
    // delete from stems where OwnerJamCID = "band#####""
    // delete from jams where JamCID = "band#####"

    static constexpr char deleteJam[] = R"(
        delete from jams where JamCID = ?1;
    )";
    static constexpr char deleteRiffs[] = R"(
        delete from riffs where OwnerJamCID = ?1;
    )";
    static constexpr char deleteStems[] = R"(
        delete from stems where OwnerJamCID = ?1;
    )";

    {
        // bundle into single transaction
        Warehouse::SqlDB::TransactionGuard txn;
        Warehouse::SqlDB::query<deleteJam>( m_jamCID.value() );
        Warehouse::SqlDB::query<deleteRiffs>( m_jamCID.value() );
        Warehouse::SqlDB::query<deleteStems>( m_jamCID.value() );
    }

    blog::database( "[{}] wiped [{}] from Db", Tag, m_jamCID.value() );
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
bool JamSyncAbortTask::Work( TaskQueue& currentTasks )
{
    static constexpr char deleteEmptyRiffs[] = R"(
        delete from riffs where OwnerJamCID = ?1 and Riffs.AppVersion is null;
    )";

    Warehouse::SqlDB::query<deleteEmptyRiffs>( m_jamCID.value() );

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// ref https://randomascii.wordpress.com/2012/03/08/float-precisionfrom-zero-to-100-digits-2/
//     https://gcc.gnu.org/onlinedocs/gcc/Hex-Floats.html
//
bool JamExportTask::Work( TaskQueue& currentTasks )
{
    OperationCompleteOnScopeExit( m_operationID );

    const std::string exportFilenameYaml = Warehouse::createExportFilenameForJam( m_jamCID, m_jamName, "yaml" );

    const fs::path finalOutputFile = m_exportFolder / exportFilenameYaml;
    blog::database( FMTX( "Export process for [{}] to [{}]" ), m_jamName, finalOutputFile.string() );

    std::ofstream yamlOutput;
    {
        yamlOutput.open( finalOutputFile );
    }
    {
        yamlOutput << "export_time_unix: " << spacetime::getUnixTimeNow().count() << std::endl;
        yamlOutput << "export_ouroveon_version: \"" << OURO_FRAMEWORK_VERSION << "\"" << std::endl;
        yamlOutput << "jam_name: \"" << m_jamName << "\"" << std::endl;
        yamlOutput << "jam_couch_id: \"" << m_jamCID.value() << "\"" << std::endl;
    }
    {
        // get a list of all the riff IDs for this jam, we decode each one in turn
        static constexpr char getAllRiffIDs[] = R"( select RiffCID from riffs where OwnerJamCID = ?1 order by CreationTime asc; )";
        auto query = Warehouse::SqlDB::query<getAllRiffIDs>( m_jamCID.value() );

        std::string_view riffCID;

        std::string riffEntryLine;

        yamlOutput << "# riffs schema" << std::endl;
        yamlOutput << "# couch ID, user, creation unix time, root index, root name, scale index, scale name, BPS (float), BPS (hex float), BPM (float), BPM (hex float), bar length, 8x [ stem couch ID, stem gain (float), stem gain (hex float), stem enabled ], app version" << std::endl;
        yamlOutput << "riffs:" << std::endl;
        while ( query( riffCID ) )
        {
            // decode an ID into a riff data block
            const endlesss::types::RiffCouchID riffCouchID( riffCID );
            endlesss::types::Riff riffData;

            if ( sql::riffs::getSingleByID( riffCouchID, riffData ) )
            {
                riffEntryLine = fmt::format( FMTX( " \"{}\": [\"{}\", {}, {}, \"{}\", {}, \"{}\", {:.9g}, \"{:a}\", {:.9g}, \"{:a}\", {}, {}, " ),
                    riffData.couchID,
                    riffData.user,
                    riffData.creationTimeUnix,
                    riffData.root,
                    endlesss::constants::cRootNames[riffData.root],
                    riffData.scale,
                    endlesss::constants::cScaleNamesFilenameSanitize[riffData.scale],
                    riffData.BPS,
                    riffData.BPS,
                    riffData.BPMrnd,
                    riffData.BPMrnd,
                    riffData.barLength,
                    riffData.appVersion );

                for ( int32_t stemI = 0; stemI < 8; stemI++ )
                {
                    riffEntryLine += fmt::format( FMTX( "[\"{}\", {:.9g}, \"{:a}\", {}], " ),
                        riffData.stems[stemI],
                        riffData.gains[stemI],
                        riffData.gains[stemI],
                        riffData.stemsOn[stemI] );
                }

                yamlOutput << riffEntryLine << std::setprecision( 9 ) << riffData.magnitude << " ]" << std::endl;
            }
            else
            {
                m_eventBusClient.Send<::events::AddToastNotification>( ::events::AddToastNotification::Type::Error,
                    ICON_FA_BOX " Jam Export Error",
                    fmt::format( FMTX( "Unable to decode [R:{}] from database" ), riffCID ) );
            }
        }
    }
    {
        // similar process for the stems
        static constexpr char getAllStemsIDs[] = R"( select StemCID from stems where OwnerJamCID = ?1 order by CreationTime asc; )";
        auto query = Warehouse::SqlDB::query<getAllStemsIDs>( m_jamCID.value() );

        std::string_view stemCID;

        std::string stemEntryLine;

        yamlOutput << "# stems schema" << std::endl;
        yamlOutput << "# couch ID, file endpoint, file bucket, file key, file MIME, file length in bytes, sample rate, creation unix time, preset, user, colour hex, BPS (float), BPS (hex float), BPM (float), BPM (hex float), length 16ths, original pitch, bar length, is-drum, is-note, is-bass, is-mic" << std::endl;
        yamlOutput << "stems:" << std::endl;
        while ( query( stemCID ) )
        {
            // decode an ID into a stem data block
            const endlesss::types::StemCouchID stemCouchID( stemCID );
            endlesss::types::Stem stemData;

            if ( sql::stems::getSingleStemByID( stemCouchID, stemData ) )
            {
                stemEntryLine = fmt::format( FMTX( " \"{}\": [\"{}\", \"{}\", \"{}\", \"{}\", {}, {}, {}, \"{}\", \"{}\", \"{}\", {:.9g}, \"{:a}\", {:.9g}, \"{:a}\", {}, {}, {}, {}, {}, {}, {}]" ),
                    stemData.couchID,
                    stemData.fileEndpoint,
                    stemData.fileBucket,
                    stemData.fileKey,
                    stemData.fileMIME,
                    stemData.fileLengthBytes,
                    stemData.sampleRate,
                    stemData.creationTimeUnix,
                    stemData.preset,
                    stemData.user,
                    stemData.colour,
                    stemData.BPS,
                    stemData.BPS,
                    stemData.BPMrnd,
                    stemData.BPMrnd,
                    stemData.length16s,
                    stemData.originalPitch,
                    stemData.barLength,
                    stemData.isDrum,
                    stemData.isNote,
                    stemData.isBass,
                    stemData.isMic
                );

                yamlOutput << stemEntryLine << std::endl;
            }
            else
            {
                m_eventBusClient.Send<::events::AddToastNotification>( ::events::AddToastNotification::Type::Error,
                    ICON_FA_BOX " Jam Export Error",
                    fmt::format( FMTX( "Unable to decode [S:{}] from database" ), stemCID ) );
            }
        }
    }

    m_eventBusClient.Send<::events::AddToastNotification>( ::events::AddToastNotification::Type::Info,
        ICON_FA_BOX " Jam Export Success",
        fmt::format( FMTX( "Written to {}" ), exportFilenameYaml ) );

    return true;
}


// ---------------------------------------------------------------------------------------------------------------------
bool JamImportTask::Work( TaskQueue& currentTasks )
{
    OperationCompleteOnScopeExit( m_operationID );

    auto loadStatus = base::readTextFile( m_fileToImport );

    auto handleFailure = [this]( const absl::Status& failureStatus ) -> bool
        {
            blog::error::database( FMTX( "Failed to import jam data from [{}]" ), m_fileToImport.string() );

            m_eventBusClient.Send<::events::AddToastNotification>( ::events::AddToastNotification::Type::Error,
                ICON_FA_BOX_OPEN " Jam Import Failed",
                failureStatus.ToString() );

            return true;
        };

    // helper for parsing a type from the yaml, bailing out in a standard way if it fails
#define PARSE_AND_CHECK( _valueName, _valueType, ... )                                      \
    const auto _valueName = data::parseYamlValue<_valueType>( yamlTree, __VA_ARGS__ );      \
    if ( !_valueName.ok() )                                                                 \
        return handleFailure( _valueName.status() );

    if ( !loadStatus.ok() )
    {
        return handleFailure( loadStatus.status() );
    }

    ryml::Tree yamlTree;
    ryml::Parser yamlParser;

    std::string parseName = m_fileToImport.filename().string();

    auto nameView = ryml::csubstr( std::data( parseName ), std::size( parseName ) );
    auto bufferView = ryml::substr( std::data( loadStatus.value() ), std::size( loadStatus.value() ) );

    yamlParser.parse_in_place( nameView, bufferView, &yamlTree );

    // begin a single mass transaction for adding the whole jam
    Warehouse::SqlDB::TransactionGuard txn;

    // fetch required header entries that identify what we're about to load; any missing are considered import failures
    PARSE_AND_CHECK( headerExportTimeUnix,  double,         "export_time_unix" );
    PARSE_AND_CHECK( headerExportOuroVer,   std::string,    "export_ouroveon_version" );
    PARSE_AND_CHECK( headerJamName,         std::string,    "jam_name" );
    PARSE_AND_CHECK( headerJamCouchID,      std::string,    "jam_couch_id" );

    {
        const auto exportTimeUnix = spacetime::InSeconds( std::chrono::seconds( static_cast<uint64_t>( headerExportTimeUnix.value() ) ) );
        const auto exportTimeDelta = spacetime::calculateDeltaFromNow( exportTimeUnix ).asPastTenseString( 3 );

        blog::database( FMTX( "Importing [{}] {}" ), headerJamName.value(), headerJamCouchID.value() );
        blog::database( FMTX( "Export data from v.{}; {}" ), headerExportOuroVer.value(), exportTimeDelta );
    }

    // -----------------------------------------------------------------------------------------------------------------
    ryml::ConstNodeRef yamlRiffsList = yamlTree["riffs"];
    for ( ryml::ConstNodeRef const& yamlRiff : yamlRiffsList.children() )
    {
        const auto riffID = std::string( yamlRiff.key().data(), yamlRiff.key().size() );

        PARSE_AND_CHECK( rUser,         std::string,        "riff-user",    yamlRiff[0].val()  );
        PARSE_AND_CHECK( rTimeUnix,     double,             "riff-ts",      yamlRiff[1].val()  );
        PARSE_AND_CHECK( rRoot,         double,             "riff-root",    yamlRiff[2].val()  );
        PARSE_AND_CHECK( rScale,        double,             "riff-scale",   yamlRiff[4].val()  );
        PARSE_AND_CHECK( rBPS,          data::HexFloat,     "riff-bps",     yamlRiff[7].val()  );
        PARSE_AND_CHECK( rBPMrnd,       data::HexFloat,     "riff-bpm-rnd", yamlRiff[9].val()  );
        PARSE_AND_CHECK( rBarLength,    double,             "riff-bar-len", yamlRiff[10].val() );
        PARSE_AND_CHECK( rAppVersion,   double,             "riff-appver",  yamlRiff[11].val() );
        PARSE_AND_CHECK( rMagnitude,    double,             "riff-mag",     yamlRiff[20].val() );

        std::array< std::string, 8 > stemCouchIDs;
        std::array< float, 8 > stemGains;
        std::array< bool, 8 > stemOn;
        for ( int32_t stemIndex = 0; stemIndex < 8; stemIndex++ )
        {
            const auto stemArrayValid = yamlRiff[12 + stemIndex].is_seq();
            const auto stemArray = yamlRiff[12 + stemIndex];

            PARSE_AND_CHECK( stem_id,   std::string,        fmt::format( FMTX("stem{}-id"),     stemIndex ), stemArray[0].val() );
            PARSE_AND_CHECK( stem_gain, data::HexFloat,     fmt::format( FMTX("stem{}-gain"),   stemIndex ), stemArray[2].val() );
            PARSE_AND_CHECK( stem_on,   bool,               fmt::format( FMTX("stem{}-on"),     stemIndex ), stemArray[3].val() );

            stemCouchIDs[stemIndex] = std::move( stem_id.value() );
            stemGains[stemIndex]    = stem_gain.value().result;
            stemOn[stemIndex]       = stem_on.value();
        }

        auto gainsJsonText = fmt::format( R"([ {} ])", fmt::join( stemGains, ", " ) );

        static constexpr char injectNewRiff[] = R"(
            INSERT OR IGNORE INTO riffs(
                riffCID,
                OwnerJamCID ) VALUES( ?1, ?2 );
        )";

        static constexpr char populateRiffData[] = R"(
            UPDATE riffs SET
                CreationTime=?2,
                Root=?3,
                Scale=?4,
                BPS=?5,
                BPMrnd=?6,
                BarLength=?7,
                AppVersion=?8,
                Magnitude=?9,
                UserName=?10,
                StemCID_1=?11,
                StemCID_2=?12,
                StemCID_3=?13,
                StemCID_4=?14,
                StemCID_5=?15,
                StemCID_6=?16,
                StemCID_7=?17,
                StemCID_8=?18,
                GainsJSON=?19
                WHERE riffCID=?1
        )";

        Warehouse::SqlDB::query<injectNewRiff>(
            riffID.c_str(),
            headerJamCouchID.value()
        );

        Warehouse::SqlDB::query<populateRiffData>(
            riffID.c_str(),
            static_cast<uint64_t>( rTimeUnix.value() ),
            static_cast<uint32_t>( rRoot.value() ),
            static_cast<uint32_t>( rScale.value() ),
            rBPS.value().result,
            rBPMrnd.value().result,
            static_cast<uint32_t>(rBarLength.value()),
            static_cast<uint32_t>(rAppVersion.value()),
            static_cast<float>(rMagnitude.value()),
            rUser.value().c_str(),
            stemOn[0] ? stemCouchIDs[0].c_str() : "",
            stemOn[1] ? stemCouchIDs[1].c_str() : "",
            stemOn[2] ? stemCouchIDs[2].c_str() : "",
            stemOn[3] ? stemCouchIDs[3].c_str() : "",
            stemOn[4] ? stemCouchIDs[4].c_str() : "",
            stemOn[5] ? stemCouchIDs[5].c_str() : "",
            stemOn[6] ? stemCouchIDs[6].c_str() : "",
            stemOn[7] ? stemCouchIDs[7].c_str() : "",
            gainsJsonText.c_str()
        );
    }

    // -----------------------------------------------------------------------------------------------------------------
    ryml::ConstNodeRef yamlStemsList = yamlTree["stems"];
    for ( ryml::ConstNodeRef const& yamlStem : yamlStemsList.children() )
    {
        const auto stemID = std::string( yamlStem.key().data(), yamlStem.key().size() );

        PARSE_AND_CHECK( sFileEnd,      std::string,        "stem-endpoint",yamlStem[0].val()  );
        PARSE_AND_CHECK( sFileBucket,   std::string,        "stem-bucket",  yamlStem[1].val()  );
        PARSE_AND_CHECK( sFileKey,      std::string,        "stem-key",     yamlStem[2].val()  );
        PARSE_AND_CHECK( sFileMIME,     std::string,        "stem-mime",    yamlStem[3].val()  );
        PARSE_AND_CHECK( sFileLenByte,  double,             "stem-f-len",   yamlStem[4].val()  );
        PARSE_AND_CHECK( sSampleRate,   double,             "stem-s-rate",  yamlStem[5].val()  );
        PARSE_AND_CHECK( sTimeUnix,     double,             "stem-ts",      yamlStem[6].val()  );
        PARSE_AND_CHECK( sPreset,       std::string,        "stem-preset",  yamlStem[7].val()  );
        PARSE_AND_CHECK( sUser,         std::string,        "stem-user",    yamlStem[8].val()  );
        PARSE_AND_CHECK( sColour,       std::string,        "stem-colour",  yamlStem[9].val()  );
        PARSE_AND_CHECK( sBPS,          data::HexFloat,     "stem-bps",     yamlStem[11].val() );
        PARSE_AND_CHECK( sBPMrnd,       data::HexFloat,     "stem-bpm-rnd", yamlStem[13].val() );
        PARSE_AND_CHECK( sLength16s,    double,             "stem-len16",   yamlStem[14].val() );
        PARSE_AND_CHECK( sPitch,        double,             "stem-pitch",   yamlStem[15].val() );
        PARSE_AND_CHECK( sBarLength,    double,             "stem-bar-len", yamlStem[16].val() );
        PARSE_AND_CHECK( sIsDrum,       bool,               "stem-is-drum", yamlStem[17].val() );
        PARSE_AND_CHECK( sIsNote,       bool,               "stem-is-note", yamlStem[18].val() );
        PARSE_AND_CHECK( sIsBass,       bool,               "stem-is-bass", yamlStem[19].val() );
        PARSE_AND_CHECK( sIsMic,        bool,               "stem-is-mic",  yamlStem[20].val() );

        int32_t instrumentMask = 0;
        if ( sIsDrum.value() )
            instrumentMask |= 1 << 1;
        if ( sIsNote.value() )
            instrumentMask |= 1 << 2;
        if ( sIsBass.value() )
            instrumentMask |= 1 << 3;
        if ( sIsMic.value() )
            instrumentMask |= 1 << 4;

        static constexpr char injectNewStem[] = R"(
            INSERT OR IGNORE INTO stems(
                stemCID, OwnerJamCID ) VALUES( ?1, ?2 );
        )";

        static constexpr char updateStemDetails[] = R"(
            UPDATE stems SET 
                CreationTime=?2,
                FileEndpoint=?3,
                FileBucket=?4,
                FileKey=?5,
                FileMIME=?6,
                FileLength=?7,
                BPS=?8,
                BPMrnd=?9,
                Instrument=?10,
                Length16s=?11,
                OriginalPitch=?12,
                BarLength=?13,
                PresetName=?14,
                CreatorUserName=?15,
                SampleRate=?16,
                PrimaryColour=?17
                WHERE stemCID=?1
        )";

        Warehouse::SqlDB::query<injectNewStem>(
            stemID.c_str(),
            headerJamCouchID.value()
        );

        Warehouse::SqlDB::query<updateStemDetails>(
            stemID.c_str(),
            static_cast<uint64_t>( sTimeUnix.value() ),
            sFileEnd.value().c_str(),
            sFileBucket.value().c_str(),
            sFileKey.value().c_str(),
            sFileMIME.value().c_str(),
            static_cast<uint64_t>( sFileLenByte.value() ),
            sBPS.value().result,
            sBPMrnd.value().result,
            instrumentMask,
            static_cast<uint64_t>( sLength16s.value() ),
            static_cast<uint64_t>( sPitch.value() ),
            static_cast<uint64_t>( sBarLength.value() ),
            sPreset.value().c_str(),
            sUser.value().c_str(),
            static_cast<uint64_t>(sSampleRate.value()),
            sColour.value().c_str()
        );
    }

    m_eventBusClient.Send<::events::AddToastNotification>( ::events::AddToastNotification::Type::Info,
        ICON_FA_BOX " Jam Import Success",
        fmt::format( FMTX( "Imported data into [{}]" ), headerJamName.value() ) );

    return true;

#undef PARSE_AND_CHECK
}

// ---------------------------------------------------------------------------------------------------------------------
bool GetRiffDataTask::Work( TaskQueue& currentTasks )
{
    blog::database( "[{}] collecting riff data ..", Tag );

    // grab all the riff data
    endlesss::api::RiffDetails riffDetails;
    if ( !riffDetails.fetchBatch( m_netConfig, m_jamCID, m_riffCIDs ) )
    {
        blog::error::database( "[{}] Failed to fetch riff details from jam [{}]", Tag, m_jamCID );
        return false;
    }

    // produce unique list of stem couch IDs from this batch of riffs; we can then mass-fetch the data to ensure its valid
    std::vector< endlesss::types::StemCouchID > stemsToValidate;
    StemSet uniqueStemCIDs;
    uniqueStemCIDs.reserve( riffDetails.rows.size() * 8 );
    stemsToValidate.reserve( riffDetails.rows.size() * 8 );
    for ( const auto& netRiffData : riffDetails.rows )
    {
        types::Riff riffData{ m_jamCID, netRiffData.doc };
        for ( auto stemI = 0; stemI < 8; stemI++ )
        {
            const auto& stemCID = riffData.stems[stemI];
            if ( !stemCID.empty() )
            {
                const auto setInsert = uniqueStemCIDs.emplace( stemCID );
                if ( setInsert.second )
                {
                    stemsToValidate.emplace_back( stemCID );
                }
            }
        }
    }
    blog::database( "[{}] validating {} stems ..", Tag, stemsToValidate.size() );

    // collect the type data for all the stems; there is a strange situation where some stem IDs turn out to
    // be .. chat messages? so we need to remove those early on
    endlesss::api::StemTypeCheck stemValidation;
    if ( !stemValidation.fetchBatch( m_netConfig, m_jamCID, stemsToValidate ) )
    {
        blog::error::database( "[{}] Failed to validate stem details [{}]", Tag );
        return false;
    }
    // re-use the hash set, add in any stems that are found to be problematic
    uniqueStemCIDs.clear();
    for ( const auto& stemCheck : stemValidation.rows )
    {
        // missing key entirely, presumably moderated away
        if ( !stemCheck.error.empty() )
        {
            uniqueStemCIDs.emplace( stemCheck.key );
            blog::database( "[{}] Found stem with a retreival error ({}), ignoring ID [{}]", Tag, stemCheck.error, stemCheck.key );

            sql::ledger::storeStemNote(
                stemCheck.key,
                Warehouse::StemLedgerType::REMOVED_ID,
                fmt::format( "[{}]", stemCheck.error ) );

            continue;
        }

        // does this have vintage attachment data? if so, allow the fact it might be also missing an app version tag
        const bool bThisIsAnOldButValidStem = !stemCheck.doc._attachments.oggAudio.content_type.empty();

        // check for invalid app version - this is usually a red flag for invalid stems but on very old jams this was the norm 
        const bool ignoreForMissingAppData = ( stemCheck.doc.app_version == 0 && bThisIsAnOldButValidStem == false );

        // stem is lacking app versioning (and isn't just old)
        if ( ignoreForMissingAppData )
        {
            uniqueStemCIDs.emplace( stemCheck.key );
            blog::database( "[{}] Found stem without app version ({}), ignoring ID [{}]", Tag, stemCheck.doc._attachments.oggAudio.digest, stemCheck.key );

            sql::ledger::storeStemNote(
                stemCheck.key,
                Warehouse::StemLedgerType::REMOVED_ID,
                fmt::format( "[{}]", stemCheck.error ) );

            continue;
        }
        // stem was destroyed?
        if ( stemCheck.value.deleted )
        {
            uniqueStemCIDs.emplace( stemCheck.key );
            blog::database( "[{}] Found stem that was deleted ({}), ignoring ID [{}]", Tag, stemCheck.error, stemCheck.key );

            sql::ledger::storeStemNote(
                stemCheck.key,
                Warehouse::StemLedgerType::REMOVED_ID,
                fmt::format( "[{}]", stemCheck.error ) );

            continue;
        }

        // this isn't a stem? 
        if ( stemCheck.doc.type != "Loop" )
        {
            uniqueStemCIDs.emplace( stemCheck.doc._id );
            blog::database( "[{}] Found stem that isn't a stem ({}), ignoring ID [{}]", Tag, stemCheck.doc.type, stemCheck.doc._id );

            sql::ledger::storeStemNote(
                stemCheck.doc._id,
                Warehouse::StemLedgerType::DAMAGED_REFERENCE,
                fmt::format( "[Ver:{}] Wrong type [{}]", stemCheck.doc.app_version, stemCheck.doc.type ) );

            continue;
        }
        // this stem was damaged and has no audio data
        if ( stemCheck.doc.cdn_attachments.oggAudio.endpoint.empty() &&
             stemCheck.doc.cdn_attachments.flacAudio.endpoint.empty() )
        {
            uniqueStemCIDs.emplace( stemCheck.doc._id );
            blog::database( "[{}] Found stem that is damaged, ignoring ID [{}]", Tag, stemCheck.doc._id );

            sql::ledger::storeStemNote(
                stemCheck.doc._id,
                Warehouse::StemLedgerType::MISSING_OGG,   // previously this only happened with OGG sources.. potentially we could have missing FLAC here too
                fmt::format( "[Ver:{}]", stemCheck.doc.app_version ) );

            continue;
        }
    }


    static constexpr char updateRiffDetails[] = R"(
        UPDATE riffs SET CreationTime=?2,
                         Root=?3,
                         Scale=?4,
                         BPS=?5,
                         BPMrnd=?6,
                         BarLength=?7,
                         AppVersion=?8,
                         Magnitude=?9,
                         UserName=?10,
                         StemCID_1=?11,
                         StemCID_2=?12,
                         StemCID_3=?13,
                         StemCID_4=?14,
                         StemCID_5=?15,
                         StemCID_6=?16,
                         StemCID_7=?17,
                         StemCID_8=?18,
                         GainsJSON=?19
                         WHERE riffCID=?1
    )";
    static constexpr char insertOrIgnoreNewStemSkeleton[] = R"(
        INSERT OR IGNORE INTO stems( stemCID, OwnerJamCID ) VALUES( ?1, ?2 );
    )";

    blog::database( "[{}] inserting {} rows of riff detail", Tag, riffDetails.rows.size() );

    {
        Warehouse::SqlDB::TransactionGuard txn;
        for ( const auto& netRiffData : riffDetails.rows )
        {
            types::Riff riffData{ m_jamCID, netRiffData.doc };

            for ( auto stemI = 0; stemI < 8; stemI++ )
            {
                const auto& stemCID = riffData.stems[stemI];
                if ( stemCID.empty() )
                    continue;

                // check if this stem is meant to be ignored from the validation phase earlier
                auto stemIter = uniqueStemCIDs.find( stemCID );
                if ( stemIter != uniqueStemCIDs.end() )
                {
                    blog::database( "[{}] Removing stem {} from [{}] as it was marked as invalid", Tag, stemI, riffData.couchID );

                    riffData.stemsOn[stemI] = false;
                    riffData.stems[stemI] = endlesss::types::StemCouchID{ "" };
                }
                else
                {
                    // as we walk the stems, poke the couch ID into the stems table if it doesn't already exist
                    // so that any new ones will be found and filled in later
                    Warehouse::SqlDB::query<insertOrIgnoreNewStemSkeleton>( stemCID.value(), m_jamCID.value() );
                }
            }

            auto gainsJson = fmt::format( R"([ {} ])", fmt::join( riffData.gains, ", " ) );

            Warehouse::SqlDB::query<updateRiffDetails>(
                riffData.couchID.value(),
                riffData.creationTimeUnix,
                riffData.root,
                riffData.scale,
                riffData.BPS,
                riffData.BPMrnd,
                riffData.barLength,
                riffData.appVersion,
                riffData.magnitude,
                riffData.user,
                riffData.stems[0].value(),
                riffData.stems[1].value(),
                riffData.stems[2].value(),
                riffData.stems[3].value(),
                riffData.stems[4].value(),
                riffData.stems[5].value(),
                riffData.stems[6].value(),
                riffData.stems[7].value(),
                gainsJson
            );
        }
    }

    // add an additional pause to network fetch tasks to avoid hitting Endlesss too hard
    addNetworkPause();

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
bool GetStemData::Work( TaskQueue& currentTasks )
{
    blog::database( "[{}] collecting stem data ..", Tag );

    endlesss::api::StemDetails stemDetails;
    if ( !stemDetails.fetchBatch( m_netConfig, m_jamCID, m_stemCIDs ) )
    {
        blog::error::database( "[{}] Failed to fetch stem details from jam [{}]", Tag, m_jamCID );
        return false;
    }

    static constexpr char updateStemDetails[] = R"(
        UPDATE stems SET CreationTime=?2,
                         FileEndpoint=?3,
                         FileBucket=?4,
                         FileKey=?5,
                         FileMIME=?6,
                         FileLength=?7,
                         BPS=?8,
                         BPMrnd=?9,
                         Instrument=?10,
                         Length16s=?11,
                         OriginalPitch=?12,
                         BarLength=?13,
                         PresetName=?14,
                         CreatorUserName=?15,
                         SampleRate=?16,
                         PrimaryColour=?17
                         WHERE stemCID=?1
    )";

    blog::database( "[{}] inserting {} rows of stem detail", Tag, stemDetails.rows.size() );

    {
        Warehouse::SqlDB::TransactionGuard txn;
        for ( const auto& stemData : stemDetails.rows )
        {
            const auto unixTime = (uint32_t)(stemData.doc.created / 1000); // from unix nano

            int32_t instrumentMask = 0;
            if ( stemData.doc.isDrum )
                instrumentMask |= 1 << 1;
            if ( stemData.doc.isNote )
                instrumentMask |= 1 << 2;
            if ( stemData.doc.isBass )
                instrumentMask |= 1 << 3;
            if ( stemData.doc.isMic )
                instrumentMask |= 1 << 4;

            const endlesss::api::IStemAudioFormat& audioFormat = stemData.doc.cdn_attachments.getAudioFormat();

            Warehouse::SqlDB::query<updateStemDetails>(
                stemData.id.value(),
                unixTime,
                audioFormat.getEndpoint().data(),
                audioFormat.getBucket().data(),
                audioFormat.getKey().data(),
                audioFormat.getMIME().data(),
                audioFormat.getLength(),
                stemData.doc.bps,
                types::BPStoRoundedBPM( stemData.doc.bps ),
                instrumentMask,
                stemData.doc.length16ths,
                stemData.doc.originalPitch,
                stemData.doc.barLength,
                stemData.doc.presetName,
                stemData.doc.creatorUserName,
                (int32_t)stemData.doc.sampleRate,
                stemData.doc.primaryColour
            );
        }
    }

    // add an additional pause to network fetch tasks to avoid hitting Endlesss too hard
    addNetworkPause();

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
bool JamSliceTask::Work( TaskQueue& currentTasks )
{
    spacetime::ScopedTimer stemTiming( "JamSliceTask::Work" );

    // count up riffs and prepare a memory buffer to populate with the results
    const int64_t riffCount = sql::riffs::countPopulated( m_jamCID, true );
    auto resultSlice = std::make_unique<Warehouse::JamSlice>( m_jamCID, riffCount );

    // extract the basic riff information and weave in user data from the stems table so that we can 
    // do identification analysis in the resulting data slice (this does make this query a fair bit slower due to
    // how I structured the data .. but it's still under half a second for a 45k jam on this machine, in release)
    static constexpr char _sqlExtractRiffBits[] = R"(
        select riffs.OwnerJamCID,
               riffs.RiffCID,
               riffs.CreationTime,
               riffs.UserName,
               riffs.Root,
               riffs.Scale,
               riffs.BPMrnd,
               riffs.StemCID_1, s1.CreatorUserName,
               riffs.StemCID_2, s2.CreatorUserName,
               riffs.StemCID_3, s3.CreatorUserName,
               riffs.StemCID_4, s4.CreatorUserName,
               riffs.StemCID_5, s5.CreatorUserName,
               riffs.StemCID_6, s6.CreatorUserName,
               riffs.StemCID_7, s7.CreatorUserName,
               riffs.StemCID_8, s8.CreatorUserName
        from riffs
        left join stems as s1 on s1.StemCID = riffs.StemCID_1
        left join stems as s2 on s2.StemCID = riffs.StemCID_2
        left join stems as s3 on s3.StemCID = riffs.StemCID_3
        left join stems as s4 on s4.StemCID = riffs.StemCID_4
        left join stems as s5 on s5.StemCID = riffs.StemCID_5
        left join stems as s6 on s6.StemCID = riffs.StemCID_6
        left join stems as s7 on s7.StemCID = riffs.StemCID_7
        left join stems as s8 on s8.StemCID = riffs.StemCID_8
        where riffs.OwnerJamCID is ?1 and riffs.AppVersion is not null
        order by riffs.CreationTime;
        )";

    auto query = Warehouse::SqlDB::query<_sqlExtractRiffBits>( m_jamCID.value() );

    std::string_view jamCID,
                     riffCID,
                     username;
    int64_t          timestamp;
    uint8_t          root;
    uint8_t          scale;
    float            bpmrnd;
    std::array< std::string_view, 8 > stemCIDs;
    std::array< std::string_view, 8 > stemUsers;

    // for tracking data from previous riff in the list
    // unrolled to the basics - flat inline buffers to store and compare to
    constexpr static std::size_t stemIDCharSize = 36;
    std::array< char[stemIDCharSize], 8 > previousStemIDs;
    for ( auto stemI = 0; stemI < 8; stemI++ )
        memset( previousStemIDs[stemI], 0, stemIDCharSize );

    // data for comparisons with previous riff
    spacetime::InSeconds previousRiffTimestamp;
    int8_t               previousNumberOfActiveStems = 0;
    int8_t               previousnumberOfUnseenStems = 0;
    bool                 firstRiffInSequence = true;

    // hashing used for usernames, matching what the apps use too
    absl::Hash<std::string_view> nameHasher;

    while ( query( jamCID,
                   riffCID,
                   timestamp,
                   username,
                   root,
                   scale,
                   bpmrnd,
                   stemCIDs[0],
                   stemUsers[0],
                   stemCIDs[1],
                   stemUsers[1],
                   stemCIDs[2],
                   stemUsers[2],
                   stemCIDs[3],
                   stemUsers[3],
                   stemCIDs[4],
                   stemUsers[4],
                   stemCIDs[5],
                   stemUsers[5],
                   stemCIDs[6],
                   stemUsers[6],
                   stemCIDs[7],
                   stemUsers[7] ) )
    {
        const uint64_t hashedUsername = nameHasher(username);

        const auto contextTimestamp = spacetime::InSeconds{ std::chrono::seconds{ timestamp } };

        resultSlice->m_ids.emplace_back( riffCID );
        resultSlice->m_timestamps.emplace_back( contextTimestamp );
        resultSlice->m_userhash.emplace_back( hashedUsername );
        resultSlice->m_roots.emplace_back( root );
        resultSlice->m_scales.emplace_back( scale );
        resultSlice->m_bpms.emplace_back( bpmrnd );

        int8_t numberOfActiveStems = 0;
        int8_t numberOfUnseenStems = 0;
        for ( const auto& stemCID : stemCIDs )
        {
            if ( !stemCID.empty() )
                numberOfActiveStems++;

            // compare current stem CID with our list of previous ones, see if we saw it in the previous riff
            bool sawPreviousStem = false;
            for ( auto stemI = 0; stemI < 8; stemI++ )
            {
                if ( memcmp( previousStemIDs[stemI], stemCID.data(), stemCID.length()) == 0 )
                {
                    sawPreviousStem = true;
                    break;
                }
            }
            if ( sawPreviousStem == false )
                numberOfUnseenStems++;
        }

        // encode per-stem user names as their hashes
        {
            Warehouse::JamSlice::StemUserHashes& stemUserHashes = resultSlice->m_stemUserHashes.emplace_back();
            for ( auto stemI = 0; stemI < 8; stemI++ )
                stemUserHashes[stemI] = nameHasher(stemUsers[stemI]);
        }

        // first riff reports no deltas
        if ( firstRiffInSequence )
        {
            resultSlice->m_deltaSeconds.push_back( 0 );
            resultSlice->m_deltaStem.push_back( 0 );
        }
        // compute deltas from last riff
        else
        {
            const int8_t changeInActiveStems = numberOfActiveStems - previousNumberOfActiveStems;

            resultSlice->m_deltaSeconds.push_back( static_cast<int32_t>( (contextTimestamp - previousRiffTimestamp).count() ) );
            resultSlice->m_deltaStem.push_back( std::max( numberOfUnseenStems, (int8_t)std::abs(changeInActiveStems) ) );
        }
        firstRiffInSequence = false;

        // stash our current state for deltas
        previousRiffTimestamp = contextTimestamp;
        previousNumberOfActiveStems = numberOfActiveStems;
        previousnumberOfUnseenStems = numberOfUnseenStems;

        // keep unordered set of stem IDs
        for ( auto stemI = 0; stemI < 8; stemI++ )
            strcpy( previousStemIDs[stemI], stemCIDs[stemI].data() );
    }

    // move the report out to the callback for it to deal with
    if ( m_reportCallback )
        m_reportCallback( m_jamCID, std::move(resultSlice) );

    return true;
}


// ---------------------------------------------------------------------------------------------------------------------
bool ContentsReportTask::Work( TaskQueue& currentTasks )
{
    spacetime::ScopedTimer stemTiming( "ContentsReportTask::Work" );

    Warehouse::ContentsReport reportResult;

    absl::flat_hash_set< std::string > uniqueJamIDs;
    uniqueJamIDs.reserve( 64 );

    // when brand new jams arrive, they don't have any stem data initially so [gatherPopulations] returns nothing
    // as the JOIN fails; however, to ensure we get the name of the jam into the contents report early so users don't
    // wonder why it isn't there (before enough data is synced to magic up some empty stem entries), we first grab 
    // a plain distinct set of jam IDs from the riff table and delete from it anything that [gatherPopulations] pulls in,
    // leaving us with a hash set of jam IDs that have no synced data yet - which we can then emit as 0,0,0,0 in the report
    {
        static constexpr char gatherUniqueJams[] = R"(
        select distinct OwnerJamCID from Riffs
        )";

        auto query = Warehouse::SqlDB::query<gatherUniqueJams>();

        std::string_view jamCID;
        while ( query( jamCID ) )
        {
            // ignore the magic virtual jam collection
            if ( jamCID == Warehouse::cVirtualJamName )
                continue;

            uniqueJamIDs.emplace( jamCID );
        }
    }
    {
        // gather unified set of empty/not-empty counts from steams and riffs in a single blast
        static constexpr char gatherPopulations[] = R"(
        SELECT a.OwnerJamCID, a.FilledRiffs, a.EmptyRiffs, b.FilledStems, b.EmptyStems
            FROM 
            (
                SELECT Riffs.OwnerJamCID,
                    count(case when Riffs.AppVersion is null then 1 end) as EmptyRiffs,
                    count(case when Riffs.AppVersion is not null then 1 end) as FilledRiffs
                FROM Riffs
                GROUP BY Riffs.OwnerJamCID
            ) as a
            JOIN
            (
                SELECT Stems.OwnerJamCID,
                    count(case when Stems.CreationTime is null then 1 end) as EmptyStems,
                    count(case when Stems.CreationTime is not null then 1 end) as FilledStems
                FROM Stems
                GROUP BY Stems.OwnerJamCID
            ) as b
            ON a.OwnerJamCID = b.OwnerJamCID
        )";

        auto query = Warehouse::SqlDB::query<gatherPopulations>();
    
        std::string_view jamCID;
        int64_t totalPopulatedRiffs;
        int64_t totalUnpopulatedRiffs;
        int64_t totalPopulatedStems;
        int64_t totalUnpopulatedStems;

        while ( query( jamCID,
                       totalPopulatedRiffs,
                       totalUnpopulatedRiffs,
                       totalPopulatedStems,
                       totalUnpopulatedStems ) )
        {
            // ignore the magic virtual jam collection
            if ( jamCID == Warehouse::cVirtualJamName )
                continue;

            uniqueJamIDs.erase( jamCID );   // this jam has sync data, remove it from the unique list as we'll be emitting data for it normally

            reportResult.m_jamCouchIDs.emplace_back( jamCID );

            reportResult.m_populatedRiffs.emplace_back( totalPopulatedRiffs );
            reportResult.m_unpopulatedRiffs.emplace_back( totalUnpopulatedRiffs );

            reportResult.m_populatedStems.emplace_back( totalPopulatedStems );
            reportResult.m_unpopulatedStems.emplace_back( totalUnpopulatedStems );

            reportResult.m_awaitingInitialSync.emplace_back( false );

            reportResult.m_totalPopulatedRiffs      += totalPopulatedRiffs;
            reportResult.m_totalUnpopulatedRiffs    += totalUnpopulatedRiffs;
            reportResult.m_totalPopulatedStems      += totalPopulatedStems;
            reportResult.m_totalUnpopulatedStems    += totalUnpopulatedStems;
        }
    }

    // all remaining unique jam IDs haven't even had a single round of data sync yet but we want to show
    // the user we're going to consider them soon
    if ( !uniqueJamIDs.empty() )
    {
        for ( const auto& unSyncJam : uniqueJamIDs )
        {
            reportResult.m_jamCouchIDs.emplace_back( unSyncJam );

            reportResult.m_populatedRiffs.emplace_back( 0 );
            reportResult.m_unpopulatedRiffs.emplace_back( 0 );

            reportResult.m_populatedStems.emplace_back( 0 );
            reportResult.m_unpopulatedStems.emplace_back( 0 );

            reportResult.m_awaitingInitialSync.emplace_back( true );
        }
    }

    if ( m_reportCallback )
        m_reportCallback( reportResult );

    return true;
}

} // namespace toolkit
} // namespace endlesss
