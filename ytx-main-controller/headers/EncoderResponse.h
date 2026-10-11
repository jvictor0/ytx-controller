#pragma once

#include <stdint.h>
#include <assert.h>
#include "ExponentialRelativeCodec.h"

struct EncoderResponseConfig
{
    static constexpr double x_baseStep = 1.0 / 1024.0;
    static constexpr uint32_t x_frameUs = 8000;
    static constexpr int32_t x_accelerationCoefficient = 18;
    static constexpr uint32_t x_decayUs = 10000;
    static constexpr int32_t x_maxGain = 16;
    // A frame can intersect one more millis bucket than its rounded-up duration.
    //
    static constexpr int32_t x_maxFramePulses = (x_frameUs + 999) / 1000 + 1;
    static_assert(x_frameUs > 0 && x_decayUs > 0, "Intervals must be positive");
    static_assert(x_maxFramePulses * x_maxGain * x_baseStep <=
                  ExponentialRelativeCodec::x_maximumInput, "Revalidate codec bound after tuning");
};

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

    double ApplyFrame(int32_t q)
    {
        assert(q >= -EncoderResponseConfig::x_maxFramePulses &&
               q <= EncoderResponseConfig::x_maxFramePulses);
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
            if (m_acceleration > EncoderResponseConfig::x_maxGain) m_acceleration = EncoderResponseConfig::x_maxGain;
            if (m_acceleration < -EncoderResponseConfig::x_maxGain) m_acceleration = -EncoderResponseConfig::x_maxGain;
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
