#pragma once

#include <stdint.h>
#include <assert.h>
#include "ExponentialRelativeCodec.h"

struct EncoderResponseConfig
{
    static constexpr double x_baseStep = 1.0 / 32768.0;
    static constexpr uint32_t x_frameUs = 33334;
    static constexpr int32_t x_accelerationCoefficient = 18;
    static constexpr uint32_t x_decayUs = 100000;
    static constexpr int32_t x_batchLimit = 30;
    // A frame intersects an extra millis bucket; variable receipt latency can
    // bring one earlier scan into the frame as well. Each knob is visited once.
    //
    static constexpr int32_t x_maxFramePulses = (x_frameUs + 999) / 1000 + 2;
    // In one decay interval, emitted batches can contain the reports received
    // during that interval plus one pending frame, including partial flushes.
    // With increment <= coefficient*pulses/8 and decay 2/3, 3*increment is an
    // invariant magnitude bound from reset. This is an assertion, not a gain cap.
    //
    static constexpr int32_t x_maxDecayPulses = (x_decayUs + 999) / 1000 + 2;
    static constexpr int32_t x_incrementPerDecayBound =
        x_accelerationCoefficient * (x_maxDecayPulses + x_maxFramePulses) / 8;
    static constexpr int32_t x_accelerationBound = 3 * x_incrementPerDecayBound;
    static_assert(x_accelerationBound <= INT32_MAX / 2, "Decay arithmetic must fit");
    static_assert(x_frameUs > 0 && x_decayUs > 0, "Intervals must be positive");
    static_assert(x_batchLimit * x_accelerationBound * x_baseStep <=
                  ExponentialRelativeCodec::x_maximumInput, "Revalidate codec bound after tuning");
};

// Service this clock at least once per half uint32 range (about 35 minutes).
// Retained boundaries can predate the previous call; longer gaps are not supported.
//
struct EncoderResponse
{
    int32_t m_acceleration = 0;
    int32_t m_framePulses = 0;
    int8_t m_lastDirection = 0;
    bool m_initialized = false;
    uint32_t m_frameUs = 0;
    uint32_t m_decayUs = 0;
    // Logical emission time, available to observers without extending AddMovement.
    //
    uint32_t m_boundaryUs = 0;

    void Reset(uint32_t nowUs)
    {
        m_acceleration = 0;
        m_framePulses = 0;
        m_lastDirection = 0;
        m_initialized = true;
        m_frameUs = nowUs;
        m_decayUs = nowUs;
        m_boundaryUs = nowUs;
    }

    // p is raw reported activity. The native input-stage batch limit is separate
    // from the uncapped profile accumulator. EmitFrame clears the entire batch.
    // Direct recurrence callers must honor the same report-rate/clock contract.
    //
    double ApplyFrame(int32_t p)
    {
        assert(p >= -EncoderResponseConfig::x_maxFramePulses &&
               p <= EncoderResponseConfig::x_maxFramePulses);
        const int32_t q = p > EncoderResponseConfig::x_batchLimit ? EncoderResponseConfig::x_batchLimit :
                          p < -EncoderResponseConfig::x_batchLimit ? -EncoderResponseConfig::x_batchLimit : p;
        if (q == 0) return 0.0;
        if ((q < 0 && m_acceleration > 0) || (q > 0 && m_acceleration < 0))
        {
            m_acceleration = 0;
        }
        const int32_t magnitude = m_acceleration < 0 ? -m_acceleration : m_acceleration;
        const double movement = EncoderResponseConfig::x_baseStep * q * (magnitude > 1 ? magnitude : 1);
        if (q > 1 || q < -1 || magnitude > 1)
        {
            m_acceleration += EncoderResponseConfig::x_accelerationCoefficient * q / 8;
            assert(m_acceleration >= -EncoderResponseConfig::x_accelerationBound &&
                   m_acceleration <= EncoderResponseConfig::x_accelerationBound);
        }
        return movement;
    }

    template<typename Sink> void EmitFrame(uint32_t boundaryUs, Sink& sink)
    {
        m_boundaryUs = boundaryUs;
        if (m_framePulses != 0)
        {
            const double movement = ApplyFrame(m_framePulses);
            m_framePulses = 0;
            sink.AddMovement(movement);
        }
    }

    template<typename Sink> void Advance(uint32_t nowUs, Sink& sink)
    {
        if (!m_initialized) Reset(nowUs);
        while (m_framePulses != 0 || m_acceleration != 0)
        {
            const uint32_t frameElapsed = nowUs - m_frameUs;
            const uint32_t decayElapsed = nowUs - m_decayUs;
            const bool frameDue = frameElapsed >= EncoderResponseConfig::x_frameUs;
            const bool decayDue = decayElapsed >= EncoderResponseConfig::x_decayUs;
            if (!frameDue && !decayDue) return;
            // Larger overdue duration is earlier; decay wins equal timestamps.
            //
            if (decayDue && (!frameDue || decayElapsed - EncoderResponseConfig::x_decayUs >=
                                          frameElapsed - EncoderResponseConfig::x_frameUs))
            {
                m_decayUs += EncoderResponseConfig::x_decayUs;
                m_acceleration = 2 * m_acceleration / 3;
            }
            else
            {
                m_frameUs += EncoderResponseConfig::x_frameUs;
                EmitFrame(m_frameUs, sink);
            }
        }
        // No movement or gain remains: jump empty boundaries in constant time.
        //
        m_frameUs += ((nowUs - m_frameUs) / EncoderResponseConfig::x_frameUs) * EncoderResponseConfig::x_frameUs;
        m_decayUs += ((nowUs - m_decayUs) / EncoderResponseConfig::x_decayUs) * EncoderResponseConfig::x_decayUs;
    }

    template<typename Sink> void Flush(uint32_t nowUs, Sink& sink)
    {
        Advance(nowUs, sink);
        EmitFrame(nowUs, sink);
    }

    // The adapter must supply exactly -1/+1 and at most x_maxFramePulses
    // same-direction pulses per frame. In any interval T, at most ceil(T/1000)+2
    // reports may arrive, including receipt-time jitter; partial Flush does not
    // waive this physical scan-rate precondition.
    //
    template<typename Sink> void Pulse(int8_t direction, uint32_t nowUs, Sink& sink)
    {
        assert(direction == -1 || direction == 1);
        Advance(nowUs, sink);
        if (m_lastDirection != 0 && direction != m_lastDirection)
        {
            EmitFrame(nowUs, sink);
            m_acceleration = 0;
        }
        m_lastDirection = direction;
        m_framePulses += direction;
        assert(m_framePulses >= -EncoderResponseConfig::x_maxFramePulses &&
               m_framePulses <= EncoderResponseConfig::x_maxFramePulses);
    }
};
