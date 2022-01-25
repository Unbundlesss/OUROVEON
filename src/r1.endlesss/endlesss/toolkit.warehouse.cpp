//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"
#include "math/rng.h"
#include "base/spacetime.h"

#include "endlesss/core.types.h"
#include "endlesss/cache.jams.h"
#include "endlesss/toolkit.warehouse.h"
#include "endlesss/api.h"

#include "app/core.h"


namespace endlesss {

std::string Warehouse::m_databaseFile;


using StemSet   = robin_hood::unordered_flat_set< endlesss::types::StemCouchID, cid_hash<endlesss::types::StemCouchID> >;

using Task      = std::unique_ptr<Warehouse::ITask>;
using TaskQueue = mcc::ConcurrentQueue<Task>;

// ---------------------------------------------------------------------------------------------------------------------
// worker thread task abstract interface; it owns a connection to the API routing
struct Warehouse::ITask
{
    ITask() {}
    ~ITask() {}

    virtual bool usesNetwork() const { return false; }
    virtual bool forceContentReport() const { return false; }

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

    virtual bool usesNetwork() const { return true; }
    const api::NetConfiguration& m_netConfig;

    virtual std::string Describe() = 0;
    virtual bool Work( TaskQueue& currentTasks ) = 0;
};

// ---------------------------------------------------------------------------------------------------------------------
struct ContentsReportTask : Warehouse::ITask
{
    static constexpr char Tag[] = "CONTENTS";

    ContentsReportTask( const Warehouse::ContentsReportCallback& callbackOnCompletion )
        : ITask()
        , m_reportCallback( callbackOnCompletion )
    {}

    Warehouse::ContentsReportCallback m_reportCallback;

    virtual std::string Describe() { return std::move( fmt::format( "[{}] creating database contents report", Tag ) ); }
    virtual bool Work( TaskQueue& currentTasks ) override;
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamSliceTask : Warehouse::ITask
{
    static constexpr char Tag[] = "JAMSLICE";

    JamSliceTask( const types::JamCouchID& jamCID, const Warehouse::JamSliceCallback& callbackOnCompletion )
        : ITask()
        , m_jamCID( jamCID )
        , m_reportCallback( callbackOnCompletion )
    {}

    types::JamCouchID               m_jamCID;
    Warehouse::JamSliceCallback     m_reportCallback;

    virtual std::string Describe() { return std::move( fmt::format( "[{}] extracting jam data for [{}]", Tag, m_jamCID.value() ) ); }
    virtual bool Work( TaskQueue& currentTasks ) override;
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamSnapshotTask : Warehouse::INetworkTask
{
    static constexpr char Tag[] = "SNAPSHOT";

    JamSnapshotTask( const api::NetConfiguration& ncfg, const types::JamCouchID& jamCID )
        : Warehouse::INetworkTask( ncfg )
        , m_jamCID( jamCID )
    {}

    // always trigger a contents update after snapping a new jam
    virtual bool forceContentReport() const override { return true; }

    types::JamCouchID m_jamCID;

    virtual std::string Describe() { return std::move( fmt::format( "[{}] fetching Jam snapshot of [{}]", Tag, m_jamCID ) ); }
    virtual bool Work( TaskQueue& currentTasks ) override;
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamPurgeTask : Warehouse::INetworkTask
{
    static constexpr char Tag[] = "PURGE";

    JamPurgeTask( const api::NetConfiguration& ncfg, const types::JamCouchID& jamCID )
        : Warehouse::INetworkTask( ncfg )
        , m_jamCID( jamCID )
    {}

    // always trigger a contents update after wiping out data
    virtual bool forceContentReport() const override { return true; }

    types::JamCouchID m_jamCID;

    virtual std::string Describe() { return std::move( fmt::format( "[{}] deleting all records for [{}]", Tag, m_jamCID ) ); }
    virtual bool Work( TaskQueue& currentTasks ) override;
};


// ---------------------------------------------------------------------------------------------------------------------
struct GetRiffDataTask : Warehouse::INetworkTask
{
    static constexpr char Tag[] = "RIFFDATA";

    GetRiffDataTask( const api::NetConfiguration& ncfg, const types::JamCouchID& jamCID, const std::vector< types::RiffCouchID >& riffCIDs )
        : Warehouse::INetworkTask( ncfg )
        , m_jamCID( jamCID )
        , m_riffCIDs( riffCIDs )
    {}

    types::JamCouchID                 m_jamCID;
    std::vector< types::RiffCouchID > m_riffCIDs;

    virtual std::string Describe() { return std::move( fmt::format( "[{}] pulling {} riff details", Tag, m_riffCIDs.size() ) ); }
    virtual bool Work( TaskQueue& currentTasks ) override;
};

// ---------------------------------------------------------------------------------------------------------------------
struct GetStemData : Warehouse::INetworkTask
{
    static constexpr char Tag[] = "STEMDATA";

    GetStemData( const api::NetConfiguration& ncfg, const types::JamCouchID& jamCID, const std::vector< types::StemCouchID >& stemCIDs )
        : Warehouse::INetworkTask( ncfg )
        , m_jamCID( jamCID )
        , m_stemCIDs( stemCIDs )
    {}

    types::JamCouchID                 m_jamCID;
    std::vector< types::StemCouchID > m_stemCIDs;

    virtual std::string Describe() { return std::move( fmt::format( "[{}] pulling {} stem details", Tag, m_stemCIDs.size() ) ); }
    virtual bool Work( TaskQueue& currentTasks ) override;
};



struct Warehouse::TaskSchedule
{
    TaskQueue   m_taskQueue;
};

namespace sql {

// ---------------------------------------------------------------------------------------------------------------------
namespace jams {

    static constexpr char createTable[] = R"(
        CREATE TABLE IF NOT EXISTS "Jams" (
            "JamCID"        TEXT NOT NULL UNIQUE,
            "PublicName"    TEXT NOT NULL,
            PRIMARY KEY("JamCID")
        );)";
    static constexpr char createIndex_0[] = R"(
        CREATE UNIQUE INDEX IF NOT EXISTS "IndexJam"       ON "Jams" ( "JamCID" );)";

    // -----------------------------------------------------------------------------------------------------------------
    void runInit()
    {
        Warehouse::SqlDB::query<createTable>();

        Warehouse::SqlDB::query<createIndex_0>();
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
        CREATE UNIQUE INDEX IF NOT EXISTS "IndexRiff"       ON "Riffs" ( "RiffCID" );)";
    static constexpr char createIndex_1[] = R"(
        CREATE INDEX        IF NOT EXISTS "IndexOwner"      ON "Riffs" ( "OwnerJamCID" );)";
    static constexpr char createIndex_2[] = R"(
        CREATE INDEX        IF NOT EXISTS "IndexTime"       ON "Riffs" ( "CreationTime" DESC );)";
    static constexpr char createIndex_3[] = R"(
        CREATE INDEX        IF NOT EXISTS "IndexUser"       ON "Riffs" ( "UserName" );)";
    static constexpr char createIndex_4[] = R"(
        CREATE INDEX        IF NOT EXISTS "IndexBPM"        ON "Riffs" ( "BPMrnd" );)";
    static constexpr char createIndex_5[] = R"(
        CREATE INDEX        IF NOT EXISTS "IndexStems"      ON "Riffs" ( "StemCID_1", "StemCID_2", "StemCID_3", "StemCID_4", "StemCID_5", "StemCID_6", "StemCID_7", "StemCID_8" );)";

    // -----------------------------------------------------------------------------------------------------------------
    void runInit()
    {
        Warehouse::SqlDB::query<createTable>();

        Warehouse::SqlDB::query<createIndex_0>();
        Warehouse::SqlDB::query<createIndex_1>();
        Warehouse::SqlDB::query<createIndex_2>();
        Warehouse::SqlDB::query<createIndex_3>();
        Warehouse::SqlDB::query<createIndex_4>();
        Warehouse::SqlDB::query<createIndex_5>();
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
    bool findUnpopulated( types::JamCouchID& jamCID, types::RiffCouchID& riffCID )
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
    bool findUnpopulatedBatch( const types::JamCouchID& jamCID, const int32_t maximumRiffsToFind, std::vector<types::RiffCouchID>& riffCIDs )
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
    void findUniqueJamIDs( std::vector<types::JamCouchID>& jamCIDs )
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
    bool getSingleByID( const types::RiffCouchID& riffCID, endlesss::types::Riff& outRiff )
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
        CREATE UNIQUE INDEX IF NOT EXISTS "IndexStem"       ON "Stems" ( "StemCID" );)";
    static constexpr char createIndex_1[] = R"(
        CREATE INDEX        IF NOT EXISTS "IndexPreset"     ON "Stems" ( "PresetName" );)";
    static constexpr char createIndex_2[] = R"(
        CREATE INDEX        IF NOT EXISTS "IndexTime"       ON "Stems" ( "CreationTime" DESC );)";
    static constexpr char createIndex_3[] = R"(
        CREATE INDEX        IF NOT EXISTS "IndexUser"       ON "Stems" ( "CreatorUserName" );)";
    static constexpr char createIndex_4[] = R"(
        CREATE INDEX        IF NOT EXISTS "IndexBPM"        ON "Stems" ( "BPMrnd" );)";

    // -----------------------------------------------------------------------------------------------------------------
    void runInit()
    {
        Warehouse::SqlDB::query<createTable>();

        Warehouse::SqlDB::query<createIndex_0>();
        Warehouse::SqlDB::query<createIndex_1>();
        Warehouse::SqlDB::query<createIndex_2>();
        Warehouse::SqlDB::query<createIndex_3>();
        Warehouse::SqlDB::query<createIndex_4>();
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
    bool findUnpopulated( types::JamCouchID& jamCID, types::StemCouchID& stemCID )
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
    bool findUnpopulatedBatch( const types::JamCouchID& jamCID, const int32_t maximumStemsToFind, std::vector<types::StemCouchID>& stemCIDs )
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
    bool getSingleStemByID( const types::StemCouchID& stemCID, endlesss::types::Stem& outStem )
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
        REMOVED_ID          = 3             // just .. gone. couch ID no long
    };

    static constexpr char createStemTable[] = R"(
        CREATE TABLE IF NOT EXISTS "StemLedger" (
            "StemCID"       TEXT NOT NULL UNIQUE,
            "Type"          INTEGER,
            "Note"          TEXT NOT NULL,
            PRIMARY KEY("StemCID")
        );)";
    static constexpr char createStemIndex_0[] = R"(
        CREATE UNIQUE INDEX IF NOT EXISTS "IndexStem"       ON "StemLedger" ( "StemCID" );)";

    // -----------------------------------------------------------------------------------------------------------------
    void runInit()
    {
        Warehouse::SqlDB::query<createStemTable>();

        Warehouse::SqlDB::query<createStemIndex_0>();
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
Warehouse::Warehouse( const app::StoragePaths& storagePaths, const api::NetConfiguration& ncfg )
    : m_netConfig( ncfg )
    , m_workerThreadPaused( false )
{
    m_taskSchedule = std::make_unique<TaskSchedule>();

    m_databaseFile = ( fs::path( storagePaths.cacheCommon ) / "warehouse.db3" ).string();
    SqlDB::post_connection_hook = []( sqlite3* db_handle )
    {
    };

    sql::jams::runInit();
    sql::riffs::runInit();
    sql::stems::runInit();
    sql::ledger::runInit();

    m_workerThreadAlive = true;
    m_workerThread      = std::make_unique<std::thread>( &Warehouse::threadWorker, this );
}

// ---------------------------------------------------------------------------------------------------------------------
Warehouse::~Warehouse()
{
    m_workerThreadAlive = false;

    static constexpr char sqlOptimize[] = R"(pragma optimize)";
    SqlDB::query<sqlOptimize>();

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
void Warehouse::syncFromJamCache( const cache::Jams& jamCache )
{
    const auto jamCount = jamCache.getJamCount();
    for ( auto jamI = 0; jamI < jamCount; jamI++ )
    {
        const auto& jamID = jamCache.getDatabaseID( jamI );
        const auto& jamName = jamCache.getDisplayName( jamI );

        static constexpr char _insertOrUpdateJamData[] = R"(
            INSERT OR IGNORE INTO jams( JamCID, PublicName ) VALUES( ?1, ?2 );
        )";

        Warehouse::SqlDB::query<_insertOrUpdateJamData>( jamID.value(), jamName );
    }
}



// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::addJamSnapshot( const types::JamCouchID& jamCouchID )
{
    m_taskSchedule->m_taskQueue.enqueue( std::make_unique<JamSnapshotTask>( m_netConfig, jamCouchID ) );
}

// ---------------------------------------------------------------------------------------------------------------------
void Warehouse::addJamSliceRequest( const types::JamCouchID& jamCouchID, const JamSliceCallback& callbackOnCompletion )
{
    m_taskSchedule->m_taskQueue.enqueue( std::make_unique<JamSliceTask>( jamCouchID, callbackOnCompletion ) );
}

// ---------------------------------------------------------------------------------------------------------------------
bool Warehouse::fetchSingleRiffByID( const endlesss::types::RiffCouchID& riffID, endlesss::types::RiffComplete& result )
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
void Warehouse::requestJamPurge( const types::JamCouchID& jamCouchID )
{
    m_taskSchedule->m_taskQueue.enqueue( std::make_unique<JamPurgeTask>( m_netConfig, jamCouchID ) );
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
    blog::api( "Warehouse::threadWorker start" );

    math::RNG32 rng;

    // mechanism for occasionally getting reports enqueued as work rolls on
    int32_t workCyclesBeforeNewReport = 0;
    const auto tryEnqueueReport = [this, &workCyclesBeforeNewReport]( bool force )
    {
        workCyclesBeforeNewReport--;
        if ( workCyclesBeforeNewReport <= 0 || force )
        {
            m_taskSchedule->m_taskQueue.enqueue( std::make_unique<ContentsReportTask>( m_cbContentsReport ) );
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
        std::this_thread::sleep_for( std::chrono::milliseconds( rng.genInt32(250, 1000) ) );

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

            blog::api( nextTask->Describe() );
            const bool taskOk = nextTask->Work( m_taskSchedule->m_taskQueue );

            if ( !taskOk )
            {
                if ( m_cbWorkUpdate )
                    m_cbWorkUpdate( false, "Paused due to task error" );

                m_workerThreadPaused = true;
                continue;
            }

            if ( nextTask->forceContentReport() )
                tryEnqueueReport( true );
        }
        // go looking for holes to fill
        else
        {
            // scour for empty riffs
            {
                types::JamCouchID owningJamCID;
                types::RiffCouchID emptyRiffCID;
                if ( sql::riffs::findUnpopulated( owningJamCID, emptyRiffCID ) )
                {
                    if ( m_cbWorkUpdate )
                        m_cbWorkUpdate( true, "Finding unpopulated riffs..." );

                    // try find more in that same jam so we can batch effectively
                    std::vector<types::RiffCouchID> emptyRiffs;
                    if ( sql::riffs::findUnpopulatedBatch( owningJamCID, 40, emptyRiffs ) )
                    {
                        // off to riff town
                        m_taskSchedule->m_taskQueue.enqueue( std::make_unique<GetRiffDataTask>( m_netConfig, owningJamCID, emptyRiffs ) );
                        tryEnqueueReport( false );

                        scrapingIsRunning = true;
                        continue;
                    }
                    else
                    {
                        blog::error::api( "we found one empty riff ({}, in jam {}) but failed during batch?", emptyRiffCID, owningJamCID );
                    }
                }
            }
            // .. and then stems
            {
                types::JamCouchID owningJamCID;
                types::StemCouchID emptyStemCID;
                if ( sql::stems::findUnpopulated( owningJamCID, emptyStemCID ) )
                {
                    if ( m_cbWorkUpdate )
                        m_cbWorkUpdate( true, "Finding unpopulated stems..." );

                    // no riffs oh no how about some stems
                    std::vector<types::StemCouchID> emptyStems;
                    if ( sql::stems::findUnpopulatedBatch( owningJamCID, 40, emptyStems ) )
                    {
                        // stem me up
                        m_taskSchedule->m_taskQueue.enqueue( std::make_unique<GetStemData>( m_netConfig, owningJamCID, emptyStems ) );
                        tryEnqueueReport( false );

                        scrapingIsRunning = true;
                        continue;
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

    blog::api( "Warehouse::threadWorker exit" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool JamSnapshotTask::Work( TaskQueue& currentTasks )
{
    blog::api( "[{}] requesting full riff manifest", Tag );

    endlesss::api::JamFullSnapshot jamSnapshot;
    if ( !jamSnapshot.fetch( m_netConfig, m_jamCID ) )
    {
        blog::error::api( "[{}] Failed to fetch snapshot for jam [{}]", Tag, m_jamCID );
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

    blog::api( "[{}] {} online, added {} to Db", Tag, jamSnapshot.rows.size(), countAfterTx - countBeforeTx );
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

    blog::api( "[{}] wiped [{}] from Db", Tag, m_jamCID.value() );
    return true;
}


// ---------------------------------------------------------------------------------------------------------------------
bool GetRiffDataTask::Work( TaskQueue& currentTasks )
{
    blog::api( "[{}] collecting riff data ..", Tag );

    // grab all the riff data
    endlesss::api::RiffDetails riffDetails;
    if ( !riffDetails.fetchBatch( m_netConfig, m_jamCID, m_riffCIDs ) )
    {
        blog::error::api( "[{}] Failed to fetch riff details from jam [{}]", Tag, m_jamCID );
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
                    stemsToValidate.emplace_back( std::move(stemCID) );
                }
            }
        }
    }
    blog::api( "[{}] validating {} stems ..", Tag, stemsToValidate.size() );

    // collect the type data for all the stems; there is a strange situation where some stem IDs turn out to
    // be .. chat messages? so we need to remove those early on
    endlesss::api::StemTypeCheck stemValidation;
    if ( !stemValidation.fetchBatch( m_netConfig, m_jamCID, stemsToValidate ) )
    {
        blog::error::api( "[{}] Failed to validate stem details [{}]", Tag );
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
            blog::api( "[{}] Found stem with a retreival error ({}), ignoring ID [{}]", Tag, stemCheck.error, stemCheck.key );

            sql::ledger::storeStemNote(
                stemCheck.key,
                sql::ledger::StemLedgerType::REMOVED_ID,
                fmt::format( "[{}]", stemCheck.error ) );

            continue;
        }

        // stem was destroyed?
        if ( stemCheck.value.deleted || stemCheck.doc.app_version == 0 )
        {
            uniqueStemCIDs.emplace( stemCheck.key );
            blog::api( "[{}] Found stem that was deleted ({}), ignoring ID [{}]", Tag, stemCheck.error, stemCheck.key );

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
            blog::api( "[{}] Found stem that isn't a stem ({}), ignoring ID [{}]", Tag, stemCheck.doc.type, stemCheck.doc._id );

            sql::ledger::storeStemNote(
                stemCheck.doc._id,
                sql::ledger::StemLedgerType::DAMAGED_REFERENCE,
                fmt::format( "[Ver:{}] Wrong type [{}]", stemCheck.doc.app_version, stemCheck.doc.type ) );

            continue;
        }
        // this stem was damaged and has no audio data
        if ( stemCheck.doc.cdn_attachments.oggAudio.endpoint.empty() )
        {
            uniqueStemCIDs.emplace( stemCheck.doc._id );
            blog::api( "[{}] Found stem that is damaged, ignoring ID [{}]", Tag, stemCheck.doc._id );

            sql::ledger::storeStemNote(
                stemCheck.doc._id,
                sql::ledger::StemLedgerType::MISSING_OGG,
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

    blog::api( "[{}] inserting {} rows of riff detail", Tag, riffDetails.rows.size() );

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
                blog::api( "[{}] Removing stem {} from [{}] as it was marked as invalid", Tag, stemI, riffData.couchID );

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
    blog::api( "[{}] collecting stem data ..", Tag );

    endlesss::api::StemDetails stemDetails;
    if ( !stemDetails.fetchBatch( m_netConfig, m_jamCID, m_stemCIDs ) )
    {
        blog::error::api( "[{}] Failed to fetch stem details from jam [{}]", Tag, m_jamCID );
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

    blog::api( "[{}] inserting {} rows of stem detail", Tag, stemDetails.rows.size() );

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

        Warehouse::SqlDB::query<updateStemDetails>(
            stemData.id.value(),
            unixTime,
            stemData.doc.cdn_attachments.oggAudio.endpoint,
            stemData.doc.cdn_attachments.oggAudio.bucket,
            stemData.doc.cdn_attachments.oggAudio.key,
            stemData.doc.cdn_attachments.oggAudio.mime,
            stemData.doc.cdn_attachments.oggAudio.length,
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
    const int64_t riffCount = sql::riffs::countPopulated( m_jamCID, true );

    Warehouse::JamSlice resultSlice;
    resultSlice.reserve( riffCount );

    static constexpr char _sqlExtractRiffBits[] = R"(
            select RiffCID,CreationTime,UserName,Root,Scale,BPMrnd,StemCID_1,StemCID_2,StemCID_3,StemCID_4,StemCID_5,StemCID_6,StemCID_7,StemCID_8 
            from riffs 
            where OwnerJamCID is ?1 and CreationTime is not null 
            order by CreationTime;
        )";

    auto query = Warehouse::SqlDB::query<_sqlExtractRiffBits>( m_jamCID.value() );

    std::string_view riffCID,
                     username;
    int64_t          timestamp;
    uint32_t         root;
    uint32_t         scale;
    float            bpmrnd;
    std::array< std::string_view, 8 > stemCIDs;

    // for tracking data from previous riff in the list
    robin_hood::unordered_flat_set< std::string_view > previousStemIDs;
    previousStemIDs.reserve( 8 );

    base::spacetime::InSeconds previousRiffTimestamp;
    int8_t                     previousActiveStems = 0;
    bool                       firstRiffInSequence = true;

    while ( query( riffCID,
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
        const uint64_t hashedUsername = CityHash64( username.data(), username.length() );

        const auto contextTimestamp = base::spacetime::InSeconds{ std::chrono::seconds{ timestamp } };

        resultSlice.m_ids.emplace_back( riffCID );
        resultSlice.m_timestamps.emplace_back( contextTimestamp );
        resultSlice.m_userhash.emplace_back( hashedUsername );
        resultSlice.m_roots.emplace_back( root );
        resultSlice.m_scales.emplace_back( scale );
        resultSlice.m_bpms.emplace_back( bpmrnd );

        int8_t liveStems = 0;
        int32_t newStems = 0;
        for ( const auto& stemCID : stemCIDs )
        {
            if ( !stemCID.empty() )
                liveStems++;
            if ( !previousStemIDs.contains( stemCID ) )
                newStems++;
        }

        // first riff reports no deltas
        if ( firstRiffInSequence )
        {
            resultSlice.m_deltaSeconds.push_back( 0 );
            resultSlice.m_deltaStem.push_back( 0 );
        }
        // compute deltas from last riff
        else
        {
            resultSlice.m_deltaSeconds.push_back( (int32_t) (contextTimestamp - previousRiffTimestamp).count() );
            resultSlice.m_deltaStem.push_back( std::max( newStems, std::abs(liveStems - previousActiveStems) ) );
        }
        firstRiffInSequence = false;

        // stash our current state for deltas
        previousRiffTimestamp = contextTimestamp;
        previousActiveStems = liveStems;

        // keep unordered set of stem IDs
        previousStemIDs.clear();
        for ( const auto& stemCID : stemCIDs )
            previousStemIDs.emplace( stemCID );
    }

    if ( m_reportCallback )
        m_reportCallback( m_jamCID, resultSlice );

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
bool ContentsReportTask::Work( TaskQueue& currentTasks )
{
    Warehouse::ContentsReport reportResult;

    std::vector<types::JamCouchID> uniqueJamCIDs;
    sql::riffs::findUniqueJamIDs( uniqueJamCIDs );
    
    for ( const auto& jamCID : uniqueJamCIDs )
    {
        reportResult.m_jamCouchIDs.emplace_back( jamCID );

        reportResult.m_populatedRiffs.emplace_back( sql::riffs::countPopulated( jamCID, true ) );
        reportResult.m_unpopulatedRiffs.emplace_back( sql::riffs::countPopulated( jamCID, false ) );

        reportResult.m_populatedStems.emplace_back( sql::stems::countPopulated( jamCID, true ) );
        reportResult.m_unpopulatedStems.emplace_back( sql::stems::countPopulated( jamCID, false ) );
    }

    if ( m_reportCallback )
        m_reportCallback( reportResult );

    return true;
}

} // namespace endlesss
