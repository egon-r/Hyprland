#pragma once

#include <optional>

#include "../../../render/transformer/Transformer.hpp"

namespace Render {
    class CMotionBlurTransformer;
    class CWobbleTransformer;
    class CWindowTransformerList;
}

namespace Desktop::View {
    class CWindow;
    enum eWindowUpdateSource : uint8_t;

    class CWindowEffectsController {
      public:
        explicit CWindowEffectsController(CWindow& window);
        ~CWindowEffectsController();

        CWindowEffectsController(const CWindowEffectsController&)                    = delete;
        CWindowEffectsController(CWindowEffectsController&&)                         = delete;
        CWindowEffectsController&         operator=(const CWindowEffectsController&) = delete;
        CWindowEffectsController&         operator=(CWindowEffectsController&&)      = delete;

        void                              onPositionUpdate(const CBox& previous, const CBox& current, eWindowUpdateSource source);
        std::optional<MotionBlur::SState> motionBlurState(bool allowStale = false) const;
        void                              damageMotionBlur(bool allowStale = false) const;
        void                              settleMotionBlur();
        void                              tickMotionBlurDecay();
        void                              resetMotionBlur();
        // unified screen-space record box: the layout hull with the workspace render offset and floating offset baked in
        CBox motionBlurRecordBox() const;
        // raw recorded boxes (no strength scaling), for consumers that continue the trail in their own tracker
        std::optional<MotionBlur::SRecordedMotion> motionBlurRecorded() const;
        // render-side frame sampler for workspace-driven motion; owns ONLY the workspace render offset step. Event-driven
        // records (animations, drags) keep authority over their own frames; the sampler anchors (never smears) on the first
        // frame of a motion, after occlusion gaps, and drops its transformer once the workspace settles again.
        void                                      sampleMotionBlurFrame(const CBox& composedGlobalBox);
        void                                      resetWobble();
        void                                      reset();
        bool                                      tickWobble();

        bool                                      hasActiveTransformers() const;
        bool                                      blocksDirectScanout() const;
        CBox                                      transformedExtents(const CBox& currentBox) const;
        CBox                                      transformBoxForDamage(const CBox& currentBox) const;
        void                                      preWindowRender(Render::CRenderContext& ctx, CSurfacePassElement::SRenderData* renderData) const;
        void                                      amendTransformedRenderData(Render::CRenderContext& ctx, const CBox& currentBox, SMotionBlurData* motionBlurData,
                                                                             const Render::SWindowRenderPresentation& presentation) const;
        const UP<Render::CWindowTransformerList>& transformers() const;

      private:
        void                                  recordMotionBlur(const CBox& previous, const CBox& current, eWindowUpdateSource source);
        Render::CMotionBlurTransformer*       motionBlurTransformer();
        const Render::CMotionBlurTransformer* motionBlurTransformer() const;
        Render::CWobbleTransformer*           wobbleTransformer();

        CWindow&                              m_window;
        UP<Render::CWindowTransformerList>    m_transformers;
        // workspace offset seen by the last motion blur frame sample, used to decompose pure workspace displacement from local
        // geometry motion
        Vector2D m_motionBlurLastOffset;
    };
}
