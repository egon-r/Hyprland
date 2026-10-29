#include "MotionBlur.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>

using namespace MotionBlur;

CBox MotionBlur::extents(const CBox& previous, const CBox& current) {
    const double x1 = std::min(previous.x, current.x);
    const double y1 = std::min(previous.y, current.y);
    const double x2 = std::max(previous.x + previous.w, current.x + current.w);
    const double y2 = std::max(previous.y + previous.h, current.y + current.h);

    return {x1, y1, x2 - x1, y2 - y1};
}

// Scaled sizes are floored to stay positive even when `strength` would extrapolate them below zero (e.g. a window that grew while
// `strength > 1`), keeping the smear geometry and its consumers valid.
constexpr double MIN_SCALED_SIZE = 0.1;

CBox             MotionBlur::scaled(const CBox& current, const CBox& previous, double strength) {
    // an invalid or unchanged strength preserves the recorded motion exactly
    if (!std::isfinite(strength) || strength == 1.0)
        return previous;

    const Vector2D DELTA = previous.pos() - current.pos();
    const Vector2D SIZE  = previous.size() - current.size();

    CBox           scaled = current.copy();
    scaled.x += DELTA.x * strength;
    scaled.y += DELTA.y * strength;
    scaled.w = std::max(scaled.w + SIZE.x * strength, MIN_SCALED_SIZE);
    scaled.h = std::max(scaled.h + SIZE.y * strength, MIN_SCALED_SIZE);

    return scaled;
}

CBox SState::extents() const {
    return MotionBlur::extents(previous, current);
}

void CTracker::record(const CBox& previous, const CBox& current) {
    m_previous  = previous;
    m_current   = current;
    m_updatedAt = Time::steadyNow();
    m_valid     = true;
}

void CTracker::reset() {
    m_previous = {};
    m_current  = {};
    m_valid    = false;
}

// upper bound matches the decoration:motion_blur:strength config range
constexpr double      MAX_STRENGTH = 10;

std::optional<SState> CTracker::state(int samples, const Vector2D& offset, bool allowStale, double strength) const {
    if (!m_valid)
        return std::nullopt;

    const float AGE = std::chrono::duration<float, std::milli>(Time::steadyNow() - m_updatedAt).count();
    if (!allowStale && AGE > 100.F)
        return std::nullopt;

    CBox previous = m_previous.copy().translate(offset);
    CBox current  = m_current.copy().translate(offset);

    if (previous == current || previous.w <= 0.F || previous.h <= 0.F || current.w <= 0.F || current.h <= 0.F)
        return std::nullopt;

    return SState{
        .previous = scaled(current, previous, std::clamp(strength, 0.0, MAX_STRENGTH)),
        .current  = current,
        .samples  = samples,
    };
}
