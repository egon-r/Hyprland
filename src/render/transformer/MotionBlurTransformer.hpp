#pragma once

#include "Transformer.hpp"

namespace Render {
    class CMotionBlurTransformer : public IWindowTransformer {
      public:
        CMotionBlurTransformer(PHLWINDOWREF window);
        virtual ~CMotionBlurTransformer();

        static bool                      shouldEnable(PHLWINDOW window);
        // decoration:motion_blur:decay clamped to its configured 0..1000 ms range
        static std::chrono::milliseconds decayTimeout();

        virtual SP<Render::IFramebuffer> transform(SP<Render::IFramebuffer> in);
        virtual void                     amendTransformedRenderData(const CBox& currentBox, SMotionBlurData* pMotionBlurData);

        void                             record(const CBox& previous, const CBox& current);
        void                             reanchor(const CBox& current);
        // keeps the transformer alive past the end of motion so the decay keeps ticking down at frame cadence
        void                             settle();
        void                             reset();

        std::optional<MotionBlur::SState> state(bool allowStale = false) const;
        bool                              expired() const;

      private:
        void                 armExpiryTimer();
        void                 disarmExpiryTimer();

        PHLWINDOWREF         m_window;
        MotionBlur::CTracker m_motionBlur;
        SP<CEventLoopTimer>  m_expiryTimer;
    };
}
