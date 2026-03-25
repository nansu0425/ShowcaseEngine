#include <showcase/core/Timer.h>

#include <showcase/core/Profiler.h>

namespace showcase {

// ── Lifecycle ─────────────────────────────────────────────────────
Timer::Timer() {
    int64_t countsPerSec;
    QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&countsPerSec));
    m_secondsPerCount = 1.0 / static_cast<double>(countsPerSec);
}

void Timer::Reset() {
    int64_t currTime;
    QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currTime));

    m_baseTime = currTime;
    m_prevTime = currTime;
    m_stopTime = 0;
    m_pausedTime = 0;
    m_paused = false;
    m_frameCount = 0;
    m_fps = 0.0f;
    m_fpsAccumulator = 0.0f;
    m_fpsFrameCount = 0;
}

// ── Frame timing ──────────────────────────────────────────────────
void Timer::Tick() {
    SE_ZONE_SCOPED_C(profile::kColorUpdate);
    if (m_paused) {
        m_deltaTime = 0.0;
        return;
    }

    QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&m_currTime));

    m_deltaTime = (m_currTime - m_prevTime) * m_secondsPerCount;
    m_prevTime = m_currTime;

    // Clamp negative delta (e.g., after debugger pause)
    if (m_deltaTime < 0.0) {
        m_deltaTime = 0.0;
    }

    m_frameCount++;

    // FPS calculation (update every 0.5 seconds)
    m_fpsFrameCount++;
    m_fpsAccumulator += static_cast<float>(m_deltaTime);
    if (m_fpsAccumulator >= 0.5f) {
        m_fps = static_cast<float>(m_fpsFrameCount) / m_fpsAccumulator;
        m_fpsFrameCount = 0;
        m_fpsAccumulator = 0.0f;
    }
}

// ── Pause / Resume ────────────────────────────────────────────────
void Timer::Pause() {
    if (!m_paused) {
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&m_stopTime));
        m_paused = true;
    }
}

void Timer::Resume() {
    if (m_paused) {
        int64_t startTime;
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&startTime));

        m_pausedTime += (startTime - m_stopTime);
        m_prevTime = startTime;
        m_stopTime = 0;
        m_paused = false;
    }
}

// ── Query ─────────────────────────────────────────────────────────
float Timer::TotalTime() const {
    if (m_paused) {
        return static_cast<float>(((m_stopTime - m_pausedTime) - m_baseTime) * m_secondsPerCount);
    }
    return static_cast<float>(((m_currTime - m_pausedTime) - m_baseTime) * m_secondsPerCount);
}

} // namespace showcase
