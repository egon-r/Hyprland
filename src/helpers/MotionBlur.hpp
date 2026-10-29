#pragma once

#include "time/Time.hpp"
#include "math/Math.hpp"
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
        void                  record(const CBox& previous, const CBox& current);
        void                  reset();

        std::optional<SState> state(int samples, const Vector2D& offset = {}, bool allowStale = false, double strength = 1) const;

      private:
        CBox            m_previous;
        CBox            m_current;
        Time::steady_tp m_updatedAt;
        bool            m_valid = false;
    };
};
