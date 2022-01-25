//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

namespace vst { class Instance; }
namespace app { struct AudioPlaybackTimeInfo; namespace module { struct Frontend; } }
namespace data { struct DataBus; }

namespace effect {

struct IContainer;

// ---------------------------------------------------------------------------------------------------------------------
struct EffectStack
{
    EffectStack() = delete;
    EffectStack( IContainer* effectContainer, const app::AudioPlaybackTimeInfo* vstTimePtr, const std::string& name )
        : m_effectContainer( effectContainer )
        , m_audioTimingInfoPtr( vstTimePtr )
        , m_stackName( name )
        , m_incrementalLoadId( 0 )
    {}

    // 
    struct Parameter
    {
        Parameter()
            : pidx( -1 )
        {}

        Parameter( const char* _name, const int32_t _idx, const double _low, const double _high )
            : name( _name )
            , pidx( _idx )
            , low( (float)_low )
            , high( (float)_high )
        {}

        template<class Archive>
        void serialize( Archive& archive )
        {
            archive(
                CEREAL_NVP( name ),
                CEREAL_NVP( pidx ),
                CEREAL_NVP( low ),
                CEREAL_NVP( high )
            );
        }

        std::string name;   // cached lookup from VST
        int32_t     pidx;
        float       low;
        float       high;

        inline bool isValid() const { return pidx >= 0; }
    };

    struct ParameterBinding
    {
        ParameterBinding() = default;
        ParameterBinding( const char* _name )
            : name(_name)
            , bus(0)
        {}

        template<class Archive>
        void serialize( Archive& archive )
        {
            archive(
                CEREAL_NVP( name ),
                CEREAL_NVP( parameters ),
                CEREAL_NVP( bus )
            );
        }

        // check if we already have this parameter index in play
        inline bool parameterIndexRegistered( const int32_t byIndex ) const
        {
            for ( const auto& param : parameters )
                if ( param.pidx == byIndex )
                    return true;

            return false;
        }

        inline void deleteParameterByIndex( const int32_t byIndex )
        {
            auto new_end = std::remove_if( parameters.begin(),
                                           parameters.end(),
                                           [&]( const Parameter& pb )
                                           {
                                               return pb.pidx == byIndex;
                                           });
            parameters.erase( new_end, parameters.end() );
        }

        std::string                 name;
        std::vector< Parameter >    parameters;
        int32_t                     bus;
    };

    struct ParameterSet
    {
        template<class Archive>
        void serialize( Archive& archive )
        {
            archive(
                CEREAL_NVP( bindings )
            );
        }

        // delete a binding, given its name
        void deleteBindingByName( const std::string& byName )
        {
            auto new_end = std::remove_if( bindings.begin(),
                                           bindings.end(),
                                           [&]( const ParameterBinding& pb )
                                           {
                                               return pb.name == byName;
                                           });
            bindings.erase( new_end, bindings.end() );
        }


        // shunt DataBus values into parameters based on the mappings designed
        void syncToDataBus( const data::DataBus& bus, vst::Instance* vsti ) const;


        std::vector< ParameterBinding > bindings;
    };

    struct Session
    {
        std::vector< int64_t >      vstIDs;
        std::vector< std::string >  vstPaths;
        std::vector< std::string >  vstStateData;
        std::vector< bool >         vstActive;

        template<class Archive>
        void serialize( Archive& archive )
        {
            archive(
                CEREAL_NVP( vstIDs ),
                CEREAL_NVP( vstPaths ),
                CEREAL_NVP( vstStateData ),
                CEREAL_NVP( vstActive )
            );
        }
    };


    inline void save( const fs::path& appStashPath )
    {
        saveSession( appStashPath );
        saveParameters( appStashPath );
    }

    void load( const fs::path& appStashPath );

    // unload and clear out everything
    void clear();

    // manually add a plugin by the given filename (usually called by chooseNewVST)
    vst::Instance* addVST( const char* vstFilename, const int64_t vstLoadID, const bool haltUntilLoaded = false );

    // pop file selector to load a new plugin
    void chooseNewVST( const app::module::Frontend& appFrontend );

    // unload a plugin by the load-id
    bool removeVST( const int64_t loadID );


    // render management ui
    void imgui( const app::module::Frontend& appFrontend, const data::DataBus* dataBus = nullptr );


    void syncToDataBus( const data::DataBus& bus )
    {
        for ( const auto& paramSet : m_parameters )
        {
            const auto vstID = paramSet.first;
            auto vstInstIter = m_instances.find( vstID );

            // not finding an instance means the ID in the parameter map points to an empty VST slot
            if ( vstInstIter != m_instances.end() )
            {
                paramSet.second.syncToDataBus( bus, vstInstIter->second );
            }
        }
    }


private:

    void saveSession( const fs::path& appStashPath );
    void saveParameters( const fs::path& appStashPath );

    IContainer*                                         m_effectContainer;      // the container we're configuring; eg. an Audio module
    const app::AudioPlaybackTimeInfo*                   m_audioTimingInfoPtr;   // shared timing memory used to bind mixers and vsts
    std::string                                         m_stackName;

    int64_t                                             m_incrementalLoadId;    // incremented each time we add a VST, encoded into the instance
                                                                                // it becomes what we use to identify each instance in the storage containers & UI
    std::vector< int64_t >                              m_order;                // list of the VSTs in play, in order of application in the audio engine
    std::unordered_map< int64_t, vst::Instance* >       m_instances;            // lookup plugin instance by load-id
    std::unordered_map< int64_t, ParameterSet >         m_parameters;           // lookup extracted automated parameters by load-id
};

} // namespace effect
