//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#pragma once

#include <type_traits>

// originally from q_lib, Copyright (c) 2014-2019 Joel de Guzman. MIT Licence

namespace dsp {

// ---------------------------------------------------------------------------------------------------------------------
// value: Value abstraction and encapsulation
template <typename T, typename Derived>
struct value
{
    using derived_type = Derived;
    using value_type = T;

    constexpr explicit            value( T val = T( 0 ) ) : rep( val ) {}
    constexpr                     value( value const& ) = default;
    constexpr                     value( value&& ) = default;

    constexpr value& operator=( value const& ) = default;
    constexpr value& operator=( value&& ) = default;

    constexpr explicit operator T() const { return rep; }
    constexpr derived_type        operator+() const { return derived(); }
    constexpr derived_type        operator-() const { return derived_type{ -rep }; }

    constexpr derived_type& operator+=( value rhs );
    constexpr derived_type& operator-=( value rhs );
    constexpr derived_type& operator*=( value rhs );
    constexpr derived_type& operator/=( value rhs );

    constexpr derived_type const& derived() const;
    constexpr derived_type& derived();

    constexpr static derived_type lerp( const derived_type lhs, const derived_type rhs, const value_type t ) {
        return lhs.rep + t * (rhs.rep - lhs.rep);
    }

    T rep;
};

// ---------------------------------------------------------------------------------------------------------------------
template <typename T, typename Derived>
constexpr typename value<T, Derived>::derived_type const&
    value<T, Derived>::derived() const
{
    return *static_cast<derived_type const*>(this);
}

template <typename T, typename Derived>
constexpr typename value<T, Derived>::derived_type&
    value<T, Derived>::derived()
{
    return *static_cast<derived_type*>(this);
}

// ---------------------------------------------------------------------------------------------------------------------
template <typename T, typename Derived>
constexpr bool operator==( value<T, Derived> a, value<T, Derived> b )
{
    return a.rep == b.rep;
}

template <typename T, typename Derived>
constexpr bool operator==( T a, value<T, Derived> b )
{
    return a == b.rep;
}

template <typename T, typename Derived>
constexpr bool operator==( value<T, Derived> a, T b )
{
    return a.rep == b;
}

// ---------------------------------------------------------------------------------------------------------------------
template <typename T, typename Derived>
constexpr bool operator!=( value<T, Derived> a, value<T, Derived> b )
{
    return a.rep != b.rep;
}

template <typename T, typename Derived>
constexpr bool operator!=( T a, value<T, Derived> b )
{
    return a != b.rep;
}

template <typename T, typename Derived>
constexpr bool operator!=( value<T, Derived> a, T b )
{
    return a.rep != b;
}

// ---------------------------------------------------------------------------------------------------------------------
template <typename T, typename Derived>
constexpr bool operator<( value<T, Derived> a, value<T, Derived> b )
{
    return a.rep < b.rep;
}

template <typename T, typename Derived>
constexpr bool operator<( T a, value<T, Derived> b )
{
    return a < b.rep;
}

template <typename T, typename Derived>
constexpr bool operator<( value<T, Derived> a, T b )
{
    return a.rep < b;
}

// ---------------------------------------------------------------------------------------------------------------------
template <typename T, typename Derived>
constexpr bool operator<=( value<T, Derived> a, value<T, Derived> b )
{
    return a.rep <= b.rep;
}

template <typename T, typename Derived>
constexpr bool operator<=( T a, value<T, Derived> b )
{
    return a <= b.rep;
}

template <typename T, typename Derived>
constexpr bool operator<=( value<T, Derived> a, T b )
{
    return a.rep <= b;
}

// ---------------------------------------------------------------------------------------------------------------------
template <typename T, typename Derived>
constexpr bool operator>( value<T, Derived> a, value<T, Derived> b )
{
    return a.rep > b.rep;
}

template <typename T, typename Derived>
constexpr bool operator>( T a, value<T, Derived> b )
{
    return a > b.rep;
}

template <typename T, typename Derived>
constexpr bool operator>( value<T, Derived> a, T b )
{
    return a.rep > b;
}

// ---------------------------------------------------------------------------------------------------------------------
template <typename T, typename Derived>
constexpr bool operator>=( value<T, Derived> a, value<T, Derived> b )
{
    return a.rep >= b.rep;
}

template <typename T, typename Derived>
constexpr bool operator>=( T a, value<T, Derived> b )
{
    return a >= b.rep;
}

template <typename T, typename Derived>
constexpr bool operator>=( value<T, Derived> a, T b )
{
    return a.rep >= b;
}

// ---------------------------------------------------------------------------------------------------------------------
template <typename T, typename Derived>
constexpr Derived& value<T, Derived>::operator+=( value<T, Derived> rhs )
{
    rep += rhs.rep;
    return derived();
}

template <typename T, typename Derived>
constexpr Derived& value<T, Derived>::operator-=( value<T, Derived> rhs )
{
    rep -= rhs.rep;
    return derived();
}

template <typename T, typename Derived>
constexpr Derived& value<T, Derived>::operator*=( value<T, Derived> rhs )
{
    rep *= rhs.rep;
    return derived();
}

template <typename T, typename Derived>
constexpr Derived& value<T, Derived>::operator/=( value<T, Derived> rhs )
{
    rep /= rhs.rep;
    return derived();
}

// ---------------------------------------------------------------------------------------------------------------------
template <typename T, typename Derived>
constexpr Derived operator+( value<T, Derived> a, value<T, Derived> b )
{
    return Derived( a.rep + b.rep );
}

template <typename T, typename Derived>
constexpr Derived operator-( value<T, Derived> a, value<T, Derived> b )
{
    return Derived( a.rep - b.rep );
}

template <typename T, typename Derived>
constexpr Derived operator*( value<T, Derived> a, value<T, Derived> b )
{
    return Derived( a.rep * b.rep );
}

template <typename T, typename Derived>
constexpr Derived operator/( value<T, Derived> a, value<T, Derived> b )
{
    return Derived( a.rep / b.rep );
}

// ---------------------------------------------------------------------------------------------------------------------
template <typename T, typename Derived, typename T2>
constexpr typename std::enable_if<
    !std::is_same<Derived, T2>::value, Derived
>::type
    operator+( T2 a, value<T, Derived> b )
{
    return Derived( a + b.rep );
}

template <typename T, typename Derived, typename T2>
constexpr typename std::enable_if<
    !std::is_same<Derived, T2>::value, Derived
>::type
    operator-( T2 a, value<T, Derived> b )
{
    return Derived( a - b.rep );
}

template <typename T, typename Derived, typename T2>
constexpr typename std::enable_if<
    !std::is_same<Derived, T2>::value, Derived
>::type
    operator*( T2 a, value<T, Derived> b )
{
    return Derived( a * b.rep );
}

template <typename T, typename Derived, typename T2>
constexpr typename std::enable_if<
    !std::is_same<Derived, T2>::value, Derived
>::type
    operator/( T2 a, value<T, Derived> b )
{
    return Derived( a / b.rep );
}

// ---------------------------------------------------------------------------------------------------------------------
template <typename T, typename Derived, typename T2>
constexpr typename std::enable_if<
    !std::is_same<Derived, T2>::value, Derived
>::type
    operator+( value<T, Derived> a, T2 b )
{
    return Derived( a.rep + b );
}

template <typename T, typename Derived, typename T2>
constexpr typename std::enable_if<
    !std::is_same<Derived, T2>::value, Derived
>::type
    operator-( value<T, Derived> a, T2 b )
{
    return Derived( a.rep - b );
}

template <typename T, typename Derived, typename T2>
constexpr typename std::enable_if<
    !std::is_same<Derived, T2>::value, Derived
>::type
    operator*( value<T, Derived> a, T2 b )
{
    return Derived( a.rep * b );
}

template <typename T, typename Derived, typename T2>
constexpr typename std::enable_if<
    !std::is_same<Derived, T2>::value, Derived
>::type
    operator/( value<T, Derived> a, T2 b )
{
    return Derived( a.rep / b );
}

struct duration;
struct frequency;
struct period;

// ---------------------------------------------------------------------------------------------------------------------
struct frequency : value<double, frequency>
{
    using base_type = value<double, frequency>;
    using base_type::base_type;

    constexpr                     frequency( double val ) : base_type( val ) {}
    constexpr                     frequency( duration d );

    constexpr explicit operator double() const { return rep; }
    constexpr explicit operator float() const { return (float)rep; }
    constexpr dsp::period         period() const;
};

// ---------------------------------------------------------------------------------------------------------------------
struct duration : value<double, duration>
{
    using base_type = value<double, duration>;
    using base_type::base_type;

    constexpr                     duration( double val ) : base_type( val ) {}

    constexpr explicit operator double() const { return rep; }
    constexpr explicit operator float() const { return (float)rep; }
};

// ---------------------------------------------------------------------------------------------------------------------
struct period : duration
{
    using duration::duration;

    constexpr                     period( duration d ) : duration( d ) {}
    constexpr                     period( frequency f ) : duration( 1.0 / f.rep ) {}
};

// ---------------------------------------------------------------------------------------------------------------------
constexpr frequency::frequency( duration d )
    : base_type( 1.0 / d.rep )
{}

constexpr dsp::period frequency::period() const
{
    return 1.0 / rep;
}

// ---------------------------------------------------------------------------------------------------------------------
template< typename _type >
struct SchmittTrigger
{
    SchmittTrigger( _type hysteresis )
        : m_hysteresis( hysteresis )
    {}

    inline bool operator()( _type pos, _type neg )
    {
        if ( !m_state && pos > (neg + m_hysteresis) )
            m_state = true;
        else if ( m_state && pos < (neg - m_hysteresis) )
            m_state = false;
        return m_state;
    }

    inline bool operator()() const
    {
        return m_state;
    }

private:
    _type const     m_hysteresis;
    bool            m_state = false;
};

// ---------------------------------------------------------------------------------------------------------------------
template< typename _type >
struct SchmittLatch
{
    SchmittLatch( _type hysteresis )
        : m_trigger( hysteresis )
        , m_latched( false )
    {}

    inline bool operator()( _type pos, _type neg )
    {
        bool trig = m_trigger( pos, neg );
        if ( trig && !m_latched )
        {
            m_latched = true;
            return true;
        }
        if ( !trig )
        {
            m_latched = false;
        }
        return false;
    }

private:
    SchmittTrigger<_type>   m_trigger;
    bool                    m_latched;
};

} // namespace dsp

constexpr dsp::frequency operator "" _Hz( long double val )
{
    return { double( val ) };
}

constexpr dsp::frequency operator "" _Hz( unsigned long long int val )
{
    return { double( val ) };
}

constexpr dsp::frequency operator "" _KHz( long double val )
{
    return { double( val * 1e3 ) };
}

constexpr dsp::frequency operator "" _KHz( unsigned long long int val )
{
    return { double( val * 1e3 ) };
}

constexpr dsp::frequency operator "" _kHz( long double val )
{
    return { double( val * 1e3 ) };
}

constexpr dsp::frequency operator "" _kHz( unsigned long long int val )
{
    return { double( val * 1e3 ) };
}

constexpr dsp::frequency operator "" _MHz( long double val )
{
    return { double( val * 1e6 ) };
}

constexpr dsp::frequency operator "" _MHz( unsigned long long int val )
{
    return { double( val * 1e6 ) };
}

