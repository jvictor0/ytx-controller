#include "ExponentialRelativeCodec.h"
#include "EncoderResponse.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>
#include <cstring>
#include <algorithm>

struct Sink
{
    std::vector<uint8_t> m_values;
    long double m_sum = 0.0;
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
    // Summation and repeated residual subtraction scale with packet count.
    const double roundoffBound = 8 * std::numeric_limits<double>::epsilon() *
        (sink.m_values.size() + 2) * std::max(1.0, std::abs(before) + std::abs(delta));
    assert(std::abs(sink.m_sum + codec.m_remainder - before - delta) < roundoffBound);
    assert(std::abs(codec.m_remainder) < 1.0 / 65536.0);
    for (uint8_t value : sink.m_values)
    {
        assert((value > 64) == (before + delta > 0.0));
    }
    assert(sink.m_values.size() <= ExponentialRelativeCodec::x_maximumPackets);
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
    const int movement[] = {1, 2, 4, 8, -2};
    const int acceleration[] = {0, 2, 4, 6, -2};
    for (unsigned i = 0; i < 5; ++i)
    {
        assert(response.ApplyFrame(frames[i]) == movement[i] * EncoderResponseConfig::x_baseStep);
        assert(response.m_acceleration == acceleration[i]);
    }
    for (int raw : {-36, -31, -30, 30, 31, 36})
    {
        response.Reset(0);
        const int direction = raw < 0 ? -1 : 1;
        assert(response.ApplyFrame(raw) == direction * 30 * EncoderResponseConfig::x_baseStep);
        assert(response.m_acceleration == direction * 30);
        assert(response.ApplyFrame(0) == 0 && response.m_acceleration == direction * 30);
    }
    response.Reset(0);
    response.m_acceleration = 12;
    MovementSink sink{&response, {}, {}};
    const int decay[] = {8, 5, 3, 2, 1, 0};
    for (unsigned i = 0; i < 6; ++i)
    {
        response.Advance((i + 1) * 100000, sink);
        assert(response.m_acceleration == decay[i]);
        assert(sink.m_deltas.empty());
    }
}

struct PipelineSink
{
    ExponentialRelativeCodec m_codec;
    Sink m_wire;
    long double m_requested = 0.0;
    void AddMovement(double delta)
    {
        m_requested += delta;
        m_codec.Add(delta, m_wire);
        assert(std::abs(m_wire.m_sum + m_codec.m_remainder - m_requested) < 1e-12);
    }
};

static MovementSink Trace(EncoderResponse& response, bool frequent, uint32_t origin)
{
    response.Reset(origin);
    MovementSink sink{&response, {}, {}};
    const uint32_t times[] = {0, 1000, 2000, 32000, 33334, 34000, 65000,
                              66668, 67000, 98000, 100000, 100002, 101000,
                              102000, 3001000, 3002000};
    uint32_t previous = 0;
    for (uint32_t time : times)
    {
        if (frequent)
        {
            for (uint32_t t = previous; t < time; t += 137)
                response.Advance(origin + t, sink);
        }
        response.Pulse(time >= 101000 && time < 3000000 ? -1 : 1, origin + time, sink);
        previous = time;
    }
    response.Flush(origin + 3003000, sink);
    response.Advance(origin + 1000000000, sink);
    assert(response.m_acceleration == 0 && response.m_framePulses == 0);
    return sink;
}

static void CheckTiming()
{
    const double step = EncoderResponseConfig::x_baseStep;
    assert(step == 1.0 / 1500.0);
    assert(EncoderResponseConfig::x_accelerationCoefficient == 8);
    assert(EncoderResponseConfig::x_frameUs == 33334);
    assert(EncoderResponseConfig::x_decayUs == 100000);
    assert(EncoderResponseConfig::x_batchLimit == 30);
    assert(EncoderResponseConfig::x_accelerationBound == 414);
    EncoderResponse a, b;
    MovementSink frequent = Trace(a, true, 0);
    MovementSink delayed = Trace(b, false, 0);
    assert(frequent.m_deltas == delayed.m_deltas);
    assert(frequent.m_times == delayed.m_times);
    assert(frequent.m_deltas[0] == 4 * step);
    assert(std::abs(frequent.m_deltas[1] - 12 * step) < 1e-15);
    assert(frequent.m_deltas.back() == 2 * step);
    MovementSink wrapped = Trace(b, false, UINT32_MAX - 19000);
    assert(frequent.m_deltas == wrapped.m_deltas);
    for (size_t i = 0; i < wrapped.m_times.size(); ++i)
        assert(wrapped.m_times[i] - (UINT32_MAX - 19000) == frequent.m_times[i]);

    a.Reset(0);
    MovementSink sink{&a, {}, {}};
    a.Pulse(1, 0, sink);
    a.Advance(33333, sink);
    assert(sink.m_deltas.empty());
    a.Pulse(1, 33334, sink);
    assert(a.m_initialized && sink.m_times[0] == 33334);
    a.Advance(66668, sink);
    assert(sink.m_deltas == std::vector<double>({step, step}));

    // The periods have their first common boundary at 1,666,700,000 us.
    //
    a.Reset(0);
    sink.m_deltas.clear(); sink.m_times.clear();
    const uint32_t tie = 1666700000;
    a.m_frameUs = tie - 33334; a.m_decayUs = tie - 100000;
    a.m_acceleration = 12;
    a.Pulse(1, tie - 1, sink);
    a.Advance(tie, sink);
    assert(sink.m_deltas.back() == 8 * step);

    a.Reset(0);
    sink.m_deltas.clear(); sink.m_times.clear();
    a.m_acceleration = 134;
    a.Pulse(1, 1, sink);
    a.Pulse(-1, 2, sink);
    a.Flush(3, sink);
    assert(sink.m_deltas == std::vector<double>({134 * step, -step}));
    assert(sink.m_times == std::vector<uint32_t>({2, 3}));
    assert(a.m_acceleration == 0);

    // The native input clamp clears the complete raw batch, including surplus.
    // This intentional input saturation is distinct from loss in the wire drain.
    //
    for (int direction : {-1, 1})
    {
        a.Reset(0); sink.m_deltas.clear(); sink.m_times.clear();
        for (uint32_t t = 0; t < 33000; t += 1000) a.Pulse(direction, t, sink);
        a.Advance(33334, sink);
        assert(sink.m_deltas.back() == direction * 30 * step);
        assert(a.m_framePulses == 0 && a.m_acceleration == direction * 30);
        for (uint32_t t = 34000; t < 66000; t += 1000) a.Pulse(direction, t, sink);
        a.Advance(66668, sink);
        assert(std::abs(sink.m_deltas.back() - direction * 900 * step) < 1e-15);
        assert(a.m_acceleration == direction * 60);
        for (uint32_t t = 67000; t < 100000; t += 1000) a.Pulse(direction, t, sink);
        a.Advance(100002, sink);
        assert(std::abs(sink.m_deltas.back() - direction * 1200 * step) < 1e-15);
        assert(a.m_acceleration == direction * 70);
    }

    // Sustained input reaches the decay-limited fixed point, without a gain cap.
    // Partial flushes also remain within the bound derived from raw report rate.
    //
    for (int direction : {-1, 1})
    {
        a.Reset(0); sink.m_deltas.clear(); sink.m_times.clear();
        int peak = 0;
        for (uint32_t t = 0; t < 30000000; t += 1000)
        {
            a.Pulse(direction, t, sink);
            peak = std::max(peak, std::abs(a.m_acceleration));
            assert(std::abs(a.m_acceleration) <= EncoderResponseConfig::x_accelerationBound);
        }
        assert(peak == 268);
        a.Flush(30000000, sink);
        a.Advance(1030000000, sink);
        assert(a.m_acceleration == 0 && a.m_framePulses == 0);
        a.Pulse(direction, 1030000001, sink); a.Flush(1030000002, sink);
        assert(sink.m_deltas.back() == direction * step);
        for (double delta : sink.m_deltas)
            assert(std::abs(delta) <= ExponentialRelativeCodec::x_maximumInput);
        a.Reset(0);
        for (uint32_t t = 0; t < 2000000; t += 1000)
        {
            a.Pulse(direction, t, sink);
            if ((t / 1000) % 7 == 0) a.Flush(t, sink);
            assert(std::abs(a.m_acceleration) <= EncoderResponseConfig::x_accelerationBound);
        }
    }
    PipelineSink pipeline;
    a.Reset(0); b.Reset(0);
    for (uint32_t t = 0; t < 1000000; t += 1000)
    {
        a.Pulse(t < 500000 ? 1 : -1, t, pipeline);
        assert(b.m_acceleration == 0 && b.m_framePulses == 0);
    }
    a.Flush(1000000, pipeline);
    std::printf("Response recurrence, timing, rollover, idle, independence and pipeline passed; sizeof = %zu, pulse bound = %d, batch limit = 30, acceleration bound = %d.\n",
                sizeof(EncoderResponse), EncoderResponseConfig::x_maxFramePulses,
                EncoderResponseConfig::x_accelerationBound);
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
        if (std::strcmp(argv[1], "oversize") == 0) invalidCodec.Add(std::nextafter(ExponentialRelativeCodec::x_maximumInput, INFINITY), invalidSink);
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
    assert(EncoderResponseConfig::x_maxFramePulses == 36);
    assert(ExponentialRelativeCodec::x_maximumInput == 30.0 * 414 * EncoderResponseConfig::x_baseStep);
    const double largestPositive = ExponentialRelativeCodec::Decode(127);
    const double alpha = 1.0 - std::pow(2.0, -1.0 / 8.0);
    assert(ExponentialRelativeCodec::x_maximumInput + minimum < 211 * largestPositive);
    assert(minimum + (largestPositive - minimum) * std::pow(alpha, 4) < 2 * minimum);
    assert(ExponentialRelativeCodec::x_maximumPackets == 210 + 5);
    size_t maximum = 0;
    for (int q = -EncoderResponseConfig::x_batchLimit; q <= EncoderResponseConfig::x_batchLimit; ++q)
    {
        for (int gain = 1; gain <= EncoderResponseConfig::x_accelerationBound; ++gain)
        {
            for (double residual : {0.0, std::nextafter(minimum, 0.0), -std::nextafter(minimum, 0.0)})
            {
                codec.Reset();
                codec.m_remainder = residual;
                const size_t packets = CheckDrain(codec, q * gain * EncoderResponseConfig::x_baseStep);
                if (packets > maximum) maximum = packets;
            }
        }
    }
    for (int i = -100000; i <= 100000; ++i)
    {
        codec.Reset();
        codec.m_remainder = ((i + 100000) % 3 - 1) * std::nextafter(minimum, 0.0);
        const size_t packets = CheckDrain(codec, i * ExponentialRelativeCodec::x_maximumInput / 100000.0);
        if (packets > maximum) maximum = packets;
    }
    codec.Reset();
    for (int i = 0; i < 10000; ++i) CheckDrain(codec, minimum / 7.0);
    codec.Reset();
    for (int i = 0; i < 10000; ++i) CheckDrain(codec, 1.0 * EncoderResponseConfig::x_baseStep);
    codec.Reset();
    for (int i = 0; i < 10000; ++i) CheckDrain(codec, (i % 2 ? -1.0 : 1.0) * EncoderResponseConfig::x_baseStep);
    CheckDrain(codec, 0.0);
    codec.Reset();
    Sink sink;
    codec.Add(minimum * 0.75, sink);
    codec.Add(-minimum * 0.75, sink);
    assert(sink.m_values.empty() && codec.m_remainder == 0.0);
    codec.Add(1.0 * EncoderResponseConfig::x_baseStep, sink);
    const size_t count = sink.m_values.size();
    codec.Add(-1.0 * EncoderResponseConfig::x_baseStep, sink);
    assert(count > 0 && sink.m_values.size() > count);
    assert(std::abs(sink.m_sum + codec.m_remainder) < 8e-16);
    std::printf("Codec boundaries, conservation and dense sweep passed; maximum packets = %zu (ceiling %u).\n", maximum, ExponentialRelativeCodec::x_maximumPackets);
}
