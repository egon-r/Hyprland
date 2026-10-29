#include "WindowEffectsController.hpp"

#include <algorithm>
#include <cmath>

#include "Window.hpp"
#include "WindowPresentation.hpp"
#include "../../../managers/input/InputManager.hpp"
#include "../../../render/Renderer.hpp"
#include "../../../render/transformer/MotionBlurTransformer.hpp"
#include "../../../render/transformer/TransformerList.hpp"
#include "../../../render/transformer/WobbleTransformer.hpp"
#include "../../../output/Monitor.hpp"

using namespace Desktop::View;

CWindowEffectsController::CWindowEffectsController(CWindow& window) : m_window(window), m_transformers(makeUnique<Render::CWindowTransformerList>()) {
    ;
}

CWindowEffectsController::~CWindowEffectsController() = default;

Render::CMotionBlurTransformer* CWindowEffectsController::motionBlurTransformer() {
    return m_transformers->get<Render::CMotionBlurTransformer>();
}

const Render::CMotionBlurTransformer* CWindowEffectsController::motionBlurTransformer() const {
    return m_transformers->get<Render::CMotionBlurTransformer>();
}

Render::CWobbleTransformer* CWindowEffectsController::wobbleTransformer() {
    return m_transformers->get<Render::CWobbleTransformer>();
}

std::optional<MotionBlur::SState> CWindowEffectsController::motionBlurState(bool allowStale) const {
    const auto MOTIONBLUR = motionBlurTransformer();
    if (!MOTIONBLUR)
        return std::nullopt;

    return MOTIONBLUR->state(allowStale);
}

void CWindowEffectsController::damageMotionBlur(bool allowStale) const {
    const auto STATE = motionBlurState(allowStale);
    if (!STATE)
        return;

    CBox damage = STATE->extents();
    damage.expand(4.F);
    g_pHyprRenderer->damageMotionBlurBox(damage);
}

CBox CWindowEffectsController::motionBlurRecordBox() const {
    return MotionBlur::composedRecordBox(m_window.getFullWindowBoundingBox(), m_window.m_workspace ? m_window.m_workspace->m_renderOffset->value() : Vector2D{},
                                         m_window.presentation().floatingOffset(), static_cast<bool>(m_window.m_state & Desktop::View::WINDOW_STATE_PINNED));
}

std::optional<MotionBlur::SRecordedMotion> CWindowEffectsController::motionBlurRecorded() const {
    const auto MOTIONBLUR = motionBlurTransformer();
    if (!MOTIONBLUR)
        return std::nullopt;

    return MOTIONBLUR->recorded();
}

void CWindowEffectsController::sampleMotionBlurFrame(const CBox& composedGlobalBox) {
    static auto PMBWORKSPACES = CConfigValue<Config::INTEGER>("decoration:motion_blur:workspaces");
    if (!*PMBWORKSPACES)
        return;

    // pinned windows never follow workspace motion
    if (static_cast<bool>(m_window.m_state & Desktop::View::WINDOW_STATE_PINNED))
        return;

    if (!Render::CMotionBlurTransformer::shouldEnable(m_window.m_self.lock()))
        return;

    // the sampler owns ONLY the workspace render offset step: geometry and floating-offset motion belong to the event-driven
    // records (animation tick, drags) and never leak into the workspace trail
    const Vector2D OFFSETNOW   = m_window.m_workspace ? m_window.m_workspace->m_renderOffset->value() : Vector2D{};
    const Vector2D OFFSETDELTA = OFFSETNOW - m_motionBlurLastOffset;
    m_motionBlurLastOffset     = OFFSETNOW;

    if (OFFSETDELTA == Vector2D{}) {
        // workspace idle: drop inert sample-only transformers (settled anchors or fully expired trails) so ordinary windows
        // stop paying the isolated transformed-pass cost; live or still-decaying trails are kept
        if (const auto MOTIONBLUR = motionBlurTransformer(); MOTIONBLUR && MOTIONBLUR->expired()) {
            const auto RECORDED = MOTIONBLUR->recorded();
            if (!RECORDED || RECORDED->previous == RECORDED->current)
                resetMotionBlur();
        }
        return;
    }

    // sampling state comes from the raw recorded pair, NOT the rendered trail: state() collapses settled (equal-box)
    // records to nullopt, which would keep the sampler re-anchoring forever instead of ever starting the trail
    const auto MOTIONBLUR = motionBlurTransformer();
    const auto RECORDED   = MOTIONBLUR ? MOTIONBLUR->recorded() : std::nullopt;
    const auto PMONITOR   = m_window.m_monitor.lock();
    if (!PMONITOR)
        return;

    const CBox BOUNDS = {PMONITOR->m_position, PMONITOR->m_size};
    const auto DECISION =
        MotionBlur::sampleDecision(RECORDED ? std::optional<CBox>{RECORDED->current} : std::nullopt, !MOTIONBLUR || MOTIONBLUR->expired(), composedGlobalBox, BOUNDS);

    if (DECISION == MotionBlur::ESampleDecision::SKIP)
        return;

    // below the invisible-step threshold an anchor is cheaper than churning the transformer
    constexpr double MIN_EDGE_DELTA_PX = 2.0;
    const bool       RECORD = DECISION == MotionBlur::ESampleDecision::RECORD && std::max(std::abs(OFFSETDELTA.x), std::abs(OFFSETDELTA.y)) * PMONITOR->m_scale > MIN_EDGE_DELTA_PX;

    // the recorded boxes are about to move (or collapse onto the anchor): clear the old footprint first
    damageMotionBlur(true);

    // translate mutates its receiver, hence the copy; the pair keeps the total screen displacement of the workspace step
    const CBox PREVIOUS = RECORD ? composedGlobalBox.copy().translate(-OFFSETDELTA) : composedGlobalBox;

    auto       TARGET = MOTIONBLUR;
    if (!TARGET)
        TARGET = m_transformers->emplace<Render::CMotionBlurTransformer>(m_window.m_self);

    if (!TARGET)
        return;

    TARGET->record(PREVIOUS, composedGlobalBox);

    if (RECORD)
        damageMotionBlur();
}

void CWindowEffectsController::recordMotionBlur(const CBox& previous, const CBox& current, eWindowUpdateSource source) {
    // window animation trails are opt-in: animation-tick records are skipped entirely when the toggle is off
    if (source == WINDOW_UPDATE_ANIMATION) {
        static auto PMBWINDOWANIMS = CConfigValue<Config::INTEGER>("decoration:motion_blur:window_animations");
        if (!*PMBWINDOWANIMS)
            return;
    }

    if (!Render::CMotionBlurTransformer::shouldEnable(m_window.m_self.lock())) {
        resetMotionBlur();
        return;
    }

    const bool DECAYING = Render::CMotionBlurTransformer::decayTimeout().count() > 0;

    if (previous == current) {
        // the motion has settled: an opted-in decay keeps the trail shrinking instead of tearing it down
        if (DECAYING)
            settleMotionBlur();
        else
            resetMotionBlur();
        return;
    }

    constexpr double MIN_EDGE_DELTA_PX = 2.0;

    const auto       PMONITOR = m_window.m_monitor.lock();
    const double     SCALE    = PMONITOR ? PMONITOR->m_scale : 1.0;
    const double     DELTA    = std::max({std::abs(previous.x - current.x), std::abs(previous.y - current.y), std::abs(previous.x + previous.w - current.x - current.w),
                                          std::abs(previous.y + previous.h - current.y - current.h)}) *
        SCALE;

    if (DELTA <= MIN_EDGE_DELTA_PX) {
        if (!DECAYING) {
            resetMotionBlur();
            return;
        }

        // a minor tail move must not reset the trail: damage the old footprint first (the recorded boxes are about to shift), then
        // glue the recorded displacement onto the actual final box and keep the decay running from the last significant movement
        damageMotionBlur(true);

        if (const auto MOTIONBLUR = motionBlurTransformer()) {
            MOTIONBLUR->reanchor(current);
            MOTIONBLUR->settle();
        }
        return;
    }

    damageMotionBlur(true);

    auto MOTIONBLUR = motionBlurTransformer();
    if (!MOTIONBLUR)
        MOTIONBLUR = m_transformers->emplace<Render::CMotionBlurTransformer>(m_window.m_self);

    if (!MOTIONBLUR)
        return;

    MOTIONBLUR->record(previous, current);
    damageMotionBlur();
}

// graceful end of motion (animation finished, drag ended): keeps the recorded trail for an opted-in decay instead of a hard clear
void CWindowEffectsController::settleMotionBlur() {
    if (Render::CMotionBlurTransformer::decayTimeout().count() <= 0) {
        resetMotionBlur();
        return;
    }

    const auto MOTIONBLUR = motionBlurTransformer();
    if (!MOTIONBLUR)
        return;

    // snap the decay target onto the actual final record box (workspace offset baked in) so the lingering trail keeps tracking
    // the settled window
    MOTIONBLUR->reanchor(motionBlurRecordBox());
    MOTIONBLUR->settle();
}

// decay tick from the transformer timer: repaints the shrinking smear at frame cadence and hard-resets once expired
void CWindowEffectsController::tickMotionBlurDecay() {
    const auto MOTIONBLUR = motionBlurTransformer();
    if (!MOTIONBLUR)
        return;

    if (MOTIONBLUR->expired()) {
        // erases the full old damage and removes the transformer; MOTIONBLUR dangles past this point, do not touch it
        resetMotionBlur();
        return;
    }

    damageMotionBlur(true);
    MOTIONBLUR->settle();
}

void CWindowEffectsController::resetMotionBlur() {
    damageMotionBlur(true);

    if (auto MOTIONBLUR = motionBlurTransformer())
        MOTIONBLUR->reset();

    m_transformers->removeInactive();
}

void CWindowEffectsController::resetWobble() {
    if (auto WOBBLE = wobbleTransformer())
        WOBBLE->resetWithDamage();

    m_transformers->removeInactive();
}

void CWindowEffectsController::reset() {
    resetMotionBlur();
    resetWobble();
}

void CWindowEffectsController::onPositionUpdate(const CBox& previous, const CBox& current, eWindowUpdateSource source) {
    recordMotionBlur(previous, current, source);

    if (previous == current)
        return;

    if (!Render::CWobbleTransformer::shouldEnable(m_window.m_self.lock())) {
        resetWobble();
        return;
    }

    auto WOBBLE = wobbleTransformer();
    if (!WOBBLE)
        WOBBLE = m_transformers->emplace<Render::CWobbleTransformer>(m_window.m_self);

    std::optional<Vector2D> grabPoint;
    if (source == WINDOW_UPDATE_MOUSE && current.w > 0.F && current.h > 0.F) {
        const auto MOUSE = g_pInputManager->getMouseCoordsInternal();
        grabPoint        = Vector2D{std::clamp((MOUSE.x - current.x) / current.w, 0.0, 1.0), std::clamp((MOUSE.y - current.y) / current.h, 0.0, 1.0)};
    }

    if (WOBBLE)
        WOBBLE->record(previous, current, grabPoint);
}

bool CWindowEffectsController::tickWobble() {
    const auto WOBBLE = wobbleTransformer();
    if (!WOBBLE)
        return false;

    const bool ACTIVE = WOBBLE->tick();
    m_transformers->removeInactive();
    return ACTIVE;
}

bool CWindowEffectsController::hasActiveTransformers() const {
    return !m_transformers->empty();
}

bool CWindowEffectsController::blocksDirectScanout() const {
    return m_transformers->blocksDirectScanout();
}

CBox CWindowEffectsController::transformedExtents(const CBox& currentBox) const {
    return m_transformers->transformedExtents(currentBox);
}

CBox CWindowEffectsController::transformBoxForDamage(const CBox& currentBox) const {
    return m_transformers->transformBoxForDamage(currentBox);
}

void CWindowEffectsController::preWindowRender(Render::CRenderContext& ctx, CSurfacePassElement::SRenderData* renderData) const {
    m_transformers->preWindowRender(ctx, renderData);
}

void CWindowEffectsController::amendTransformedRenderData(Render::CRenderContext& ctx, const CBox& currentBox, SMotionBlurData* motionBlurData,
                                                          const Render::SWindowRenderPresentation& presentation) const {
    m_transformers->amendTransformedRenderData(ctx, currentBox, motionBlurData, presentation);
}

const UP<Render::CWindowTransformerList>& CWindowEffectsController::transformers() const {
    return m_transformers;
}
