//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//

#pragma once

namespace vx {


// ---------------------------------------------------------------------------------------------------------------------
// data format of effect configuration file on disk, stored as json
//
struct VibeBlueprint
{
    struct Operation
    {
        struct Toggle
        {
            std::string                     name;   // "Film Grain"
            std::vector< std::string >      opts;   // list of 1 or more defines; one define = checkbox, multiple = radio selection
            int32_t                         default_value = -1;

            template<class Archive>
            void serialize( Archive& archive )
            {
                archive( CEREAL_NVP( name ),
                         CEREAL_NVP( opts ),
                         CEREAL_OPTIONAL_NVP( default_value )
                );
            }
        };
        struct Dial
        {
            std::string                     name;       // "Intensity"
            float                           default_value = 0;
            std::vector< float >            range;      // min,max

            template<class Archive>
            void serialize( Archive& archive )
            {
                archive( CEREAL_NVP( name ),
                         CEREAL_NVP( default_value ),
                         CEREAL_NVP( range )
                );
            }
        };

        std::string             name;
        std::string             fragment;
        std::string             bufferInA;
        std::string             bufferInB;
        std::string             bufferOut;
        std::vector< Toggle >   toggles;
        std::vector< Dial >     dials;

        template<class Archive>
        void serialize( Archive& archive )
        {
            archive( CEREAL_NVP( name ),
                     CEREAL_NVP( fragment ),
                     CEREAL_OPTIONAL_NVP( bufferInA ),
                     CEREAL_OPTIONAL_NVP( bufferInB ),
                     CEREAL_NVP( bufferOut ),
                     CEREAL_OPTIONAL_NVP( toggles ),
                     CEREAL_OPTIONAL_NVP( dials )
            );
        }
    };

    struct Declaration
    {
        std::string                 name;
        std::string                 copyright;
        std::vector< Operation >    operations;

        template<class Archive>
        void serialize( Archive& archive )
        {
            archive( CEREAL_NVP( name ),
                     CEREAL_NVP( copyright ),
                     CEREAL_NVP( operations )
            );
        }
    };

    std::vector< Declaration >      declarations;

    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( declarations )
        );
    }
};

} // namespace vx
