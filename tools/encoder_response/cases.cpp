#include "ExponentialRelativeCodec.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>
#include <cstring>

struct Sink
{
    std::vector<uint8_t> m_values;
    double m_sum = 0.0;
    void Emit(uint8_t value)
    {
        assert(value <= 127 && value != 64);
        m_values.push_back(value);
        m_sum += ExponentialRelativeCodec::Decode(value);
    }
};

static size_t CheckDrain(ExponentialRelativeCodec& codec, double delta)
{
    const double before = codec.m_remainder;
    Sink sink;
    codec.Add(delta, sink);
    assert(std::abs(sink.m_sum + codec.m_remainder - before - delta) < 2e-16);
    assert(std::abs(codec.m_remainder) < 1.0 / 65536.0);
    for (uint8_t value : sink.m_values)
    {
        assert((value > 64) == (before + delta > 0.0));
    }
    assert(sink.m_values.size() <= 12);
    return sink.m_values.size();
}

int main(int argc, char** argv)
{
    if (argc > 1)
    {
        ExponentialRelativeCodec invalidCodec;
        Sink invalidSink;
        if (std::strcmp(argv[1], "nan") == 0) invalidCodec.Add(NAN, invalidSink);
        if (std::strcmp(argv[1], "infinity") == 0) invalidCodec.Add(INFINITY, invalidSink);
        if (std::strcmp(argv[1], "oversize") == 0) invalidCodec.Add(0.15, invalidSink);
        if (std::strcmp(argv[1], "bad_remainder") == 0)
        {
            invalidCodec.m_remainder = 1.0;
            invalidCodec.Add(0.0, invalidSink);
        }
        if (std::strcmp(argv[1], "negative_magnitude") == 0)
        {
            ExponentialRelativeCodec::EncodeFloor(-1.0, true);
        }
        return 0;
    }
    const double minimum = 1.0 / 65536.0;
    assert(ExponentialRelativeCodec::Decode(64) == 0.0);
    assert(ExponentialRelativeCodec::Decode(65) == minimum);
    assert(ExponentialRelativeCodec::Decode(63) == -minimum);
    for (int code = 0; code <= 127; ++code)
    {
        const double value = ExponentialRelativeCodec::Decode(static_cast<uint8_t>(code));
        std::printf("%.17g\n", value);
        if (code > 0)
        {
            assert(value > ExponentialRelativeCodec::Decode(static_cast<uint8_t>(code - 1)));
        }
        if (code == 64) continue;
        const bool positive = code > 64;
        const double magnitude = std::abs(value);
        assert(ExponentialRelativeCodec::EncodeFloor(magnitude, positive) == code);
        const uint8_t below = ExponentialRelativeCodec::EncodeFloor(std::nextafter(magnitude, 0.0), positive);
        assert(below == (positive ? code - 1 : code + 1));
        assert(ExponentialRelativeCodec::EncodeFloor(std::nextafter(magnitude, INFINITY), positive) == code);
        const int maximumCode = positive ? 127 : 0;
        if (code != maximumCode)
        {
            const double next = std::abs(ExponentialRelativeCodec::Decode(
                static_cast<uint8_t>(positive ? code + 1 : code - 1)));
            assert(ExponentialRelativeCodec::EncodeFloor((magnitude + next) / 2.0, positive) == code);
        }
    }
    for (int n = 1; n <= 63; ++n)
    {
        assert(ExponentialRelativeCodec::Decode(static_cast<uint8_t>(64 - n)) ==
               -ExponentialRelativeCodec::Decode(static_cast<uint8_t>(64 + n)));
    }
    ExponentialRelativeCodec codec;
    assert(codec.m_remainder == 0.0);
    size_t maximum = 0;
    for (int q = -9; q <= 9; ++q)
    {
        for (int gain = 1; gain <= 16; ++gain)
        {
            for (double residual : {0.0, std::nextafter(minimum, 0.0), -std::nextafter(minimum, 0.0)})
            {
                codec.Reset();
                codec.m_remainder = residual;
                const size_t packets = CheckDrain(codec, q * gain / 1024.0);
                if (packets > maximum) maximum = packets;
            }
        }
    }
    for (int i = -100000; i <= 100000; ++i)
    {
        codec.Reset();
        codec.m_remainder = ((i + 100000) % 3 - 1) * std::nextafter(minimum, 0.0);
        const size_t packets = CheckDrain(codec, i * 0.140625 / 100000.0);
        if (packets > maximum) maximum = packets;
    }
    codec.Reset();
    for (int i = 0; i < 10000; ++i) CheckDrain(codec, minimum / 7.0);
    codec.Reset();
    for (int i = 0; i < 10000; ++i) CheckDrain(codec, 1.0 / 1024.0);
    codec.Reset();
    for (int i = 0; i < 10000; ++i) CheckDrain(codec, (i % 2 ? -1.0 : 1.0) / 1024.0);
    CheckDrain(codec, 0.0);
    codec.Reset();
    Sink sink;
    codec.Add(minimum * 0.75, sink);
    codec.Add(-minimum * 0.75, sink);
    assert(sink.m_values.empty() && codec.m_remainder == 0.0);
    codec.Add(1.0 / 1024.0, sink);
    const size_t count = sink.m_values.size();
    codec.Add(-1.0 / 1024.0, sink);
    assert(count > 0 && sink.m_values.size() > count);
    assert(std::abs(sink.m_sum + codec.m_remainder) < 2e-16);
    std::printf("Codec boundaries, conservation and dense sweep passed; maximum packets = %zu (ceiling 12).\n", maximum);
}
