#include "MotionBlur.hpp"

#include "memory/Memory.hpp"

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

CBox MotionBlur::composedRecordBox(const CBox& layoutBox, const Vector2D& workspaceRenderOffset, const Vector2D& floatingOffset, bool pinned) {
    return layoutBox.copy().translate((pinned ? Vector2D{} : workspaceRenderOffset) + floatingOffset);
}

MotionBlur::ESampleDecision MotionBlur::sampleDecision(const std::optional<CBox>& lastCurrent, bool lastExpired, const CBox& current, const CBox& bounds) {
    if (!lastCurrent)
        return ESampleDecision::ANCHOR;

    // an event-driven record within the same frame, or a fully idle window: the recorded pair is already authoritative
    if (*lastCurrent == current)
        return ESampleDecision::SKIP;

    // stale motion (occlusion gaps, teleports) or a previous sample that left the monitor entirely (off-screen transit of an
    // incoming workspace): anchor instead of smearing the whole jump on the first visible frame
    if (lastExpired || lastCurrent->intersection(bounds).empty())
        return ESampleDecision::ANCHOR;

    return ESampleDecision::RECORD;
}

bool MotionBlur::eligible(bool enabled, int samples, double strength, bool fullscreenActive, bool allowFullscreen) {
    return enabled && samples > 1 && strength > 0 && (allowFullscreen || !fullscreenActive);
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

void CTracker::record(const CBox& previous, const CBox& current, Time::steady_tp now) {
    m_previous  = previous;
    m_current   = current;
    m_updatedAt = now;
    m_valid     = true;
}

std::optional<SRecordedMotion> CTracker::recorded() const {
    if (!m_valid)
        return std::nullopt;

    return SRecordedMotion{.previous = m_previous, .current = m_current};
}

void CTracker::reanchor(const CBox& current) {
    if (!m_valid)
        return;

    // translate the whole recorded displacement (position and size) onto the actual final box; `m_updatedAt` is deliberately
    // left untouched
    const Vector2D POS_DELTA  = current.pos() - m_current.pos();
    const Vector2D SIZE_DELTA = current.size() - m_current.size();

    m_previous.translate(POS_DELTA);
    m_previous.w = std::max(m_previous.w + SIZE_DELTA.x, MIN_SCALED_SIZE);
    m_previous.h = std::max(m_previous.h + SIZE_DELTA.y, MIN_SCALED_SIZE);

    m_current = current;
}

void CTracker::reset() {
    m_previous = {};
    m_current  = {};
    m_valid    = false;
}

// upper bound matches the decoration:motion_blur:strength config range
constexpr double MAX_STRENGTH = 10;

// stock staleness cutoff for the recorded motion when no decay is configured
constexpr float STALE_MS = 100;

bool            CTracker::expired(Time::steady_tp now, std::chrono::milliseconds decay) const {
    if (!m_valid)
        return true;

    const float AGE = std::chrono::duration<float, std::milli>(now - m_updatedAt).count();
    return decay.count() > 0 ? AGE >= sc<float>(decay.count()) : AGE > STALE_MS;
}

std::optional<SState> CTracker::state(int samples, const Vector2D& offset, bool allowStale, double strength, Time::steady_tp now, std::chrono::milliseconds decay) const {
    if (!m_valid || (!allowStale && expired(now, decay)))
        return std::nullopt;

    CBox previous = m_previous.copy().translate(offset);
    CBox current  = m_current.copy().translate(offset);

    // with a decay configured, the trail shrinks linearly toward the static current box over the configured duration; the stale
    // (damage clearing) path skips the shrink so the full recorded footprint stays available even at expiry
    const float DECAY_MS = sc<float>(decay.count());
    if (!allowStale && DECAY_MS > 0.F) {
        const float AGE      = std::chrono::duration<float, std::milli>(now - m_updatedAt).count();
        const float PROGRESS = std::clamp(AGE / DECAY_MS, 0.F, 1.F);

        previous.x += (current.x - previous.x) * PROGRESS;
        previous.y += (current.y - previous.y) * PROGRESS;
        previous.w += (current.w - previous.w) * PROGRESS;
        previous.h += (current.h - previous.h) * PROGRESS;
    }

    if (previous == current || previous.w <= 0.F || previous.h <= 0.F || current.w <= 0.F || current.h <= 0.F)
        return std::nullopt;

    return SState{
        .previous = scaled(current, previous, std::clamp(strength, 0.0, MAX_STRENGTH)),
        .current  = current,
        .samples  = samples,
    };
}

bool MotionBlur::recordFrame(CTracker& tracker, const CBox& current, Time::steady_tp now, std::chrono::milliseconds decay) {
    const auto LAST = tracker.recorded();
    if (LAST && LAST->current == current) {
        // motion settled: with a decay configured the pair keeps shrinking toward the unchanged current box; without one the
        // stock staleness collapses it
        if (decay.count() > 0 && !tracker.expired(now, decay))
            return false;

        tracker.record(current, current, now);
        return false;
    }

    // first sample or expired trail (occlusion gap, toggle flips): anchor without displacement instead of smearing the gap
    if (!LAST || tracker.expired(now, decay)) {
        tracker.record(current, current, now);
        return false;
    }

    tracker.record(LAST->current, current, now);
    return true;
}
