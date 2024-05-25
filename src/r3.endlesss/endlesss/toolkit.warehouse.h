//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

#include "base/text.h"
#include "base/operations.h"
#include "spacetime/chronicle.h"
#include "endlesss/core.constants.h"
#include "endlesss/ids.h"
#include "endlesss/api.h"

namespace app { struct StoragePaths; }

namespace endlesss {

namespace cache { struct Jams; }

namespace toolkit {

// ---------------------------------------------------------------------------------------------------------------------
//
struct Warehouse
{
    using Instance = std::unique_ptr< Warehouse >;

    struct _change_index {};
    using ChangeIndex = base::id::Simple<_change_index, uint32_t, 1, 0>;

    struct ContentsReport
    {
        std::vector< types::JamCouchID >    m_jamCouchIDs;
        std::vector< int64_t >              m_populatedRiffs;
        std::vector< int64_t >              m_unpopulatedRiffs;
        std::vector< int64_t >              m_populatedStems;
        std::vector< int64_t >              m_unpopulatedStems;
        std::vector< bool >                 m_awaitingInitialSync;  // initial data set has not yet arrived, consider in flux

        int64_t m_totalPopulatedRiffs     = 0;
        int64_t m_totalUnpopulatedRiffs   = 0;
        int64_t m_totalPopulatedStems     = 0;
        int64_t m_totalUnpopulatedStems   = 0;
    };
    using ContentsReportCallback = std::function<void( const ContentsReport& report )>;

    // when there is a failure to validate a stem during riff fetching, the stem is cut out of the resulting database
    // and an entry is logged in a ledger, stashing the stem ID with any notes / tags about why we ignore it
    enum class StemLedgerType : int32_t
    {
        MISSING_OGG         = 1,            // ogg data vanished; this is mostly due to the broken beta that went out
        DAMAGED_REFERENCE   = 2,            // sometimes chat messages (?!) or other riffs (??) seem to have been stored as stem CouchIDs 
        REMOVED_ID          = 3             // just .. gone. couch ID not found. unrecoverable
    };

    static const std::string_view getStemLedgerTypeAsString( const StemLedgerType ledgerType )
    {
        switch ( ledgerType )
        {
            case StemLedgerType::MISSING_OGG:       return "MISSING_OGG";
            case StemLedgerType::DAMAGED_REFERENCE: return "DAMAGED_REFERENCE";
            case StemLedgerType::REMOVED_ID:        return "REMOVED_ID";
            default:
                return "UNKNOWN";
        }
    }

    // configurable modes for dealing with the cursed duplicate Riff ID values that can happen when people Remix stuff
    // into their private jams. These riff IDs have new owner-jam IDs but they are the same Riff ID from the original 
    // jam, which isn't valid in our current sql data model where the Riff ID is a unique key
    // .. so we have some options
    enum class RiffIDConflictHandling : int32_t
    {
        IgnoreAll,                  // first riff that arrived wins, we ignore all conflicts
        Overwrite,                  // whatever is being synced wins, we overwrite the owner-jam ID with whatever the new one is
        OverwriteExceptPersonal     // the default; we do 'Overwrite' behaviour except for in your personal jam where it is ignored - 
                                    // this means all non-personal jams are kept intact as others would see them, but your personal jam
                                    // may be missing things. users can switch to Overwrite and sync their personal to have all those
                                    // duplicate riffs restored into the personal jam timeline
    };
    
    static const std::string_view getRiffIDConflictHandlingTooltip( const RiffIDConflictHandling conflictHandling )
    {
        switch ( conflictHandling )
        {
        case RiffIDConflictHandling::IgnoreAll:                 return "Ignore all conflicts; any duplicate riff arriving will be ignored, leaving the original that arrived first.";
        case RiffIDConflictHandling::Overwrite:                 return "Synchronising a jam containing any duplicates will give that jam ownership of them. If you then sync your personal jam, it will take ownership of them.";
        case RiffIDConflictHandling::OverwriteExceptPersonal:   return "Like 'Overwrite' but we never let the personal jam have ownership.\nThis is the default to try and ensure the most visible, public data - the original jams - have the most complete data";
        default:
            return "UNKNOWN";
        }
    }


    // SoA extraction of a set of riff data; this is the data returned to the client app when a view on a jam is requested
    struct JamSlice
    {
        using StemUserHashes = std::array< uint64_t, 8 >;

        DECLARE_NO_COPY_NO_MOVE( JamSlice );

        JamSlice() = delete;
        JamSlice( const types::JamCouchID& jamID, const size_t elementsToReserve )
        {
            reserve( elementsToReserve );
        }

        // per-riff information
        std::vector< types::RiffCouchID >           m_ids;
        std::vector< spacetime::InSeconds >         m_timestamps;
        std::vector< uint64_t >                     m_userhash;
        std::vector< uint8_t >                      m_roots;
        std::vector< uint8_t >                      m_scales;
        std::vector< float >                        m_bpms;

        // hashed stem-owner username, per stem in each riff
        std::vector< StemUserHashes >               m_stemUserHashes;

        // sequential adjacency information, based on previously (index - 1) seen riff
        std::vector< int32_t >                      m_deltaSeconds;
        std::vector< int8_t >                       m_deltaStem;

    protected:

        inline void reserve( const size_t elements )
        {
            m_ids.reserve( elements );
            m_timestamps.reserve( elements );
            m_userhash.reserve( elements );
            m_roots.reserve( elements );
            m_scales.reserve( elements );
            m_bpms.reserve( elements );

            m_stemUserHashes.reserve( elements );

            m_deltaSeconds.reserve( elements );
            m_deltaStem.reserve( elements );

            std::size_t memoryUsageEstimation = 0;
            {
                memoryUsageEstimation += sizeof( types::RiffCouchID ) * elements;
                memoryUsageEstimation += sizeof( spacetime::InSeconds ) * elements;
                memoryUsageEstimation += sizeof( uint64_t ) * elements;
                memoryUsageEstimation += sizeof( uint8_t ) * elements;
                memoryUsageEstimation += sizeof( uint8_t ) * elements;
                memoryUsageEstimation += sizeof( float ) * elements;

                memoryUsageEstimation += sizeof( uint64_t ) * elements * 8;

                memoryUsageEstimation += sizeof( int32_t ) * elements;
                memoryUsageEstimation += sizeof( uint8_t ) * elements;
            }

            blog::app( FMTX( "JamSlice SOA array allocation {}" ), base::humaniseByteSize( "~", memoryUsageEstimation ) );
        }
    };
    using JamSlicePtr = std::unique_ptr<JamSlice>;
    using JamSliceCallback = std::function<void( const types::JamCouchID& jamCouchID, JamSlicePtr&& resultSlice )>;


    struct ITask;
    struct INetworkTask;

    using WorkUpdateCallback    = std::function<void( const bool tasksRunning, const std::string& currentTask ) >;

    using TagBatchingCallback   = std::function<void( bool bBatchUpdateBegun )>;
    using TagUpdateCallback     = std::function<void( const endlesss::types::RiffTag& tagData )>;
    using TagRemovedCallback    = std::function<void( const endlesss::types::RiffCouchID& tagRiffID )>;


    Warehouse( const app::StoragePaths& storagePaths, api::NetConfiguration::Shared& networkConfig, base::EventBusClient eventBus );
    ~Warehouse();

    static std::string  m_databaseFile;
    using SqlDB = sqlite::Database<m_databaseFile>;

    // -----------------------------------------------------------------------------------------------------------------


    // inject callbacks to client code that are used to report what the warehouse is currently doing, or to receive
    // larger data packets like the full contents report
    void setCallbackWorkReport( const WorkUpdateCallback& cb );
    void setCallbackContentsReport( const ContentsReportCallback& cb );

    void setCallbackTagUpdate( const TagUpdateCallback& cbUpdate, const TagBatchingCallback& cbBatch );
    void setCallbackTagRemoved( const TagRemovedCallback& cb );

    void clearAllCallbacks();


    // manually enqueue a contents report event, refreshing anything that responds to that callback
    void requestContentsReport();


    // -----------------------------------------------------------------------------------------------------------------
    // Jam Naming

    // upsert a jamID (band####) -> display name record
    void upsertSingleJamIDToName( const endlesss::types::JamCouchID& jamCID, const std::string& displayName );

    // upsert all jamID -> display name records from the given jam cache, keeping the warehouse name lookup fresh
    void upsertJamDictionaryFromCache( const cache::Jams& jamCache );

    // upsert all jamID -> display name records from the Band Name Service record, similar to above
    void upsertJamDictionaryFromBNS( const config::endlesss::BandNameService& bnsData );

    // fetch a list of all the JamID -> display name rows we have as a lookup table
    void extractJamDictionary( types::JamIDToNameMap& jamDictionary ) const;


    // -----------------------------------------------------------------------------------------------------------------
    // Jam Synchronisation

    static constexpr base::OperationVariant OV_AddOrUpdateJamSnapshot{ 0xF0 };

    // insert a jam ID into the warehouse to be queried and filled
    ouro_nodiscard base::OperationID addOrUpdateJamSnapshot( const types::JamCouchID& jamCouchID );

    // fetch the full stack of data for a given jam
    void addJamSliceRequest( const types::JamCouchID& jamCouchID, const JamSliceCallback& callbackOnCompletion );

    // erase the given jam from the warehouse database entirely
    void requestJamPurge( const types::JamCouchID& jamCouchID );

    // erase all unfilled riffs, effectively cutting short any in-progress sync
    void requestJamSyncAbort( const types::JamCouchID& jamCouchID );



    // -----------------------------------------------------------------------------------------------------------------
    // Jam Export / Import

    static constexpr base::OperationVariant OV_ExportAction{ 0xF1 };
    static constexpr base::OperationVariant OV_ImportAction{ 0xF2 };

    ouro_nodiscard base::OperationID requestJamDataExport( const types::JamCouchID& jamCouchID, const fs::path exportFolder, std::string_view jamTitle );
    ouro_nodiscard base::OperationID requestJamDataImport( const fs::path pathToData );

    // produce a common format filename for exported things - database stuff, stem archives, etc
    ouro_nodiscard static std::string createExportFilenameForJam(
        const types::JamCouchID& jamCouchID,
        const std::string_view jamName,
        const std::string_view fileExtension );



    // -----------------------------------------------------------------------------------------------------------------
    // Riffs

    // instead of hitting the Endlesss network, the warehouse may be able to fill in all the data required to 
    // bring a riff online; returns true if that was the case
    bool fetchSingleRiffByID( const endlesss::types::RiffCouchID& riffID, endlesss::types::RiffComplete& result ) const;

    // change a specific stem entry in a riff, also creating an empty stem record if one doesn't exist;
    // used to modify existing riff structures if we find invalid mismatches from the server data
    bool patchRiffStemRecord(
        const types::JamCouchID& jamCouchID,
        const endlesss::types::RiffCouchID& riffID,
        const int32_t stemIndex,    // 0-base stem index to modify
        const endlesss::types::StemCouchID& newStemID );

    // used to find the ranges of rounded BPMs given scale/root choices, and how many riffs are associated with that BPM
    // returns size of the populated bpmCounts vector
    struct BPMCountTuple
    {
        uint32_t    m_BPM;
        uint32_t    m_riffCount;
        uint32_t    m_jamCount;
    };
    enum class BPMCountSort
    {
        ByBPM,
        ByCount
    };
    std::size_t filterRiffsByBPM( const endlesss::constants::RootScalePairs& keySearchPairs, const BPMCountSort sortOn, std::vector< BPMCountTuple >& bpmCounts ) const;


    bool fetchRandomRiffBySeed( const endlesss::constants::RootScalePairs& keySearchPairs, const uint32_t BPM, const int32_t seedValue, endlesss::types::RiffComplete& result ) const;

    // get the last known committed riff in the given jam, return the timestamp
    uint32_t getOldestRiffUnixTimestampFromJam( const types::JamCouchID& jamCouchID ) const;


    // -----------------------------------------------------------------------------------------------------------------
    // Stems

    // given N stems, return a matching vector of origin jam IDs for each; if no jam ID can be determined, an empty ID is stored
    bool batchFindJamIDForStem( const endlesss::types::StemCouchIDs& stems, endlesss::types::JamCouchIDs& result ) const;

    // populate the output vector with every stem associated with a jam (for example, for precatching them all into the cache)
    // also returns the summed FileLength values, in case disk storage estimation or reporting is useful
    bool fetchAllStemsForJam( const types::JamCouchID& jamCouchID, endlesss::types::StemCouchIDs& result, std::size_t& estimatedTotalFileSize ) const;

    // resolve a single stem data block from the database, if we can find it; returns false if we didn't
    bool fetchSingleStemByID( const types::StemCouchID& stemCouchID, endlesss::types::Stem& result ) const;

    // do you want all the stems? all of them? damn son alright
    bool fetchAllStems( endlesss::types::StemCouchIDs& result, std::size_t& estimatedTotalFileSize ) const;


    // see if we have any notes for a stem ID (if it was removed from the database during a sync for some reason)
    // returns false if we have no record for this stem ID
    bool getNoteTypeForStem( const types::StemCouchID& stemCID, StemLedgerType& typeResult );



    // -----------------------------------------------------------------------------------------------------------------
    // Tags

    // add or update a jam:riff tag
    void upsertTag( const endlesss::types::RiffTag& tag );

    // delete the tag from the database
    void removeTag( const endlesss::types::RiffTag& tag );

    // returns true if the given riff has tag data, optionally also returning the tag data if a structure is passed in
    bool isRiffTagged( const endlesss::types::RiffCouchID& riffID, endlesss::types::RiffTag* tagOutput = nullptr ) const;

    // get the current set of tags for a jam; returns the size of outputTags on return
    std::size_t fetchTagsForJam( const endlesss::types::JamCouchID& jamCID, std::vector<endlesss::types::RiffTag>& outputTags ) const;

    // set all the specific tag data in a single transaction
    void batchUpdateTags( const std::vector<endlesss::types::RiffTag>& inputTags );

    // delete all tags for a jam as a batch operation
    void batchRemoveAllTags( const endlesss::types::JamCouchID& jamCID );



    // -----------------------------------------------------------------------------------------------------------------
    // Jam Virtualisation
    // a way to allow us to create arbitrarily defined riffs from scratch; eg. for procedural generation experiments
    // this uses a bespoke magic-named jam in the database where we can horse about and write custom data / purge stuff.
    // these can be requested and interacted with by the pipeline code as if they were "real" riffs
    //

    static constexpr std::string_view cVirtualJamName = "virtual_jam";

    // injects a new virtual riff, returning the Identity information to be able to enqueue it (should you wish)
    endlesss::types::RiffIdentity createNewVirtualRiff( const endlesss::types::VirtualRiff& vriff );

    // if the given riffID is marked as "virtual", a result of the createNewVirtualRiff() fn
    static bool isRiffIDVirtual( const endlesss::types::RiffCouchID& riffID );

    void clearOutVirtualJamStorage();


    // -----------------------------------------------------------------------------------------------------------------

    ouro_nodiscard ChangeIndex getChangeIndexForJam( const endlesss::types::JamCouchID& jamID ) const;

    // control background task processing; pausing will stop any new work from being generated
    void workerTogglePause();
    ouro_nodiscard bool workerIsPaused() const { return m_workerThreadPaused; }

    // passing in the NetConfiguration for API access to Endlesss is optional; users should not enqueue tasks
    // that require it if it isn't present (and tasks will check and bail in error)
    ouro_nodiscard bool hasFullEndlesssNetworkAccess() const { return m_networkConfiguration->hasAccess( api::NetConfiguration::Access::Authenticated ); }


    // -----------------------------------------------------------------------------------------------------------------
    // conflict handling controls

    void setCurrentRiffIDConflictHandling( RiffIDConflictHandling handling ) { m_riffIDConflictHandling = handling; }
    ouro_nodiscard RiffIDConflictHandling getCurrentRiffIDConflictHandling() const { return m_riffIDConflictHandling; }


protected:

    using ChangeIndexMap = absl::flat_hash_map< ::endlesss::types::JamCouchID, ChangeIndex >;

    friend ITask;
    struct TaskSchedule;

    void threadWorker();

    void incrementChangeIndexForJam( const ::endlesss::types::JamCouchID& jamID );


    // handle riff tag actions, do database operations to add/remove as requested
    void event_RiffTagAction( const events::RiffTagAction* eventData );
    base::EventListenerID                   m_eventLID_RiffTagAction = base::EventListenerID::invalid();


    api::NetConfiguration::Shared           m_networkConfiguration;
    base::EventBusClient                    m_eventBusClient;

    std::unique_ptr<TaskSchedule>           m_taskSchedule;
    std::unique_ptr<TaskSchedule>           m_taskSchedulePriority;     // parallel queue used to stage tasks that should be run before the default queue gets a look in

    ChangeIndexMap                          m_changeIndexMap;

    WorkUpdateCallback                      m_cbWorkUpdate              = nullptr;
    WorkUpdateCallback                      m_cbWorkUpdateToInstall     = nullptr;
    ContentsReportCallback                  m_cbContentsReport          = nullptr;
    ContentsReportCallback                  m_cbContentsReportToInstall = nullptr;
    TagUpdateCallback                       m_cbTagUpdate;
    TagBatchingCallback                     m_cbTagBatching;
    TagRemovedCallback                      m_cbTagRemoved;
    std::mutex                              m_cbMutex;

    std::unique_ptr<std::thread>            m_workerThread;
    std::atomic_bool                        m_workerThreadAlive;
    std::atomic_bool                        m_workerThreadPaused;

    RiffIDConflictHandling                  m_riffIDConflictHandling = RiffIDConflictHandling::OverwriteExceptPersonal;
};

} // namespace toolkit
} // namespace endlesss
