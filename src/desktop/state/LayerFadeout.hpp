#pragma once

#include "Fadeout.hpp"
#include "../../helpers/MotionBlur.hpp"

namespace Desktop {
    class CLayerFadeout final : public IFadeout {
      public:
        static SP<CLayerFadeout>               create(PHLLS layer, SP<Render::IFramebuffer> snapshot, float sourceAlpha);

        virtual PHLMONITORREF                  monitor() const override;
        virtual eFadeoutPlane                  plane() const override;
        virtual int                            zIndex() const override;
        virtual CBox                           renderBox() const override;
        virtual float                          alpha() const override;
        virtual bool                           done() const override;
        virtual SFadeoutRenderEffects          effects() const override;
        virtual std::optional<SMotionBlurData> motionBlur() const override;

      private:
        CLayerFadeout() = default;

        PHLMONITORREF     m_monitor;
        eFadeoutPlane     m_plane  = FADEOUT_PLANE_LAYER_TOP;
        int               m_zIndex = 0;
        CBox              m_geometry;
        PHLANIMVAR<float> m_alpha;
        bool              m_marksBlurDirty = false;

        // per-frame sampled trail of the layer exit slide/popin over the full snapshot mapping; mutated in a const sampler
        mutable MotionBlur::CTracker m_motionBlur;
    };
}
