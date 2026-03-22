#pragma once

#include <Windows.h>

#include <cstdint>

namespace showcase {

class Timer {
public:
    Timer();

    void Reset();
    void Tick();
    void Pause();
    void Resume();

    float DeltaTime() const { return static_cast<float>(m_deltaTime); }
    float TotalTime() const;
    float FPS() const { return m_fps; }
    uint64_t FrameCount() const { return m_frameCount; }
    bool IsPaused() const { return m_paused; }

private:
    double m_secondsPerCount = 0.0;
    double m_deltaTime = 0.0;

    int64_t m_baseTime = 0;
    int64_t m_pausedTime = 0;
    int64_t m_stopTime = 0;
    int64_t m_prevTime = 0;
    int64_t m_currTime = 0;

    bool m_paused = false;

    // FPS calculation
    float m_fps = 0.0f;
    uint64_t m_frameCount = 0;
    float m_fpsAccumulator = 0.0f;
    uint32_t m_fpsFrameCount = 0;
};

} // namespace showcase
