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

#include "math/rng.h"
#include "spacetime/chronicle.h"

#include "app/module.frontend.fonts.h"

#include "endlesss/core.types.h"
#include "endlesss/core.constants.h"
#include "endlesss/cache.jams.h"
#include "endlesss/toolkit.warehouse.h"
#include "endlesss/api.h"

#include "app/core.h"


namespace endlesss {
namespace toolkit {

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
    virtual bool forceContentReport() const { return false; }

    virtual const char* getTag() = 0;
    virtual std::string Describe() = 0;
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
};

// ---------------------------------------------------------------------------------------------------------------------
struct ContentsReportTask final : Warehouse::ITask
{
    static constexpr char Tag[] = "CONTENTS";

    ContentsReportTask( const Warehouse::ContentsReportCallback& callbackOnCompletion )
        : ITask()
        , m_reportCallback( callbackOnCompletion )
    {}

    Warehouse::ContentsReportCallback m_reportCallback;

    const char* getTag() override { return Tag; }
    std::string Describe() override { return fmt::format( "[{}] creating database contents report", Tag ); }
    bool Work( TaskQueue& currentTasks ) override;
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamSliceTask final : Warehouse::ITask
{
    static constexpr char Tag[] = "JAMSLICE";

    JamSliceTask( const types::JamCouchID& jamCID, const Warehouse::JamSliceCallback& callbackOnCompletion )
        : ITask()
        , m_jamCID( jamCID )
        , m_reportCallback( callbackOnCompletion )
    {}

    types::JamCouchID               m_jamCID;
    Warehouse::JamSliceCallback     m_reportCallback;

    const char* getTag() override { return Tag; }
    std::string Describe() override { return fmt::format( "[{}] extracting jam data for [{}]", Tag, m_jamCID.value() ); }
    bool Work( TaskQueue& currentTasks ) override;
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamSnapshotTask final : Warehouse::INetworkTask
{
    static constexpr char Tag[] = "SNAPSHOT";

    JamSnapshotTask( const api::NetConfiguration& ncfg, const types::JamCouchID& jamCID )
        : Warehouse::INetworkTask( ncfg )
        , m_jamCID( jamCID )
    {}

    // always trigger a contents update after snapping a new jam
    bool forceContentReport() const override { return true; }

    types::JamCouchID m_jamCID;

    const char* getTag() override { return Tag; }
    std::string Describe() override { return fmt::format( "[{}] fetching Jam snapshot of [{}]", Tag, m_jamCID ); }
    bool Work( TaskQueue& currentTasks ) override;
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamPurgeTask final : Warehouse::ITask
{
    static constexpr char Tag[] = "PURGE";

    JamPurgeTask( const types::JamCouchID& jamCID )
        : Warehouse::ITask()
        , m_jamCID( jamCID )
    {}

    // always trigger a contents update after wiping out data
    bool forceContentReport() const override { return true; }

    types::JamCouchID m_jamCID;

    const char* getTag() override { return Tag; }
    std::string Describe() override { return fmt::format( "[{}] deleting all records for [{}]", Tag, m_jamCID ); }
    bool Work( TaskQueue& currentTasks ) override;
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamSyncAbortTask final : Warehouse::ITask
{
    static constexpr char Tag[] = "SYNC-ABORT";

    JamSyncAbortTask( const types::JamCouchID& jamCID )
        : Warehouse::ITask()
        , m_jamCID( jamCID )
    {}

    // always trigger a contents update after wiping out data
    bool forceContentReport() const override { return true; }

    types::JamCouchID m_jamCID;

    const char* getTag() override { return Tag; }
    std::string Describe() override { return fmt::format( "[{}] purging empty riff records for [{}]", Tag, m_jamCID ); }
    bool Work( TaskQueue& currentTasks ) override;
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamExportTask final : Warehouse::ITask
{
    static constexpr char Tag[] = "EXPORT";

    JamExportTask( base::EventBusClient& eventBus, const types::JamCouchID& jamCID, const fs::path& exportFolder, std::string_view jamName )
        : Warehouse::ITask()
        , m_eventBusClient( eventBus )
        , m_jamCID( jamCID )
        , m_exportFolder( exportFolder )
        , m_jamName( jamName )
    {}

    base::EventBusClient    m_eventBusClient;
    types::JamCouchID       m_jamCID;
    fs::path                m_exportFolder;
    std::string             m_jamName;

    const char* getTag() override { return Tag; }
    std::string Describe() override { return fmt::format( "[{}] exporting jam to disk", Tag ); }
    bool Work( TaskQueue& currentTasks ) override;
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamImportTask final : Warehouse::ITask
{
    static constexpr char Tag[] = "IMPORT";

    JamImportTask( base::EventBusClient& eventBus, const fs::path& fileToImport )
        : Warehouse::ITask()
        , m_eventBusClient( eventBus )
        , m_fileToImport( fileToImport )
    {}

    base::EventBusClient    m_eventBusClient;
    fs::path                m_fileToImport;

    const char* getTag() override { return Tag; }
    std::string Describe() override { return fmt::format( "[{}] importing jam from disk", Tag ); }
    bool Work( TaskQueue& currentTasks ) override;
};

// ---------------------------------------------------------------------------------------------------------------------
struct GetRiffDataTask final : Warehouse::INetworkTask
{
    static constexpr char Tag[] = "RIFFDATA";

    GetRiffDataTask( const api::NetConfiguration& ncfg, const types::JamCouchID& jamCID, const std::vector< types::RiffCouchID >& riffCIDs )
        : Warehouse::INetworkTask( ncfg )
        , m_jamCID( jamCID )
        , m_riffCIDs( riffCIDs )
    {}

    types::JamCouchID                 m_jamCID;
    std::vector< types::RiffCouchID > m_riffCIDs;

    const char* getTag() override { return Tag; }
    std::string Describe() override { return fmt::format( "[{}] pulling {} riff details", Tag, m_riffCIDs.size() ); }
    bool Work( TaskQueue& currentTasks ) override;
};

// ---------------------------------------------------------------------------------------------------------------------
struct GetStemData final : Warehouse::INetworkTask
{
    static constexpr char Tag[] = "STEMDATA";

    GetStemData( const api::NetConfiguration& ncfg, const types::JamCouchID& jamCID, const std::vector< types::StemCouchID >& stemCIDs )
        : Warehouse::INetworkTask( ncfg )
        , m_jamCID( jamCID )
        , m_stemCIDs( stemCIDs )
    {}

    types::JamCouchID                 m_jamCID;
    std::vector< types::StemCouchID > m_stemCIDs;

    const char* getTag() override { return Tag; }
    std::string Describe() override { return fmt::format( "[{}] pulling {} stem details", Tag, m_stemCIDs.size() ); }
    bool Work( TaskQueue& currentTasks ) override;
};



struct Warehouse::TaskSchedule
{
    TaskQueue   m_taskQueue;
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
            select count(*) from riffs where OwnerJamCID is ?1 and CreationTime is null;
        )";
        static constexpr char _sqlCountTotalRiffsInJamPopulated[] = R"(
            select count(*) from riffs where OwnerJamCID is ?1 and CreationTime is not null;
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
            select OwnerJamCID, riffCID from riffs where CreationTime is null limit 1 )";

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
            select riffCID from riffs where OwnerJamCID is ?1 and CreationTime is null limit ?2 )";

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

    enum class StemLedgerType
    {
        MISSING_OGG         = 1,            // ogg data vanished; this is mostly due to the broken beta that went out
        DAMAGED_REFERENCE   = 2,            // sometimes chat messages (?!) or other riffs (??) seem to have been stored as stem CouchIDs 
        REMOVED_ID          = 3             // just .. gone. couch ID not found. unrecoverable
    };

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
    static void storeStemNote( const types::StemCouchID& stemCID, const StemLedgerType& type, const std::string& note )
    {
        static constexpr char _sqlAddLedger[] = R"(
            INSERT OR IGNORE INTO StemLedger( StemCID, Type, Note ) VALUES( ?1, ?2, ?3 );
        )";

        Warehouse::SqlDB::query<_sqlAddLedger>( stemCID.value(), (int32_t)type, note );
    }

} // namespace ledger

} // namespace sql


// ---------------------------------------------------------------------------------------------------------------------
Warehouse::Warehouse( const app::StoragePaths& storagePaths, api::NetConfiguration::Shared& networkConfig, base::EventBusClient eventBus )
    : m_networkConfiguration( networkConfig )
    , m_eventBusClient( eventBus )
    , m_workerThreadPaused( false )
{
    blog::database( FMTX("sqlite version {}"), SQLITE_VERSION );

    m_taskSchedule = std::make_unique<TaskSchedule>();

    m_databaseFile = ( storagePaths.cacheCommon / "warehouse.db3" ).string();
    SqlDB::post_connection_hook = []( sqlite3* db_handle )
    {
        // https://www.sqlite.org/pragma.html#pragma_temp_store
        sqlite3_exec( db_handle, "pragma temp_store = memory", nullptr, nullptr, nullptr );
    };

    // set the database up; creating tables & indices if we're starting fresh
    sql::jams::runInit();
    sql::riffs::runInit();
    sql::tags::runInit();
    sql::stems::runInit();
    sql::ledger::runInit();

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

    {
        spacetime::ScopedTimer stemTiming( "warehouse [optimize]" );
        static constexpr char sqlOptimize[] = R"(pragma optimize)";
        SqlDB::query<sqlOptimize>();
    }

    m_workerThread->join();
    m_workerThread.reset();
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::setCallbackWorkReport( const WorkUpdateCallback& cb )
{
    std::scoped_lock<std::mutex> cbLock( m_cbMutex );
    m_cbWorkUpdateToInstall = cb;
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::setCallbackContentsReport( const ContentsReportCallback& cb )
{
    std::scoped_lock<std::mutex> cbLock( m_cbMutex );
    m_cbContentsReportToInstall = cb;
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::setCallbackTagUpdate( const TagUpdateCallback& cbUpdate, const TagBatchingCallback& cbBatch )
{
    std::scoped_lock<std::mutex> cbLock( m_cbMutex );
    m_cbTagUpdate = cbUpdate;
    m_cbTagBatching = cbBatch;
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::setCallbackTagRemoved( const TagRemovedCallback& cb )
{
    std::scoped_lock<std::mutex> cbLock( m_cbMutex );
    m_cbTagRemoved = cb;
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::clearAllCallbacks()
{
    std::scoped_lock<std::mutex> cbLock( m_cbMutex );
    m_cbWorkUpdateToInstall = nullptr;
    m_cbContentsReportToInstall = nullptr;
    m_cbTagUpdate = nullptr;
    m_cbTagBatching = nullptr;
    m_cbTagRemoved = nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::requestContentsReport()
{
    m_taskSchedule->m_taskQueue.enqueue( std::make_unique<ContentsReportTask>( m_cbContentsReport ) );
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
    jamCache.iterateAllJams( [this]( const cache::Jams::Data& jamData )
        {
            upsertSingleJamIDToName( jamData.m_jamCID, jamData.m_displayName );
        });
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
void Warehouse::addOrUpdateJamSnapshot( const types::JamCouchID& jamCouchID )
{
    if ( jamCouchID.empty() )
    {
        blog::error::database( "cannot add empty jam ID to warehouse" );
        return;
    }
    if ( !hasFullEndlesssNetworkAccess() )
    {
        blog::error::database( "cannot call Warehouse::addOrUpdateJamSnapshot() with no active Endlesss network" );
        return;
    }

    m_taskSchedule->m_taskQueue.enqueue( std::make_unique<JamSnapshotTask>( *m_networkConfiguration, jamCouchID ) );
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::addJamSliceRequest( const types::JamCouchID& jamCouchID, const JamSliceCallback& callbackOnCompletion )
{
    if ( jamCouchID.empty() )
    {
        blog::error::database( "empty Jam ID passed to warehouse for slice request" );
        return;
    }

    m_taskSchedule->m_taskQueue.enqueue( std::make_unique<JamSliceTask>( jamCouchID, callbackOnCompletion ) );
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::requestJamPurge( const types::JamCouchID& jamCouchID )
{
    if ( jamCouchID.empty() )
    {
        blog::error::database( "empty Jam ID passed to warehouse for purge" );
        return;
    }

    m_taskSchedule->m_taskQueue.enqueue( std::make_unique<JamPurgeTask>( jamCouchID ) );
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::requestJamSyncAbort( const types::JamCouchID& jamCouchID )
{
    if ( jamCouchID.empty() )
    {
        blog::error::database( "empty Jam ID passed to warehouse for sync abort" );
        return;
    }

    m_taskSchedule->m_taskQueue.enqueue( std::make_unique<JamSyncAbortTask>( jamCouchID ) );
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::requestJamExport( const types::JamCouchID& jamCouchID, const fs::path exportFolder, std::string_view jamTitle )
{
    if ( jamCouchID.empty() )
    {
        blog::error::database( "empty Jam ID passed to warehouse for export" );
        return;
    }

    m_taskSchedule->m_taskQueue.enqueue( std::make_unique<JamExportTask>( m_eventBusClient, jamCouchID, exportFolder, jamTitle ) );
}

// ---------------------------------------------------------------------------------------------------------------------
bool Warehouse::fetchSingleRiffByID( const endlesss::types::RiffCouchID& riffID, endlesss::types::RiffComplete& result ) const
{
    if ( !sql::riffs::getSingleByID( riffID, result.riff ) )
        return false;

    result.jam.couchID = result.riff.jamCouchID;

    sql::jams::getPublicNameForID( result.jam.couchID, result.jam.displayName );

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
bool Warehouse::batchFindJamIDForStem( const endlesss::types::StemCouchIDs& stems, endlesss::types::JamCouchIDs& result ) const
{
    static constexpr char _ownerJamForStemID[] = R"(
            select OwnerJamCID from stems where stemCID = ?1;
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
bool Warehouse::fetchAllStemsForJam( const types::JamCouchID& jamCouchID, endlesss::types::StemCouchIDs& result ) const
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

    // pull all stem IDs out
    {
        static constexpr char _allStemsInJam[] = R"(
            select StemCID from stems where OwnerJamCID = ?1 order by CreationTime asc;
        )";

        auto query = Warehouse::SqlDB::query<_allStemsInJam>( jamCouchID.value() );

        std::string_view stemCID;

        while ( query( stemCID ) )
        {
            result.emplace_back( endlesss::types::StemCouchID( stemCID ) );
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
bool Warehouse::fetchAllStems( endlesss::types::StemCouchIDs& result ) const
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

    // pull all stem IDs out
    {
        static constexpr char _allStems[] = R"(
            select StemCID from stems;
        )";

        auto query = Warehouse::SqlDB::query<_allStems>();

        std::string_view stemCID;

        while ( query( stemCID ) )
        {
            result.emplace_back( endlesss::types::StemCouchID( stemCID ) );
        }
    }

    return !result.empty();
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

    math::RNG32 rng;

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
        // #HDD refactor how this thread works;
        //      we should be sleeping it until we know there's potential work and controlling any rate-limiting
        //      by virtue of what tasks are running rather than a generic top-level wait
        //      (as this also interferes with local requests like generating jam-slices)
        //
        std::this_thread::sleep_for( std::chrono::milliseconds( rng.genInt32(250, 700) ) );

        checkLockAndInstallNewCallbacks();

        // cycle round if paused
        if ( m_workerThreadPaused )
        {
            continue;
        }

        // something to do?
        Task nextTask;
        if ( m_taskSchedule->m_taskQueue.try_dequeue( nextTask ) )
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

            if ( nextTask->forceContentReport() )
                tryEnqueueReport( true );
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
                        m_taskSchedule->m_taskQueue.enqueue( std::make_unique<GetStemData>( *m_networkConfiguration, owningJamCID, emptyStems ) );
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
                        m_taskSchedule->m_taskQueue.enqueue( std::make_unique<GetRiffDataTask>( *m_networkConfiguration, owningJamCID, emptyRiffs ) );
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
                m_cbWorkUpdate( false, "No tasks queued" );
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
    blog::database( "[{}] requesting full riff manifest", Tag );

    endlesss::api::JamFullSnapshot jamSnapshot;
    if ( !jamSnapshot.fetch( m_netConfig, m_jamCID ) )
    {
        blog::error::database( "[{}] Failed to fetch snapshot for jam [{}]", Tag, m_jamCID );
        return false;
    }

    const auto countBeforeTx = sql::riffs::countRiffsInJam( m_jamCID );

    // the snapshot is only for getting the root couch IDs for all riffs in a jam; further details
    // are then plucked one-by-one as we fill the warehouse. newly discovered riffs will INSERT, previously
    // touched ones are ignored
    static constexpr char insertOrIgnoreNewRiffSkeleton[] = R"(
        INSERT OR IGNORE INTO riffs( riffCID, OwnerJamCID ) VALUES( ?1, ?2 );
    )";

    {
        // bundle into single transaction
        Warehouse::SqlDB::TransactionGuard txn;
        for ( const auto& jamData : jamSnapshot.rows )
        {
            const auto& riffCID = jamData.id;
            Warehouse::SqlDB::query<insertOrIgnoreNewRiffSkeleton>( riffCID.value(), m_jamCID.value() );
        }
    }

    const auto countAfterTx = sql::riffs::countRiffsInJam( m_jamCID );

    blog::database( "[{}] {} online, added {} to Db", Tag, jamSnapshot.rows.size(), countAfterTx - countBeforeTx );
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
        delete from riffs where OwnerJamCID = ?1 and Riffs.CreationTime is null;
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
    std::string sanitisedJamName;
    {
        base::sanitiseNameForPath( m_jamName, sanitisedJamName, '_', false );
        sanitisedJamName = "ldx." + m_jamCID.value() + "." + base::StrToLwrExt(sanitisedJamName) + ".yaml";
    }
    const fs::path finalOutputFile = m_exportFolder / sanitisedJamName;
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
        fmt::format( FMTX( "Written to {}" ), sanitisedJamName ) );

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
bool JamImportTask::Work( TaskQueue& currentTasks )
{
    auto loadStatus = base::readTextFile( m_fileToImport );

    return true;
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
                sql::ledger::StemLedgerType::REMOVED_ID,
                fmt::format( "[{}]", stemCheck.error ) );

            continue;
        }

        // does this have vintage attachment data? if so, allow the fact it might be also missing an app version tag
        const bool bThisIsAnOldButValidStem = !stemCheck.doc._attachments.oggAudio.content_type.empty();

        // check for invalid app version - this is usually a red flag for invalid stems but on very old jams this
        // was the norm - there is a config flag that allows these to pass and be synced
        const bool ignoreForMissingAppData = ( stemCheck.doc.app_version == 0 && bThisIsAnOldButValidStem == false );

        // stem is lacking app versioning (and isn't just old)
        if ( ignoreForMissingAppData )
        {
            uniqueStemCIDs.emplace( stemCheck.key );
            blog::database( "[{}] Found stem without app version ({}), ignoring ID [{}]", Tag, stemCheck.doc._attachments.oggAudio.digest, stemCheck.key );

            sql::ledger::storeStemNote(
                stemCheck.key,
                sql::ledger::StemLedgerType::REMOVED_ID,
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
                sql::ledger::StemLedgerType::REMOVED_ID,
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
                sql::ledger::StemLedgerType::DAMAGED_REFERENCE,
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
                sql::ledger::StemLedgerType::MISSING_OGG,   // previously this only happened with OGG sources.. potentially we could have missing FLAC here too
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
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
bool JamSliceTask::Work( TaskQueue& currentTasks )
{
    spacetime::ScopedTimer stemTiming( "JamSliceTask::Work" );

    const int64_t riffCount = sql::riffs::countPopulated( m_jamCID, true );

    auto resultSlice = std::make_unique<Warehouse::JamSlice>( m_jamCID, riffCount );

    static constexpr char _sqlExtractRiffBits[] = R"(
            select OwnerJamCID,RiffCID,CreationTime,UserName,Root,Scale,BPMrnd,StemCID_1,StemCID_2,StemCID_3,StemCID_4,StemCID_5,StemCID_6,StemCID_7,StemCID_8 
            from riffs 
            where OwnerJamCID is ?1 and CreationTime is not null 
            order by CreationTime;
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

    // for tracking data from previous riff in the list
    absl::flat_hash_set< std::string_view > previousStemIDs;
    previousStemIDs.reserve( 8 );

    spacetime::InSeconds previousRiffTimestamp;
    int8_t               previousNumberOfActiveStems = 0;
    int8_t               previousnumberOfUnseenStems = 0;
    bool                 firstRiffInSequence = true;

    while ( query( jamCID,
                   riffCID,
                   timestamp,
                   username,
                   root,
                   scale,
                   bpmrnd,
                   stemCIDs[0],
                   stemCIDs[1],
                   stemCIDs[2],
                   stemCIDs[3],
                   stemCIDs[4],
                   stemCIDs[5],
                   stemCIDs[6],
                   stemCIDs[7] ) )
    {
        const uint64_t hashedUsername = absl::Hash<std::string_view>{}(username);

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
            if ( !previousStemIDs.contains( stemCID ) )
                numberOfUnseenStems++;
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
        previousStemIDs.clear();
        for ( const auto& stemCID : stemCIDs )
            previousStemIDs.emplace( stemCID );
    }

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
                    count(case when Riffs.CreationTime is null then 1 end) as EmptyRiffs,
                    count(case when Riffs.CreationTime is not null then 1 end) as FilledRiffs
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
            uniqueJamIDs.erase( jamCID );   // this jam has sync data, remove it from the unique list as we'll be emitting data for it normally

            reportResult.m_jamCouchIDs.emplace_back( jamCID );

            reportResult.m_populatedRiffs.emplace_back( totalPopulatedRiffs );
            reportResult.m_unpopulatedRiffs.emplace_back( totalUnpopulatedRiffs );

            reportResult.m_populatedStems.emplace_back( totalPopulatedStems );
            reportResult.m_unpopulatedStems.emplace_back( totalUnpopulatedStems );

            reportResult.m_awaitingInitialSync.emplace_back( false );
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
