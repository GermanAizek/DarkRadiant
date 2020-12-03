#pragma once

#include <wx/stopwatch.h>
#include "string/string.h"

namespace render
{

/// Storage class for per-frame render statistics
class RenderStatistics
{
    // Timer for measuring render time
    wxStopWatch _timer;

    // Time for the render front-end only
    long _feTime = 0;

    // Count of lights
    int _visibleLights = 0;
    int _totalLights = 0;

public:

    /// Return the constructed string for display
    std::string getStatString()
    {
        // Calculate times for render front-end and back-end
        long totTime = _timer.Time();
        long beTime = totTime - _feTime;

        return "lights: " + std::to_string(_visibleLights)
             + " / " + std::to_string(_totalLights)
             + " | f/e: " + std::to_string(_feTime) + " ms"
             + " | b/e: " + std::to_string(beTime) + " ms"
             + " | tot: " + std::to_string(totTime) + " ms"
             + " | fps: " + (totTime > 0 ? std::to_string(1000 / totTime) : "-");
    }

    /// Mark the front-end render stage as completed, storing the time internally
    void frontEndComplete()
    {
        _feTime = _timer.Time();
    }

    /// Set the light count
    void setLightCount(int visible, int total)
    {
        _visibleLights += visible;
        _totalLights += total;
    }

    /// Reset statistics at the beginning of a frame render
    void resetStats()
    {
        _visibleLights = _totalLights = 0;

        _feTime = 0;
        _timer.Start();
    }
};

} // namespace render
