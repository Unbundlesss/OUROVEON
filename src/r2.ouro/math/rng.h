//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//

#include <inttypes.h>

namespace math {

class RNG32
{
public:

    RNG32();            // construct with seed from time value
    void reseed();      // reseed from time value


    // construct or reseed with a specific seed value
    constexpr RNG32( uint32_t seed )
    {
        reseed( seed );
    }
    constexpr void reseed( uint32_t newseed );


    // random float in range [0..1]
    constexpr float genFloat();

    // random float between rmin-rmax
    constexpr float genFloat( float rmin, float rmax );

    // float in range [0..1] in Gaussian distribution (bell curve approximation)
    constexpr float genGaussApprox();

    // random signed integer
    constexpr int32_t genInt32();

    // random signed integer between rmin-rmax
    constexpr int32_t genInt32( int32_t rmin, int32_t rmax );

    // random unsigned integer
    constexpr uint32_t genUInt32();

protected:

    uint32_t  m_seed[2];
};


//----------------------------------------------------------------------------------------------------------------------
constexpr void RNG32::reseed( uint32_t newseed )
{
    m_seed[0] = newseed + !newseed;
    m_seed[1] = ((newseed << 16) | (newseed >> 16)) ^ newseed;
}

//----------------------------------------------------------------------------------------------------------------------
constexpr float RNG32::genFloat()
{
    // mask lower 23 bits, multiply by 1/2**23.
    return (genInt32() & ((1 << 23) - 1)) * 0.00000011920928955078125f;
}

//----------------------------------------------------------------------------------------------------------------------
constexpr float RNG32::genFloat( float rmin, float rmax )
{
    if ( rmin == rmax )
        return rmin;

    return (genFloat() * (rmax - rmin) + rmin);
}

//----------------------------------------------------------------------------------------------------------------------
constexpr float RNG32::genGaussApprox()
{
    // sum-of-uniforms / central limit theorem; a fast way to produce an approximate bell curve distribution
    float r0 = genFloat();
    float r1 = genFloat();
    float r2 = genFloat();
    float r3 = genFloat();
    return (r0 + r1 + r2 + r3) * 0.25f;
}

//----------------------------------------------------------------------------------------------------------------------
constexpr int32_t RNG32::genInt32()
{
    return (int32_t)genUInt32();
}

//----------------------------------------------------------------------------------------------------------------------
constexpr int32_t RNG32::genInt32( int32_t rmin, int32_t rmax )
{
    if ( rmin == rmax )
        return rmin;

    auto uRange = ((uint64_t)rmax - (uint64_t)rmin + 1);
    auto uRand  = (uint64_t)genUInt32();

    uRand = (uRand * uRange) >> 32;
    int32_t result = rmin + (int32_t)uRand;

    return result;
}

//----------------------------------------------------------------------------------------------------------------------
constexpr uint32_t RNG32::genUInt32()
{
    /*
    http://cliodhna.cop.uop.edu/~hetrick/na_faq.html

    [George Marsaglia]:
    Here is a simple version, one so simple and good I predict
    it will be the system generator for many future computers:
    x(n)=a*x(n-1)+carry mod 2^32
    With multiplier 'a' chosen from a large set of easily found
    integers, the period is a*2^31-1, around 2^60, and
    I have yet to find a test it will not pass!

    The 'carry' works like this, which shows how well this
    method will serve as a system RNG32:
    Have seed x and c.  Form a*x+c in 64 bits.  All modern
    CPU's have instructions for this: sparc, intel, etc.
    Then the new x is the bottom 32 bits.
    the new carry is the top 32 bits.

    The period is the order of b=2^32 in the group of residues
    relatively prime to m=a*2^32-1.  One need only choose a's
    of some 30 bits for which m=a*2^32-1 is a safeprime:
    both m and (m-1)/2 are prime.  There are thousands of them.

    In general, for any choice of 'a', let m=a*2^32-1.  If both m
    and (m-1)/2 are prime then the period will be (m-1)/2.
    Even if 'a' is, say, your social security number, the
    period will most likely be on the order of 2^50 or more.
    (For mine, it is 2^54.8).

    For 32-bit generators, some possible values of 'a' are:
    1967773755 1517746329 1447497129 1655692410 1606218150
    2051013963 1075433238 1557985959 1781943330 1893513180
    */

    uint64_t temp = (uint64_t)1517746329 * m_seed[0] + m_seed[1];
    m_seed[0] = uint32_t( temp & ~0u );
    m_seed[1] = uint32_t( (temp >> 32) & ~0u );

    return m_seed[0];
}

} // namespace math