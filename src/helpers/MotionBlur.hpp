#pragma once

#include "time/Time.hpp"
#include "math/Math.hpp"
#include <chrono>
#include <cstdint>
#include <optional>

namespace MotionBlur {
    CBox extents(const CBox& previous, const CBox& current);

    // Unified screen-space record box: the layout hull with the workspace render offset and the floating offset baked in at
    // record time. Pinned windows (or windows without a workspace) do not follow workspace motion, so their render offset
    // contribution is dropped.
    CBox composedRecordBox(const CBox& layoutBox, const Vector2D& workspaceRenderOffset, const Vector2D& floatingOffset, bool pinned);

    // Decision for a frame-sampled render-side record against the last recorded state.
    // SKIP: the current box is unchanged since the last record (idle, or an event-driven record already wrote this exact
    // box within the same frame, so the sampler must not overwrite the authoritative pair).
    // ANCHOR: there is no usable previous sample (first frame, the recorded motion expired after an occlusion gap or a
    // teleport, or the previous sample lies fully outside `bounds` after an off-screen transit): restart without
    // displacement instead of smearing the whole jump.
    // RECORD: fresh on-screen previous sample with a real displacement.
    enum class ESampleDecision : uint8_t {
        SKIP   = 0,
        ANCHOR = 1,
        RECORD = 2,
    };

    ESampleDecision sampleDecision(const std::optional<CBox>& lastCurrent, bool lastExpired, const CBox& current, const CBox& bounds);

    // Master gate shared by every motion blur consumer: the effect needs the master toggle, multiple samples and a nonzero
    // strength. Fullscreen windows are excluded unless the fullscreen opt-in is set. Per-kind toggles gate at the record
    // and render sites on top of this.
    bool eligible(bool enabled, int samples, double strength, bool fullscreenActive, bool allowFullscreen);

    // Scales the motion recorded between `previous` and `current` by `strength`: 1 keeps `previous` unchanged, 2 doubles the smear
    // length, 0 collapses it onto `current`. Non-finite strengths are treated as 1, and scaled sizes are clamped to a small positive
    // value so aggressive resizes never produce invalid boxes.
    CBox scaled(const CBox& current, const CBox& previous, double strength);

    struct SRecordedMotion {
        CBox previous;
        CBox current;
    };

    struct SState {
        CBox previous;
        CBox current;
        int  samples = 1;

        CBox extents() const;
    };

    class CTracker {
      public:
        void record(const CBox& previous, const CBox& current, Time::steady_tp now = Time::steadyNow());

        // Raw recorded boxes without strength scaling, for consumers that continue a trail in their own tracker (e.g. close
        // fadeouts); nullopt while nothing valid is recorded.
        std::optional<SRecordedMotion> recorded() const;

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

    // Single-box frame sampler for tracker owners without an event-driven record site (popups, layers, close fadeouts):
    // records real displacement against the last record, anchoring without smear on the first sample, after expiry/occlusion
    // gaps, or when the box is unchanged. With a nonzero `decay`, a settled (unchanged-box) sample leaves the pair for the
    // decay to shrink instead of collapsing it. Returns true only when a new displacement was recorded.
    bool recordFrame(CTracker& tracker, const CBox& current, Time::steady_tp now = Time::steadyNow(), std::chrono::milliseconds decay = {});
};
