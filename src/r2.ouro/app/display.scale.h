//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//

#pragma once

namespace app
{
    // represents a computed display scale for a monitor;
    // sizes multiplied by this value using scaleToDisplay() (eg. for a 150% monitor scale, it will multiply by 1.5)
    class DisplayScale
    {
    public:
        DisplayScale() = default;

        constexpr void set( float newValue ) { m_displayScale = newValue; }

        ouro_nodiscard constexpr float getDisplayScaleFactor() const { return m_displayScale; }
        ouro_nodiscard constexpr float scaleToDisplay( const float value ) const { return value * getDisplayScaleFactor(); }
        ouro_nodiscard int32_t roundedScaleToDisplay( const int32_t value ) const
        {
            // integer version scales as float and rounds out
            return static_cast< int32_t >( std::round( scaleToDisplay( static_cast< float >( value ) ) ) );
        }

        ouro_nodiscard constexpr bool isUnscaled() const { return m_displayScale == 1.0f; }

        // sugar for scaleToDisplay(), multiply on lhs or rhs
        constexpr float operator*( const float value ) const { return value * getDisplayScaleFactor(); }
        constexpr float operator()( const float value ) const { return value * getDisplayScaleFactor(); }
        constexpr friend float operator*( float value, const DisplayScale& scale );

    private:
        float m_displayScale = 1.0f;
    };

    constexpr float operator*( const float value, const DisplayScale& scale )
    {
        return scale.scaleToDisplay( value );
    }
} // namespace app
