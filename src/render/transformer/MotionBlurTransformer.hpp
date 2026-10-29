#pragma once

#include "Transformer.hpp"

namespace Render {
    class CMotionBlurTransformer : public IWindowTransformer {
      public:
        CMotionBlurTransformer(PHLWINDOWREF window);
        virtual ~CMotionBlurTransformer();

        static bool shouldEnable(PHLWINDOW window);
        // decoration:motion_blur:decay clamped to its configured 0..1000 ms range
        static std::chrono::milliseconds decayTimeout();

        virtual SP<Render::IFramebuffer> transform(SP<Render::IFramebuffer> in);
        virtual void                     amendTransformedRenderData(const CBox& currentBox, SMotionBlurData* pMotionBlurData);

        void                             record(const CBox& previous, const CBox& current);
        void                             reanchor(const CBox& current);
        // keeps the transformer alive past the end of motion so the decay keeps ticking down at frame cadence
        void                              settle();
        void                              reset();

        std::optional<MotionBlur::SState> state(bool allowStale = false) const;
        // raw recorded boxes (no strength scaling), for consumers that continue the trail in their own tracker
        std::optional<MotionBlur::SRecordedMotion> recorded() const;

        // Configured state for a raw tracker owned outside the transformed-window pass (popups, layer surfaces, close
        // fadeouts): applies the global samples/strength/decay config. Stale (damage-clearing) reads bypass the master gate.
        static std::optional<MotionBlur::SState> trackerState(const MotionBlur::CTracker& tracker, bool allowStale);

        bool                                     expired() const;

      private:
        void                 armExpiryTimer();
        void                 disarmExpiryTimer();

        PHLWINDOWREF         m_window;
        MotionBlur::CTracker m_motionBlur;
        SP<CEventLoopTimer>  m_expiryTimer;
    };
}
