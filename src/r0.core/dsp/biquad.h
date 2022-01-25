//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#pragma once

#include "dsp/types.h"

// originally from q_lib, Copyright (c) 2014-2019 Joel de Guzman. MIT Licence

namespace dsp {


// ---------------------------------------------------------------------------------------------------------------------
// biquad class. Based on Audio-EQ Cookbook by Robert Bristow-Johnson.
// http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
//
struct biquad
{
    biquad( biquad const& ) = default;

    biquad( double a0, double a1, double a2, double a3, double a4 )
        : a0( (float)a0 ), a1( (float)a1 ), a2( (float)a2 ), a3( (float)a3 ), a4( (float)a4 )
        , x1( 0 ), x2( 0 ), y1( 0 ), y2( 0 )
    {}

    float operator()( float s )
    {
        // compute result
        auto r = a0 * s + a1 * x1 + a2 * x2 - a3 * y1 - a4 * y2;

        // shift x1 to x2, s to x1
        x2 = x1;
        x1 = s;

        // shift y1 to y2, r to y1
        y2 = y1;
        y1 = r;

        return r;
    }

    void config( double a0_, double a1_, double a2_, double a3_, double a4_ )
    {
        a0 = (float)a0_;
        a1 = (float)a1_;
        a2 = (float)a2_;
        a3 = (float)a3_;
        a4 = (float)a4_;
    }

    // Coefficients
    float a0, a1, a2, a3, a4;

    // Sample delays
    float x1, x2, y1, y2;
};

// ---------------------------------------------------------------------------------------------------------------------
// bw: utility type to distinguish bandwidth from q which is just a double
//
struct bw
{
    double val;
};

namespace detail
{
    // -----------------------------------------------------------------------------------------------------------------
    struct config_biquad
    {
        config_biquad( frequency f, std::uint32_t sps )
            : omega( constants::d_2pi * double( f ) / sps )
            , sin( std::sin( omega ) )
            , cos( std::cos( omega ) )
        {}

        config_biquad( frequency f, std::uint32_t sps, bw _bw )
            : config_biquad( f, sps )
        {
            alpha = sin * std::sinh( std::log( 2.0 ) / 2.0 * _bw.val * omega / sin );
        }

        config_biquad( frequency f, std::uint32_t sps, double q )
            : config_biquad( f, sps )
        {
            alpha = sin / (2.0 * q);
        }

        biquad make()
        {
            return biquad( b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 );
        }

        void config( biquad& bq )
        {
            bq.config( b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 );
        }

        double omega, sin, cos, alpha;
        double a0, a1, a2, b0, b1, b2;
    };

    // -----------------------------------------------------------------------------------------------------------------
    struct config_biquad_a : config_biquad
    {
        config_biquad_a( double db_gain, frequency f, std::uint32_t sps, bw _bw )
            : config_biquad( f, sps, _bw )
            , a( std::pow( 10.0, db_gain / 40.0 ) )
            , beta( std::sqrt( a + a ) )
        {}

        config_biquad_a( double db_gain, frequency f, std::uint32_t sps, double q )
            : config_biquad( f, sps, q )
            , a( std::pow( 10.0, db_gain / 40.0 ) )
            , beta( std::sqrt( a + a ) )
        {}

        double a;
        double beta;
    };

    // -----------------------------------------------------------------------------------------------------------------
    struct config_lowpass : config_biquad
    {
        config_lowpass( frequency f, std::uint32_t sps, double q )
            : config_biquad( f, sps, q )
        {
            init();
        }

        void init()
        {
            b0 = (1.0 - cos) / 2.0;
            b1 = 1.0 - cos;
            b2 = (1.0 - cos) / 2.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cos;
            a2 = 1.0 - alpha;
        }
    };

    // -----------------------------------------------------------------------------------------------------------------
    struct config_highpass : config_biquad
    {
        config_highpass( frequency f, std::uint32_t sps, double q )
            : config_biquad( f, sps, q )
        {
            init();
        }

        void init()
        {
            b0 = (1.0 + cos) / 2.0;
            b1 = -(1.0 + cos);
            b2 = (1.0 + cos) / 2.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cos;
            a2 = 1.0 - alpha;
        }
    };

    // -----------------------------------------------------------------------------------------------------------------
    struct config_bandpass_csg : config_biquad
    {
        config_bandpass_csg( frequency f, std::uint32_t sps, bw _bw )
            : config_biquad( f, sps, _bw )
        {
            init();
        }

        config_bandpass_csg( frequency f, std::uint32_t sps, double q )
            : config_biquad( f, sps, q )
        {
            init();
        }

        void init()
        {
            b0 = sin / 2.0;
            b1 = 0.0;
            b2 = -sin / 2;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cos;
            a2 = 1.0 - alpha;
        }
    };

    // -----------------------------------------------------------------------------------------------------------------
    struct config_bandpass_cpg : config_biquad
    {
        config_bandpass_cpg( frequency f, std::uint32_t sps, bw _bw )
            : config_biquad( f, sps, _bw )
        {
            init();
        }

        config_bandpass_cpg( frequency f, std::uint32_t sps, double q )
            : config_biquad( f, sps, q )
        {
            init();
        }

        void init()
        {
            b0 = alpha;
            b1 = 0.0;
            b2 = -alpha;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cos;
            a2 = 1.0 - alpha;
        }
    };

    // -----------------------------------------------------------------------------------------------------------------
    struct config_notch : config_biquad
    {
        config_notch( frequency f, std::uint32_t sps, bw _bw )
            : config_biquad( f, sps, _bw )
        {
            init();
        }

        config_notch( frequency f, std::uint32_t sps, double q )
            : config_biquad( f, sps, q )
        {
            init();
        }

        void init()
        {
            b0 = 1.0;
            b1 = -2.0 * cos;
            b2 = 1.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cos;
            a2 = 1.0 - alpha;
        }
    };

    // -----------------------------------------------------------------------------------------------------------------
    struct config_allpass : config_biquad
    {
        config_allpass( frequency f, std::uint32_t sps, double q )
            : config_biquad( f, sps, q )
        {
            init();
        }

        void init()
        {
            b0 = 1.0 - alpha;
            b1 = -2.0 * cos;
            b2 = 1.0 + alpha;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cos;
            a2 = 1.0 - alpha;
        }
    };

    // -----------------------------------------------------------------------------------------------------------------
    struct config_peaking : config_biquad_a
    {
        config_peaking( double db_gain, frequency f, std::uint32_t sps, bw _bw )
            : config_biquad_a( db_gain, f, sps, _bw )
        {
            init();
        }

        config_peaking( double db_gain, frequency f, std::uint32_t sps, double q )
            : config_biquad_a( db_gain, f, sps, q )
        {
            init();
        }

        void init()
        {
            b0 = 1.0 + alpha * a;
            b1 = -2.0 * cos;
            b2 = 1.0 - alpha * a;
            a0 = 1.0 + alpha / a;
            a1 = -2.0 * cos;
            a2 = 1.0 - alpha / a;
        }
    };

    // -----------------------------------------------------------------------------------------------------------------
    struct config_lowshelf : config_biquad_a
    {
        config_lowshelf( double db_gain, frequency f, std::uint32_t sps, double q )
            : config_biquad_a( db_gain, f, sps, q )
        {
            init();
        }

        void init()
        {
            b0 = a * ((a + 1.0) - (a - 1.0) * cos + beta * sin);
            b1 = 2.0 * a * ((a - 1.0) - (a + 1.0) * cos);
            b2 = a * ((a + 1.0) - (a - 1.0) * cos - beta * sin);
            a0 = (a + 1.0) + (a - 1.0) * cos + beta * sin;
            a1 = -2.0 * ((a - 1.0) + (a + 1.0) * cos);
            a2 = (a + 1.0) + (a - 1.0) * cos - beta * sin;
        }
    };

    // -----------------------------------------------------------------------------------------------------------------
    struct config_highshelf : config_biquad_a
    {
        config_highshelf( double db_gain, frequency f, std::uint32_t sps, double q )
            : config_biquad_a( db_gain, f, sps, q )
        {
            init();
        }

        void init()
        {
            b0 = a * ((a + 1.0) + (a - 1.0) * cos + beta * sin);
            b1 = -2.0 * a * ((a - 1.0) + (a + 1.0) * cos);
            b2 = a * ((a + 1.0) + (a - 1.0) * cos - beta * sin);
            a0 = (a + 1.0) - (a - 1.0) * cos + beta * sin;
            a1 = 2.0 * ((a - 1.0) - (a + 1.0) * cos);
            a2 = (a + 1.0) - (a - 1.0) * cos - beta * sin;
        }
    };
}

// ---------------------------------------------------------------------------------------------------------------------
// Low pass filter
//
struct lowpass : biquad
{
    lowpass( frequency f, std::uint32_t sps, double q = 0.707 )
        : biquad( detail::config_lowpass( f, sps, q ).make() )
    {}

    void config( frequency f, std::uint32_t sps, double q = 0.707 )
    {
        detail::config_lowpass( f, sps, q ).config( *this );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// High pass filter
//
struct highpass : biquad
{
    highpass( frequency f, std::uint32_t sps, double q = 0.707 )
        : biquad( detail::config_highpass( f, sps, q ).make() )
    {}

    void config( frequency f, std::uint32_t sps, double q = 0.707 )
    {
        detail::config_highpass( f, sps, q ).config( *this );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Band pass filter; constant skirt gain, peak gain = Q
//
struct bandpass_csg : biquad
{
    bandpass_csg( frequency f, std::uint32_t sps, bw _bw )
        : biquad( detail::config_bandpass_csg( f, sps, _bw ).make() )
    {}

    bandpass_csg( frequency f, std::uint32_t sps, double q = 0.707 )
        : biquad( detail::config_bandpass_csg( f, sps, q ).make() )
    {}

    void config( frequency f, std::uint32_t sps, bw _bw )
    {
        detail::config_bandpass_csg( f, sps, _bw ).config( *this );
    }

    void config( frequency f, std::uint32_t sps, double q = 0.707 )
    {
        detail::config_bandpass_csg( f, sps, q ).config( *this );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Band pass filter; constant 0 dB peak gain
//
struct bandpass_cpg : biquad
{
    bandpass_cpg( frequency f, std::uint32_t sps, bw _bw )
        : biquad( detail::config_bandpass_cpg( f, sps, _bw ).make() )
    {}

    bandpass_cpg( frequency f, std::uint32_t sps, double q = 0.707 )
        : biquad( detail::config_bandpass_cpg( f, sps, q ).make() )
    {}

    void config( frequency f, std::uint32_t sps, bw _bw )
    {
        detail::config_bandpass_cpg( f, sps, _bw ).config( *this );
    }

    void config( frequency f, std::uint32_t sps, double q = 0.707 )
    {
        detail::config_bandpass_cpg( f, sps, q ).config( *this );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// All pass filter
//
struct allpass : biquad
{
    allpass( frequency f, std::uint32_t sps, double q = 0.707 )
        : biquad( detail::config_allpass( f, sps, q ).make() )
    {}

    void config( frequency f, std::uint32_t sps, double q = 0.707 )
    {
        detail::config_allpass( f, sps, q ).config( *this );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Notch filter
//
struct notch : biquad
{
    notch( frequency f, std::uint32_t sps, bw _bw )
        : biquad( detail::config_notch( f, sps, _bw ).make() )
    {}

    notch( frequency f, std::uint32_t sps, double q = 0.707 )
        : biquad( detail::config_notch( f, sps, q ).make() )
    {}

    void config( frequency f, std::uint32_t sps, bw _bw )
    {
        detail::config_notch( f, sps, _bw ).config( *this );
    }

    void config( frequency f, std::uint32_t sps, double q = 0.707 )
    {
        detail::config_notch( f, sps, q ).config( *this );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Peaking band EQ filter
//
struct peaking : biquad
{
    peaking( double db_gain, frequency f, std::uint32_t sps, bw _bw )
        : biquad( detail::config_peaking( db_gain, f, sps, _bw ).make() )
    {}

    peaking( double db_gain, frequency f, std::uint32_t sps, double q = 0.707 )
        : biquad( detail::config_peaking( db_gain, f, sps, q ).make() )
    {}

    void config( double db_gain, frequency f, std::uint32_t sps, bw _bw )
    {
        detail::config_peaking( db_gain, f, sps, _bw ).config( *this );
    }

    void config( double db_gain, frequency f, std::uint32_t sps, double q = 0.707 )
    {
        detail::config_peaking( db_gain, f, sps, q ).config( *this );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Low shelf filter
//
struct lowshelf : biquad
{
    lowshelf( double db_gain, frequency f, std::uint32_t sps, double q = 0.707 )
        : biquad( detail::config_lowshelf( db_gain, f, sps, q ).make() )
    {}

    void config( double db_gain, frequency f, std::uint32_t sps, double q = 0.707 )
    {
        detail::config_lowshelf( db_gain, f, sps, q ).config( *this );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// High shelf filter
//
struct highshelf : biquad
{
    highshelf( double db_gain, frequency f, std::uint32_t sps, double q = 0.707 )
        : biquad( detail::config_highshelf( db_gain, f, sps, q ).make() )
    {}

    void config( double db_gain, frequency f, std::uint32_t sps, double q = 0.707 )
    {
        detail::config_highshelf( db_gain, f, sps, q ).config( *this );
    }
};

} // namespace dsp

