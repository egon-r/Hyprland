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

        virtual SWindowTransformBuffer    transform(CRenderContext& ctx, const SWindowTransformBuffer& in, const SWindowTransformContext& context);
        virtual int                       priority() const;
        virtual bool                      active() const;
        virtual bool                      allocatesOutputBuffer() const;
        virtual CBox                      sourceBoxForOutput(const CBox& outputBox, const CBox& inputBox) const;
        virtual CBox                      transformBoxForDamage(const CBox& currentBox) const;
        virtual void                      amendTransformedRenderData(CRenderContext& ctx, const CBox& currentBox, SMotionBlurData* pMotionBlurData,
                                                                     const Render::SWindowRenderPresentation& presentation);

        void                             record(const CBox& previous, const CBox& current);
        void                             reanchor(const CBox& current);
        // keeps the transformer alive past the end of motion so the decay keeps ticking down at frame cadence
        void                             settle();
        void                             reset();

        std::optional<MotionBlur::SState> state(bool allowStale = false) const;
        bool                              expired() const;

      private:
        std::optional<MotionBlur::SState> stateForOffset(const Vector2D& renderOffset, bool allowStale) const;

        void                              armExpiryTimer();
        void                              disarmExpiryTimer();

        PHLWINDOWREF                      m_window;
        MotionBlur::CTracker              m_motionBlur;
        SP<CEventLoopTimer>               m_expiryTimer;
    };
}
