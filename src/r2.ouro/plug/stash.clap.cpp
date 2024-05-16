//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"

#include "platform_folders.h"

#include "base/construction.h"
#include "base/text.transform.h"
#include "plug/stash.clap.h"
#include "plug/plug.clap.h"
#include "spacetime/moment.h"
#include "sys/dynlib.h"

#include "clap/clap.h"
#include "clap/version.h"


namespace plug {
namespace stash {

// to be able to ask a plugin about their available processing ports (so we can tell if we want to support it) we
// need to initialise it briefly so we can scrape the extensions; to do so requires a clap_host, so we create a 
// virtual "null" host that can't really do anything useful - these callbacks are used to build it
namespace host_null {

static const void* clapGetExtension( const struct clap_host* host, const char* extension_id ) noexcept
{
    return nullptr;
}

static void clapRequestRestart( const struct clap_host* host ) noexcept
{
}

static void clapRequestProcess( const struct clap_host* host ) noexcept
{
}

static void clapRequestCallback( const struct clap_host* host ) noexcept
{
}

} // namespace null

// ---------------------------------------------------------------------------------------------------------------------
CLAP::CLAP()
    : m_asyncPopulationComplete( false )
    , m_asyncAllProcessingComplete( false )
{
}

// ---------------------------------------------------------------------------------------------------------------------
CLAP::~CLAP()
{
}

// ---------------------------------------------------------------------------------------------------------------------
CLAP::Instance CLAP::createAndPopulateAsync( tf::Executor& taskExecutor )
{
    Instance result = base::protected_make_unique<CLAP>();
    result->beginPopulateAsync( taskExecutor );

    return std::move( result );
}

// ---------------------------------------------------------------------------------------------------------------------
// load a file in chunks and hash the contents to give us a unique hash value
//
uint64_t CLAP::hashfile( const fs::path& filepath )
{
#if OURO_PLATFORM_WIN
    FILE* fpHandle = _wfopen( filepath.wstring().c_str(), L"rbS" ); // S = sequential access from disk
#else
    FILE* fpHandle = fopen( filepath.string().c_str(), "rb" );
#endif
    if ( fpHandle == nullptr )
    {
        blog::error::plug( FMTX( "[CLAP] hashing [{}] failed to open file" ), filepath.string() );
        return 0;
    }
    // ensure file always closed on scope exit
    absl::Cleanup closeFileOnExit = [=] { fclose( fpHandle ); };

    // seed the hashes with a versioning number so we can invalidate any stored seeds easily
    komihash_stream_t ctx;
    komihash_stream_init( &ctx, cHashingVersionSeed );

    // read in and hash chunks of the file
    std::array< uint8_t, 1024 >  fileScanBuffer;
    for ( ;; )
    {
        std::size_t fileScanBytesRead = fread( fileScanBuffer.data(), sizeof( uint8_t ), fileScanBuffer.size(), fpHandle );
        if ( fileScanBytesRead <= 0 )
            break;

        // incremental hash update
        komihash_stream_update( &ctx, fileScanBuffer.data(), sizeof( uint8_t ) * fileScanBytesRead );
    }

    return komihash_stream_final( &ctx );
}

// ---------------------------------------------------------------------------------------------------------------------
void CLAP::beginPopulateAsync( tf::Executor& taskExecutor )
{
    absl::InlinedVector< fs::path, 3 > clapSearchPaths;

    blog::plug( FMTX( "[CLAP] version {}.{}.{}" ), CLAP_VERSION_MAJOR, CLAP_VERSION_MINOR, CLAP_VERSION_REVISION );

    // first phase is synchronous, we collect up a list of all the plugin files from the 
    // known CLAP install locations - then kick off background tasks to actually analyse that manifest
    {
        // per CLAP documentation:
        // ---------------------------------------------
        // CLAP plugins standard search path:
        //
        // Linux
        //   - ~/.clap
        //   - /usr/lib/clap
        //
        // Windows
        //   - %COMMONPROGRAMFILES%\CLAP
        //   - %LOCALAPPDATA%\Programs\Common\CLAP
        //
        // MacOS
        //   - /Library/Audio/Plug-Ins/CLAP
        //   - ~/Library/Audio/Plug-Ins/CLAP
        //
#if OURO_PLATFORM_LINUX
        {

        }
#endif // OURO_PLATFORM_LINUX
#if OURO_PLATFORM_WIN
        {
            const fs::path windowsProgramFilesPath{ sago::GetWindowsProgramFilesCommon() };
            const fs::path localAppDataPath{ sago::getCacheDir() };

            clapSearchPaths.push_back( windowsProgramFilesPath / "CLAP" );
            clapSearchPaths.push_back( localAppDataPath / "Programs" / "Common" / "CLAP" );
        }
#endif // OURO_PLATFORM_WIN
#if OURO_PLATFORM_OSX
        {

        }
#endif // OURO_PLATFORM_OSX


        // go digging in the configured paths
        for ( const auto& searchPath : clapSearchPaths )
        {
            std::error_code osError;

            // bail if the path doesn't exist, recursive_directory_iterator will throw up otherwise
            if ( !fs::exists( searchPath ) )
            {
                blog::plug( FMTX( "[CLAP] search path: {} (does not exist)" ), searchPath.string() );
                continue;
            }

            blog::plug( FMTX( "[CLAP] search path: {}" ), searchPath.string() );

            // setup to iterate through all files and folders under this path
            auto fileIterator = fs::recursive_directory_iterator( searchPath, std::filesystem::directory_options::skip_permission_denied, osError );
            if ( osError )
            {
                blog::error::plug( FMTX( "[CLAP] error ({}) starting file iteration, skipping path" ), osError.message() );
                break;
            }

            // find files to interrogate
            for ( auto fIt = fs::begin( fileIterator ); fIt != fs::end( fileIterator ); fIt = fIt.increment( osError ) )
            {
                if ( osError )
                {
                    blog::error::plug( FMTX( "[CLAP] error ({}) during file iteration, skipping path" ), osError.message() );
                    break;
                }

                if ( fIt->is_directory() )
                    continue;

                const auto& entryFullFilename = fIt->path();
                m_pluginFullPaths.push_back( entryFullFilename );
                m_pluginFileHashes.push_back( 0 );
            }
        }
        blog::plug( FMTX( "[CLAP] found {} plugins" ), m_pluginFullPaths.size() );
        ABSL_ASSERT( m_pluginFullPaths.size() == m_pluginFileHashes.size() );
    }

    tf::Taskflow stashProcessingTaskflow;

    // do the rest of the interrogation in a background thread, anything wanting to know about plugins
    // will need to check m_pluginPopulationRunning via isBusy()
    tf::Task taskProcessing = stashProcessingTaskflow.emplace( [this]( tf::Subflow& subflow )
        {
            spacetime::ScopedTimer populateTiming( "CLAP plugin stash interrogation" );

            m_knownPlugins.reserve( m_pluginFullPaths.size() * 2 );
            m_knownPluginsVerified.reserve( m_pluginFullPaths.size() * 2 );

            m_pluginFilesHashed = 0;
            for ( std::size_t pluginFileIndex = 0; pluginFileIndex < m_pluginFullPaths.size(); pluginFileIndex ++ )
            {
                const auto& pluginFile = m_pluginFullPaths[pluginFileIndex];

                // kick off a background task to compute the plugin file contents hash value, used to 
                // check if it has changed since we saw it last
                tf::Task subtaskHashing = subflow.emplace( [this, pluginFileIndex, pluginFile]()
                    {
                        m_pluginFileHashes[pluginFileIndex] = hashfile( pluginFile );
                        m_pluginFilesHashed++;
                    });

                auto clapLib = sys::DynLib::loadFromFile( pluginFile );
                if ( clapLib.ok() )
                {
                    // fetch the entrypoint or die trying
                    const clap_plugin_entry* clapEntry = clapLib.value()->resolve< const clap_plugin_entry >( "clap_entry" );
                    if ( clapEntry == nullptr )
                    {
                        blog::error::plug( FMTX( "[CLAP] unable to find clap_entry bootstrap in ({})" ), pluginFile.string() );
                        continue;
                    }

                    // init() the plugin so we can fetch a factory instance to interrogate it for details; this should
                    // be a fairly lightweight call, according to the docs
                    if ( clapEntry->init( pluginFile.string().c_str() ) )
                    {
                        const clap_plugin_factory* clapFactory = static_cast<const clap_plugin_factory*>(clapEntry->get_factory( CLAP_PLUGIN_FACTORY_ID ));

                        // add KnownPlugin instances for each internal plugin found inside the library
                        const uint32_t clapPluginCount = clapFactory->get_plugin_count( clapFactory );
                        for ( uint32_t clapPluginIndex = 0; clapPluginIndex < clapPluginCount; clapPluginIndex++ )
                        {
                            const clap_plugin_descriptor_t* clapPluginDescriptor = clapFactory->get_plugin_descriptor( clapFactory, clapPluginIndex );

                            // first things first - check for basic compatibility with our CLAP version. if this fails, no point in continuing.
                            const bool bIsCompatible = clap_version_is_compatible( clapPluginDescriptor->clap_version );
                            if ( !bIsCompatible )
                            {
                                // not an error as such
                                blog::plug( FMTX( "[CLAP] {:64} reports incompatiblity with this CLAP version" ), clapPluginDescriptor->id );
                                continue;
                            }

                            // iterate the features list; this tells us the type and capabilities of the plugin. we are looking for
                            // the audio-effect plugins specifically, and keeping note of the ones that explicitly declare stereo support
                            // in case there is any confusion over ports later on
                            bool featureIsAudioEffect       = false;
                            bool featureDeclaresStereo      = false;
                            auto featureIteration           = clapPluginDescriptor->features;
                            while ( *featureIteration != nullptr )
                            {
                                const std::string_view featureString{ *featureIteration };

                                featureIsAudioEffect  |= featureString.find( CLAP_PLUGIN_FEATURE_AUDIO_EFFECT   ) != std::string::npos;
                                featureDeclaresStereo |= featureString.find( CLAP_PLUGIN_FEATURE_STEREO         ) != std::string::npos;

                                ++featureIteration;
                            }

                            // plugin has been deemed .. (probably) acceptable
                            if ( featureIsAudioEffect )
                            {
                                plug::KnownPlugin::Instance pluginRecord = std::make_unique<plug::KnownPlugin>( plug::Systems::CLAP );

                                // transfer over all the details
                                pluginRecord->m_fullLibraryPath = pluginFile;
                                pluginRecord->m_exteriorIndex   = plug::ExteriorIndex{ static_cast< int64_t >( pluginFileIndex ) };
                                pluginRecord->m_interiorIndex   = clapPluginIndex;
                                pluginRecord->m_uid             = clapPluginDescriptor->id;
                                pluginRecord->m_name            = clapPluginDescriptor->name;
                                pluginRecord->m_vendor          = clapPluginDescriptor->vendor;
                                pluginRecord->m_version         = clapPluginDescriptor->version;
                                pluginRecord->m_sortable        = fmt::format( FMTX("{} {} {}"),
                                                                    clapPluginDescriptor->vendor,
                                                                    clapPluginDescriptor->name,
                                                                    clapPluginDescriptor->version );

                                blog::debug::plug( FMTX( "[CLAP] {:64} | {}" ), pluginRecord->m_uid, pluginRecord->m_sortable );

                                // keep any notes from other feature declarations
                                if ( featureDeclaresStereo )
                                    pluginRecord->m_flags      |= plug::KnownPlugin::SF_ExplicitStereoSupport;

                                // store a lookup from the UID to the index into the plugin record array
                                const plug::KnownPluginIndex knownPluginIndex = plug::KnownPluginIndex{ static_cast< int64_t >( m_knownPlugins.size() ) };
                                m_knownPluginLookupByUID.emplace( pluginRecord->m_uid, knownPluginIndex );

                                m_knownPlugins.emplace_back( std::move( pluginRecord ) );
                                m_knownPluginsVerified.emplace_back( false );
                            }
                        }

                        // symmetric shut-down
                        clapEntry->deinit();
                    }
                    else
                    {
                        blog::error::plug( FMTX( "[CLAP] init() failed for ({})" ), pluginFile.string() );
                    }
                }
            }

            m_asyncPopulationComplete = true;
        });

    // load and analyse the plugins in parallel; find ones that we can safely support in the effects chain - stereo in, stereo out
    // this then results in a list of "known possible" effects
    tf::Task taskFiltering = stashProcessingTaskflow.emplace( [this]( tf::Subflow& subflow )
        {
            const int64_t knownPluginCount = static_cast< int64_t >( m_knownPlugins.size() );
            for ( int64_t sI = 0; sI < knownPluginCount; sI ++ )
            {
                subflow.emplace( [this, sI]()
                    {
                        const KnownPluginIndex knownPluginIndex{ sI };

                        const absl::Status pluginValidStatus = checkKnownPluginIsValidForEffects( knownPluginIndex );
                        if ( pluginValidStatus.ok() )
                        {
                            m_knownPluginsVerified[sI] = true;
                        }
                        else
                        {
                            m_knownPluginsVerified[sI] = false;
                            blog::debug::plug( FMTX( "[CLAP] {} not usable as effect plugin" ), getKnownPluginAtIndex( knownPluginIndex ).m_uid );
                        }
                    });
            }
        });
    taskFiltering.succeed( taskProcessing );

    // take the list of known-good plugins and sort them by name into an index list
    tf::Task taskSorting = stashProcessingTaskflow.emplace( [this]()
        {
            m_knownPluginsVerifiedSorted.clear();

            // create a list of all the valid indices we could display
            const int64_t knownPluginCount = static_cast< int64_t >( m_knownPlugins.size() );
            for ( int64_t sI = 0; sI < knownPluginCount; sI ++ )
            {
                if ( m_knownPluginsVerified[sI] )
                    m_knownPluginsVerifiedSorted.push_back( KnownPluginIndex{ sI } );
            }

            std::sort( m_knownPluginsVerifiedSorted.begin(), m_knownPluginsVerifiedSorted.end(),
                [this]( const KnownPluginIndex lhs, const KnownPluginIndex rhs ) -> bool
                {
                    return base::StrToLwrExt( m_knownPlugins[lhs.get()]->m_sortable ) < base::StrToLwrExt( m_knownPlugins[rhs.get()]->m_sortable );
                });

            m_asyncAllProcessingComplete = true;
        });
    taskSorting.succeed( taskFiltering );

    taskExecutor.run( std::move( stashProcessingTaskflow ) );
}

// ---------------------------------------------------------------------------------------------------------------------
absl::Status CLAP::checkKnownPluginIsValidForEffects( const plug::KnownPluginIndex& index ) const
{
    // create a null/dummy host to use for temporarily loading the plugin
    clap_host virtualHost
    {
        CLAP_VERSION,
        nullptr,
        "virtual",
        OURO_FRAMEWORK_CREDIT,
        OURO_FRAMEWORK_URL,
        OURO_FRAMEWORK_VERSION,
        host_null::clapGetExtension,
        host_null::clapRequestRestart,
        host_null::clapRequestProcess,
        host_null::clapRequestCallback
    };

    // loading the plugin will do the initial port analysis to see if its something we can use
    auto runtimeLoadStatus = plug::runtime::CLAP::load( *(m_knownPlugins[index.get()]), &virtualHost);
    if ( runtimeLoadStatus.ok() )
    {
        if ( runtimeLoadStatus.value()->isValidEffectPlugin() )
            return absl::OkStatus();

        return absl::InternalError( "plugin not discernible as valid effect processor" );
    }
    return runtimeLoadStatus.status();
}

} // namespace stash
} // namespace plug
