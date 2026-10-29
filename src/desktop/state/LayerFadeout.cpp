#include "LayerFadeout.hpp"
#include "../view/LayerSurface.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../config/shared/animation/AnimationTree.hpp"
#include "../../animation/AnimationManager.hpp"
#include "../../output/Monitor.hpp"
#include "../../protocols/LayerShell.hpp"
#include "../../render/Framebuffer.hpp"
#include "../../render/Renderer.hpp"
#include "../../render/transformer/MotionBlurTransformer.hpp"

#include <algorithm>
#include <cmath>

using namespace Desktop;
using namespace Desktop::View;

static int layerZIndex(PHLLS layer) {
    const auto MONITOR = layer ? layer->m_monitor.lock() : nullptr;
    if (!MONITOR || layer->m_layer >= MONITOR->m_layerSurfaceLayers.size())
        return 0;

    const auto& LAYERS = MONITOR->m_layerSurfaceLayers[layer->m_layer];
    const auto  IT     = std::ranges::find_if(LAYERS, [&](const auto& other) { return other.lock() == layer; });
    return IT == LAYERS.end() ? 0 : std::distance(LAYERS.begin(), IT);
}

static bool shouldBlurLayer(PHLLS layer) {
    static auto PBLUR = CConfigValue<Config::INTEGER>("decoration:blur:enabled");
    if (!*PBLUR)
        return false;

    auto surface = layer->wlSurface();
    if (surface && surface->m_hasBackgroundEffect)
        return !surface->m_blurRegion.empty();

    return layer->m_ruleApplicator->blur().valueOrDefault();
}

static void damageFadeoutMonitor(PHLMONITORREF monitor) {
    if (const auto MONITOR = monitor.lock(); MONITOR)
        g_pHyprRenderer->damageMonitor(MONITOR);
}

SP<CLayerFadeout> CLayerFadeout::create(PHLLS layer, SP<Render::IFramebuffer> snapshot, float sourceAlpha) {
    if (!layer || !snapshot)
        return nullptr;

    const auto MONITOR = layer->m_monitor.lock();
    if (!MONITOR)
        return nullptr;

    auto fadeout           = SP<CLayerFadeout>(new CLayerFadeout());
    fadeout->m_monitor     = MONITOR;
    fadeout->m_framebuffer = snapshot;
    fadeout->m_geometry    = layer->m_geometry;
    fadeout->m_zIndex      = layerZIndex(layer);

    static auto PDIMAROUND = CConfigValue<Config::FLOAT>("decoration:dim_around");
    if (*PDIMAROUND && layer->m_ruleApplicator->dimAround().valueOrDefault())
        fadeout->m_effects.dimAroundAlpha = *PDIMAROUND;

    fadeout->m_effects.textureBlur.enabled = shouldBlurLayer(layer);
    if (fadeout->m_effects.textureBlur.enabled)
        fadeout->m_effects.textureBlur.ignoreAlpha = layer->m_ruleApplicator->ignoreAlpha().valueOr(0.01F);

    switch (layer->m_layer) {
        case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND: fadeout->m_plane = FADEOUT_PLANE_LAYER_BACKGROUND; break;
        case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM: fadeout->m_plane = FADEOUT_PLANE_LAYER_BOTTOM; break;
        case ZWLR_LAYER_SHELL_V1_LAYER_TOP: fadeout->m_plane = FADEOUT_PLANE_LAYER_TOP; break;
        case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY: fadeout->m_plane = FADEOUT_PLANE_LAYER_OVERLAY; break;
        default: fadeout->m_plane = FADEOUT_PLANE_LAYER_TOP; break;
    }

    fadeout->m_marksBlurDirty = layer->m_layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND || layer->m_layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;

    const auto ANIMCTX = layer->m_animationController.animateOut();

    Animation::mgr()->createAnimation(ANIMCTX.pos.from, fadeout->m_realPosition, Config::animationTree()->getAnimationPropertyConfig("layersOut"), AVARDAMAGE_NONE);
    Animation::mgr()->createAnimation(ANIMCTX.size.from, fadeout->m_realSize, Config::animationTree()->getAnimationPropertyConfig("layersOut"), AVARDAMAGE_NONE);
    Animation::mgr()->createAnimation(sourceAlpha, fadeout->m_alpha, Config::animationTree()->getAnimationPropertyConfig("fadeLayersOut"), AVARDAMAGE_NONE);

    const WP<CLayerFadeout> WEAK = fadeout;

    // seed the exit-anim trail by carrying the dying layer's rendered displacement onto the fadeout's own animated hull
    static auto PLAYERS = CConfigValue<Config::INTEGER>("decoration:motion_blur:layers");
    if (*PLAYERS) {
        const CBox FROM  = {ANIMCTX.pos.from, ANIMCTX.size.from};
        const auto TRAIL = layer->motionBlurState(true);
        if (TRAIL) {
            const Vector2D DELTA = TRAIL->current.pos() - TRAIL->previous.pos();
            fadeout->m_motionBlur.record(FROM.copy().translate(-DELTA), FROM);
        } else
            MotionBlur::recordFrame(fadeout->m_motionBlur, FROM, Time::steadyNow(), Render::CMotionBlurTransformer::decayTimeout());
    }

    const auto DAMAGE = [WEAK](auto) {
        if (const auto FADEOUT = WEAK.lock(); FADEOUT) {
            damageFadeoutMonitor(FADEOUT->m_monitor);
            if (const auto MONITOR = FADEOUT->m_monitor.lock(); MONITOR && FADEOUT->m_marksBlurDirty)
                MONITOR->m_blurFBDirty = true;
        }
    };
    fadeout->m_realPosition->setUpdateCallback(DAMAGE);
    fadeout->m_realSize->setUpdateCallback(DAMAGE);
    fadeout->m_alpha->setUpdateCallback(DAMAGE);

    fadeout->m_realPosition->setValueAndWarp(ANIMCTX.pos.from);
    fadeout->m_realSize->setValueAndWarp(ANIMCTX.size.from);
    fadeout->m_alpha->setValueAndWarp(sourceAlpha);

    *fadeout->m_realPosition = ANIMCTX.pos.to;
    *fadeout->m_realSize     = ANIMCTX.size.to;
    *fadeout->m_alpha        = ANIMCTX.alpha.to;

    return fadeout;
}

PHLMONITORREF CLayerFadeout::monitor() const {
    return m_monitor;
}

eFadeoutPlane CLayerFadeout::plane() const {
    return m_plane;
}

int CLayerFadeout::zIndex() const {
    return m_zIndex;
}

CBox CLayerFadeout::renderBox() const {
    const auto MONITOR = m_monitor.lock();
    if (!MONITOR || m_geometry.w == 0 || m_geometry.h == 0)
        return {};

    const Vector2D SCALE = {m_realSize->value().x / m_geometry.w, m_realSize->value().y / m_geometry.h};

    return {((m_realPosition->value().x - MONITOR->m_position.x) * MONITOR->m_scale) - (((m_geometry.x - MONITOR->m_position.x) * MONITOR->m_scale) * SCALE.x),
            ((m_realPosition->value().y - MONITOR->m_position.y) * MONITOR->m_scale) - (((m_geometry.y - MONITOR->m_position.y) * MONITOR->m_scale) * SCALE.y),
            MONITOR->m_transformedSize.x * SCALE.x, MONITOR->m_transformedSize.y * SCALE.y};
}

float CLayerFadeout::alpha() const {
    return m_alpha->value();
}

bool CLayerFadeout::done() const {
    return m_alpha->value() == 0.F && !m_alpha->isBeingAnimated() && !m_realPosition->isBeingAnimated() && !m_realSize->isBeingAnimated();
}

SFadeoutRenderEffects CLayerFadeout::effects() const {
    auto effects = m_effects;
    effects.dimAroundAlpha *= alpha();

    if (effects.textureBlur.enabled)
        effects.textureBlur.alpha = std::sqrt(std::max(alpha(), 0.F));

    return effects;
}

std::optional<SMotionBlurData> CLayerFadeout::motionBlur() const {
    static auto PENABLED = CConfigValue<Config::INTEGER>("decoration:motion_blur:enabled");
    static auto PLAYERS  = CConfigValue<Config::INTEGER>("decoration:motion_blur:layers");
    const auto  MONITOR  = m_monitor.lock();
    if (!*PENABLED || !*PLAYERS || !MONITOR) {
        // toggled off mid-fade: drop any lingering trail; the fadeout's update callbacks keep the monitor damaged
        m_motionBlur.reset();
        return std::nullopt;
    }

    // per-frame sampling over the exit slide/popin; alpha-only settles decay the trail out naturally
    MotionBlur::recordFrame(m_motionBlur, CBox{m_realPosition->value(), m_realSize->value()}, Time::steadyNow(), Render::CMotionBlurTransformer::decayTimeout());

    const auto STATE = Render::CMotionBlurTransformer::trackerState(m_motionBlur, false);
    if (!STATE)
        return std::nullopt;

    SMotionBlurData mb;
    mb.enabled  = true;
    mb.previous = STATE->previous.copy().translate(-MONITOR->m_position);
    mb.current  = STATE->current.copy().translate(-MONITOR->m_position);
    mb.samples  = STATE->samples;
    // the snapshot FB holds the ORIGINAL monitor image: sample the original content crop (m_geometry is monitor-local
    // logical; renderFadeouts scales it to FB pixels), while prev/current are the animated destination rects
    mb.source = CBox{m_geometry.pos(), m_geometry.size()};
    return mb;
}
