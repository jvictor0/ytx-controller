#pragma once

#include <stdint.h>
#include <math.h>
#include <assert.h>

struct ExponentialRelativeCodec
{
    static constexpr double x_minimumDelta = 1.0 / 65536.0;
    static constexpr double x_codesPerDoubling = 8.0;
    // Adapter bounds only; the v1 Decode/EncodeFloor equations are unchanged.
    // At most 21 largest positive packets plus five shrinking-tail packets.
    //
    static constexpr double x_maximumInput = 30.0 * 930.0 / 32768.0;
    static constexpr unsigned x_maximumPackets = 26;
    double m_remainder = 0.0;

    static double Decode(uint8_t value)
    {
        assert(value <= 127);
        const int z = static_cast<int>(value) - 64;
        if (z == 0) return 0.0;
        const int magnitude = z > 0 ? z : -z;
        const double delta = x_minimumDelta * expm1(log(2.0) * magnitude / x_codesPerDoubling)
                             / expm1(log(2.0) / x_codesPerDoubling);
        return z > 0 ? delta : -delta;
    }

    static uint8_t EncodeFloor(double magnitude, bool positive)
    {
        assert(isfinite(magnitude) && magnitude >= 0.0);
        if (magnitude < x_minimumDelta) return 64;
        const int maximum = positive ? 63 : 64;
        const double inverse = x_codesPerDoubling * log1p(
            magnitude * expm1(log(2.0) / x_codesPerDoubling) / x_minimumDelta) / log(2.0);
        int candidate = inverse >= maximum ? maximum : static_cast<int>(floor(inverse));
        const int sign = positive ? 1 : -1;
        if (candidate > 0 && fabs(Decode(static_cast<uint8_t>(64 + sign * candidate))) > magnitude)
        {
            --candidate;
        }
        if (candidate < maximum && fabs(Decode(static_cast<uint8_t>(64 + sign * (candidate + 1)))) <= magnitude)
        {
            ++candidate;
        }
        return static_cast<uint8_t>(64 + sign * candidate);
    }

    void Reset()
    {
        m_remainder = 0.0;
    }

    template<typename Sink> void Add(double delta, Sink& sink)
    {
        assert(isfinite(delta) && fabs(delta) <= x_maximumInput);
        assert(isfinite(m_remainder) && fabs(m_remainder) < x_minimumDelta);
        m_remainder += delta;
        for (unsigned packet = 0; packet < x_maximumPackets; ++packet)
        {
            const uint8_t value = EncodeFloor(fabs(m_remainder), m_remainder > 0.0);
            if (value == 64) return;
            sink.Emit(value);
            m_remainder -= Decode(value);
        }
        assert(fabs(m_remainder) < x_minimumDelta);
    }
};
