#include "ExponentialRelativeCodec.h"
#include "EncoderResponse.h"
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

struct MovementSink
{
    EncoderResponse* m_response;
    std::vector<double> m_deltas;
    std::vector<uint32_t> m_times;
    void AddMovement(double delta)
    {
        m_deltas.push_back(delta);
        m_times.push_back(m_response->m_boundaryUs);
    }
};

static void CheckResponse()
{
    EncoderResponse response;
    response.Reset(0);
    const int frames[] = {1, 2, 2, 2, -2};
    const int movement[] = {1, 2, 8, 16, -2};
    const int acceleration[] = {0, 4, 8, 12, -4};
    for (unsigned i = 0; i < 5; ++i)
    {
        assert(response.ApplyFrame(frames[i]) == movement[i] / 1024.0);
        assert(response.m_acceleration == acceleration[i]);
    }
    response.Reset(0);
    response.m_acceleration = 12;
    MovementSink sink{&response, {}, {}};
    const int decay[] = {8, 5, 3, 2, 1, 0};
    for (unsigned i = 0; i < 6; ++i)
    {
        response.Advance((i + 1) * 10000, sink);
        assert(response.m_acceleration == decay[i]);
        assert(sink.m_deltas.empty());
    }
}

struct PipelineSink
{
    ExponentialRelativeCodec m_codec;
    Sink m_wire;
    double m_requested = 0.0;
    void AddMovement(double delta)
    {
        m_requested += delta;
        m_codec.Add(delta, m_wire);
        assert(std::abs(m_wire.m_sum + m_codec.m_remainder - m_requested) < 2e-14);
    }
};

static MovementSink Trace(EncoderResponse& response, bool frequent, uint32_t origin)
{
    response.Reset(origin);
    MovementSink sink{&response, {}, {}};
    const uint32_t times[] = {0, 1000, 2000, 7000, 8000, 9000, 15000, 16000,
                              17000, 18000, 24000, 25000, 39000, 40000, 41000,
                              42000, 43000, 48000, 200000, 201000};
    uint32_t previous = 0;
    for (uint32_t time : times)
    {
        if (frequent)
        {
            for (uint32_t t = previous; t < time; t += 137)
            {
                response.Advance(origin + t, sink);
            }
        }
        response.Pulse(time >= 41000 && time < 200000 ? -1 : 1, origin + time, sink);
        previous = time;
    }
    response.Flush(origin + 202000, sink);
    response.Advance(origin + 0xffff0000U, sink);
    assert(response.m_acceleration == 0 && response.m_framePulses == 0);
    return sink;
}

static void CheckTiming()
{
    EncoderResponse a, b;
    MovementSink frequent = Trace(a, true, 0);
    MovementSink delayed = Trace(b, false, 0);
    assert(frequent.m_deltas == delayed.m_deltas);
    assert(frequent.m_times == delayed.m_times);
    assert(frequent.m_deltas[0] == 4.0 / 1024);
    assert(frequent.m_deltas[1] > 3.0 / 1024);
    assert(frequent.m_deltas.back() == 2.0 / 1024);
    MovementSink wrapped = Trace(b, false, UINT32_MAX - 19000);
    assert(frequent.m_deltas == wrapped.m_deltas);
    for (size_t i = 0; i < wrapped.m_times.size(); ++i)
    {
        assert(wrapped.m_times[i] - (UINT32_MAX - 19000) == frequent.m_times[i]);
    }
    a.Reset(0);
    MovementSink sink{&a, {}, {}};
    a.Pulse(1, 0, sink);
    a.Pulse(1, 8000, sink);
    assert(a.m_initialized && sink.m_times[0] == 8000);
    a.Advance(16000, sink);
    assert(sink.m_deltas == std::vector<double>({1.0 / 1024, 1.0 / 1024}));
    a.Reset(0);
    sink.m_deltas.clear(); sink.m_times.clear();
    a.m_acceleration = 12;
    a.Pulse(1, 39000, sink);
    const int32_t before = a.m_acceleration;
    a.Advance(40000, sink);
    assert(sink.m_deltas.back() == (2 * before / 3) / 1024.0);
    a.Reset(0);
    sink.m_deltas.clear(); sink.m_times.clear();
    a.m_acceleration = 12;
    a.Pulse(1, 1, sink);
    a.Pulse(-1, 2, sink);
    a.Flush(3, sink);
    assert(sink.m_deltas == std::vector<double>({12.0 / 1024, -1.0 / 1024}));
    assert(sink.m_times == std::vector<uint32_t>({2, 3}));
    assert(a.m_acceleration == 0);
    a.Reset(0);
    sink.m_deltas.clear(); sink.m_times.clear();
    a.Pulse(1, 1, sink);
    a.Advance(1000000000, sink);
    a.Pulse(1, 1000000001, sink);
    a.Flush(1000000002, sink);
    assert(sink.m_deltas == std::vector<double>({1.0 / 1024, 1.0 / 1024}));
    PipelineSink pipeline;
    a.Reset(0); b.Reset(0);
    for (uint32_t t = 0; t < 1000000; t += 1000)
    {
        a.Pulse(t < 500000 ? 1 : -1, t, pipeline);
        assert(std::abs(a.m_acceleration) <= EncoderResponseConfig::x_maxGain);
        assert(b.m_acceleration == 0 && b.m_framePulses == 0);
    }
    a.Flush(1000000, pipeline);
    for (int i = 0; i < 100; ++i)
    {
        assert(a.ApplyFrame(-9) == -9 * 16.0 / 1024);
        assert(a.m_acceleration == -16);
    }
    assert(a.ApplyFrame(0) == 0.0 && a.m_acceleration == -16);
    std::printf("Response recurrence, timing, rollover, idle, independence and pipeline passed; sizeof = %zu, pulse bound = %d.\n",
                sizeof(EncoderResponse), EncoderResponseConfig::x_maxFramePulses);
}

int main(int argc, char** argv)
{
    if (argc > 1)
    {
        if (std::strcmp(argv[1], "bad_direction") == 0)
        {
            EncoderResponse response;
            MovementSink sink{&response, {}, {}};
            response.Pulse(0, 0, sink);
        }
        if (std::strcmp(argv[1], "bad_frame") == 0)
        {
            EncoderResponse response;
            response.ApplyFrame(EncoderResponseConfig::x_maxFramePulses + 1);
        }
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
    CheckResponse();
    CheckTiming();
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
