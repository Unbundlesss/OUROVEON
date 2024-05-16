//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

// use c++20 concepts to better verify config templates at compile time
#define OURO_CONFIG_CONCEPTS 1

namespace config {

// ---------------------------------------------------------------------------------------------------------------------
struct IPathProvider
{
    static constexpr auto cOuroveonRootName = "OUROVEON";

    enum class PathFor
    {
        SharedData,         // shared data directory root common for all apps (eg. fonts)

        SharedConfig,       // directory for storing shared core configuration (eg. audio driver choice)
        PerAppConfig,       // directory for storing per-app data (eg. current window state, app-specific blobs)
    };

    virtual fs::path getPath( const PathFor p ) const = 0;
};

// ---------------------------------------------------------------------------------------------------------------------

#if OURO_CONFIG_CONCEPTS

template<typename T>
concept Serializeable = requires( T& t )
{
    { T::StoragePath }        -> std::convertible_to< ::config::IPathProvider::PathFor >;
    { T::StorageFilename }    -> std::convertible_to< const char* >;
};
#define ConfigSerializeableTypename   Serializeable

template<typename T>
constexpr bool HasPostLoad = requires( T& t )
{
    { t.postLoad() } -> std::same_as<bool>;
};

template<typename T>
constexpr bool HasPreSave = requires( const T& t )
{
    { t.preSave() } -> std::same_as<bool>;
};

#define OURO_CONFIG( _name )    struct _name

#else

#define ConfigSerializeableTypename   typename

struct Base
{
    virtual ~Base() {}
    virtual bool postLoad() { return true; }
    virtual bool preSave() { return true; }
};

#define OURO_CONFIG( _name )    struct _name : public Base

#endif // OURO_CONFIG_CONCEPTS


// ---------------------------------------------------------------------------------------------------------------------
enum class LoadResult
{
    Success,
    CannotFindConfigFile,
    ErrorDuringLoad,
    FailedPostLoad
};

inline const char* LoadResultToString( LoadResult lr )
{
    switch ( lr )
    {
    case LoadResult::Success:               return "Success";
    case LoadResult::CannotFindConfigFile:  return "Cannot Find File";
    case LoadResult::ErrorDuringLoad:       return "Error During Load";
    case LoadResult::FailedPostLoad:        return "Failed Post Load";
    default:
        break;
    }
    return "Unknown";
}

enum class SaveResult
{
    Success,
    FailedPreSave,
    Failed
};

// ---------------------------------------------------------------------------------------------------------------------
// return the full assembled path for the given config type
//
template < ConfigSerializeableTypename _config_type >
inline fs::path getFullPath( const IPathProvider& pathProvider )
{
    fs::path loadPath = pathProvider.getPath( _config_type::StoragePath );
    loadPath.append( _config_type::StorageFilename );
    return loadPath;
}

// ---------------------------------------------------------------------------------------------------------------------
// generic load & save functions for a Base derived data type

template < ConfigSerializeableTypename _config_type >
inline LoadResult load( const IPathProvider& pathProvider, _config_type& result )
{
    const fs::path loadPath = getFullPath<_config_type>( pathProvider );

    if ( !fs::exists( loadPath ) )
        return LoadResult::CannotFindConfigFile;

    try
    {
        std::ifstream is( loadPath );
        cereal::JSONInputArchive archive( is );

        result.serialize( archive );
    }
    catch ( cereal::Exception& cEx )
    {
        blog::error::cfg( "cannot parse [{}] : {}", loadPath.string(), cEx.what() );
        return LoadResult::ErrorDuringLoad;
    }

#if OURO_CONFIG_CONCEPTS
    if constexpr ( HasPostLoad<_config_type> )
#endif // OURO_CONFIG_CONCEPTS
    {
        if ( !result.postLoad() )
        {
            return LoadResult::FailedPostLoad;
        }
    }

    return LoadResult::Success;
}

template < ConfigSerializeableTypename _config_type >
inline LoadResult loadFromMemory( const std::string& rawJson, _config_type& result )
{
    try
    {
        std::istringstream is( rawJson );
        cereal::JSONInputArchive archive( is );

        result.serialize( archive );
    }
    catch ( cereal::Exception& cEx )
    {
        blog::error::cfg( "cannot parse from JSON : {}", cEx.what() );
        return LoadResult::ErrorDuringLoad;
    }

#if OURO_CONFIG_CONCEPTS
    if constexpr ( HasPostLoad<_config_type> )
#endif // OURO_CONFIG_CONCEPTS
    {
        if ( !result.postLoad() )
        {
            return LoadResult::FailedPostLoad;
        }
    }

    return LoadResult::Success;

}

template < ConfigSerializeableTypename _config_type >
inline SaveResult save( const IPathProvider& pathProvider, _config_type& data )
{
#if OURO_CONFIG_CONCEPTS
    if constexpr ( HasPreSave<_config_type> )
#endif // OURO_CONFIG_CONCEPTS
    {
        if ( !data.preSave() )
        {
            return SaveResult::FailedPreSave;
        }
    }

    const fs::path savePath = getFullPath<_config_type>( pathProvider );

    try
    {
        std::ofstream is( savePath );
        cereal::JSONOutputArchive archive( is );

        data.serialize( archive );
    }
    catch ( cereal::Exception& cEx )
    {
        blog::error::cfg( "cannot write [{}] | {}", savePath.string(), cEx.what() );
        return SaveResult::Failed;
    }

    return SaveResult::Success;
}

} // namespace config

// specialisation for loading direct map of string:jamscan rather than cereal's default bloated way
#define SPECIALISE_FLAT_HASH_MAP_LOAD( _keyType, _valueType )                               \
    template <class Archive, class C, class A> inline                                       \
        void load( Archive& ar, absl::flat_hash_map<_keyType, _valueType, C, A>& map )      \
    {                                                                                       \
        map.clear();                                                                        \
                                                                                            \
        auto hint = map.begin();                                                            \
        while ( true )                                                                      \
        {                                                                                   \
            const auto namePtr = ar.getNodeName();                                          \
                                                                                            \
            if ( !namePtr )                                                                 \
                break;                                                                      \
                                                                                            \
            _keyType key = _keyType( namePtr );                                             \
            _valueType value; ar( value );                                                  \
            hint = map.emplace_hint( hint, std::move( key ), std::move( value ) );          \
        }                                                                                   \
    }
