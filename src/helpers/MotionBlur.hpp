#pragma once

#include "time/Time.hpp"
#include "math/Math.hpp"
#include <chrono>
#include <optional>

namespace MotionBlur {
    CBox extents(const CBox& previous, const CBox& current);

    // Scales the motion recorded between `previous` and `current` by `strength`: 1 keeps `previous` unchanged, 2 doubles the smear
    // length, 0 collapses it onto `current`. Non-finite strengths are treated as 1, and scaled sizes are clamped to a small positive
    // value so aggressive resizes never produce invalid boxes.
    CBox scaled(const CBox& current, const CBox& previous, double strength);

    struct SState {
        CBox previous;
        CBox current;
        int  samples = 1;

        CBox extents() const;
    };

    class CTracker {
      public:
        void record(const CBox& previous, const CBox& current, Time::steady_tp now = Time::steadyNow());

        // Glues the recorded displacement (position and size) onto the actual final box after a minor (<2px edge) move, keeping
        // the recorded age: the decay end stays anchored to the last significant movement instead of being pushed forward forever.
        void reanchor(const CBox& current);
        void reset();

        // `decay` shrinks the rendered trail linearly toward `current` over the configured duration since the last recorded
        // movement. It never applies to the `allowStale` (damage clearing) path, which must always see the full recorded footprint.
        std::optional<SState> state(int samples, const Vector2D& offset = {}, bool allowStale = false, double strength = 1, Time::steady_tp now = Time::steadyNow(),
                                    std::chrono::milliseconds decay = {}) const;

        // Whether the recorded motion is expired: fully decayed (`decay > 0`) or older than the stock 100 ms cutoff.
        bool expired(Time::steady_tp now = Time::steadyNow(), std::chrono::milliseconds decay = {}) const;

      private:
        CBox            m_previous;
        CBox            m_current;
        Time::steady_tp m_updatedAt;
        bool            m_valid = false;
    };
};
