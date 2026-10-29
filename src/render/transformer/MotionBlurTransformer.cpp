#include "MotionBlurTransformer.hpp"

#include "../../config/ConfigValue.hpp"
#include "../../desktop/view/Window.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../managers/eventLoop/EventLoopTimer.hpp"
#include "../../managers/fullscreen/FullscreenController.hpp"
#include "../Renderer.hpp"

#include <algorithm>
#include <chrono>

using namespace Render;

CMotionBlurTransformer::CMotionBlurTransformer(PHLWINDOWREF window) : m_window(window) {
    ;
}

CMotionBlurTransformer::~CMotionBlurTransformer() {
    disarmExpiryTimer();
}

bool CMotionBlurTransformer::shouldEnable(PHLWINDOW window) {
    static auto PMBENABLED  = CConfigValue<Config::INTEGER>("decoration:motion_blur:enabled");
    static auto PMBSAMPLES  = CConfigValue<Config::INTEGER>("decoration:motion_blur:samples");
    static auto PMBSTRENGTH = CConfigValue<Config::FLOAT>("decoration:motion_blur:strength");

    if (!window)
        return false;

    return *PMBENABLED && *PMBSAMPLES > 1 && *PMBSTRENGTH > 0 && !Fullscreen::controller()->isFullscreen(window);
}

std::chrono::milliseconds CMotionBlurTransformer::decayTimeout() {
    static auto PMBDECAY = CConfigValue<Config::FLOAT>("decoration:motion_blur:decay");

    // bounds match the decoration:motion_blur:decay config range
    return std::chrono::milliseconds{sc<int64_t>(std::clamp<double>(*PMBDECAY, 0.0, 1000.0))};
}

SP<Render::IFramebuffer> CMotionBlurTransformer::transform(SP<Render::IFramebuffer> in) {
    return in;
}

void CMotionBlurTransformer::amendTransformedRenderData(const CBox& currentBox, SMotionBlurData* pMotionBlurData) {
    if (!pMotionBlurData)
        return;

    const auto PMONITOR = g_pHyprRenderer->m_renderData.pMonitor;
    if (!PMONITOR)
        return;

    const auto STATE = state();
    if (!STATE)
        return;

    SMotionBlurData motionBlur;
    motionBlur.enabled  = true;
    motionBlur.previous = STATE->previous.copy().translate(-PMONITOR->m_position);
    motionBlur.current  = STATE->current.copy().translate(-PMONITOR->m_position);
    motionBlur.samples  = STATE->samples;

    const Vector2D relPos = currentBox.pos() - motionBlur.current.pos();
    const Vector2D scale  = motionBlur.previous.size() / motionBlur.current.size();

    motionBlur.previous = {motionBlur.previous.pos() + relPos * scale, currentBox.size() * scale};
    motionBlur.current  = currentBox;

    *pMotionBlurData = motionBlur;
}

void CMotionBlurTransformer::record(const CBox& previous, const CBox& current) {
    m_motionBlur.record(previous, current);
    armExpiryTimer();
}

void CMotionBlurTransformer::reanchor(const CBox& current) {
    m_motionBlur.reanchor(current);
}

void CMotionBlurTransformer::settle() {
    armExpiryTimer();
}

void CMotionBlurTransformer::reset() {
    m_motionBlur.reset();
    disarmExpiryTimer();
}

bool CMotionBlurTransformer::expired() const {
    return m_motionBlur.expired(Time::steadyNow(), decayTimeout());
}

std::optional<MotionBlur::SState> CMotionBlurTransformer::state(bool allowStale) const {
    const auto PWINDOW = m_window.lock();
    if (!PWINDOW)
        return std::nullopt;

    // the stale path is damage clearing only: it must report the full recorded footprint even when enablement has flipped
    // (disabled, fullscreen, samples/strength changed), so the final clear still erases the whole trail before the reset
    if (!allowStale && !shouldEnable(PWINDOW))
        return std::nullopt;

    static auto    PMBSAMPLES  = CConfigValue<Config::INTEGER>("decoration:motion_blur:samples");
    static auto    PMBSTRENGTH = CConfigValue<Config::FLOAT>("decoration:motion_blur:strength");

    const Vector2D RENDEROFFSET = (PWINDOW->m_pinned || !PWINDOW->m_workspace ? Vector2D{} : PWINDOW->m_workspace->m_renderOffset->value()) + PWINDOW->m_floatingOffset;
    return m_motionBlur.state(std::clamp(sc<int>(*PMBSAMPLES), 2, 64), RENDEROFFSET, allowStale, *PMBSTRENGTH, Time::steadyNow(), decayTimeout());
}

void CMotionBlurTransformer::armExpiryTimer() {
    if (!m_expiryTimer) {
        m_expiryTimer = makeShared<CEventLoopTimer>(
            std::nullopt,
            [window = m_window](SP<CEventLoopTimer>, void*) {
                // the window may have reset (and destroyed) this transformer in the meantime: never touch `this`, re-resolve it
                // through the weak window reference instead
                if (const auto PWINDOW = window.lock())
                    PWINDOW->tickMotionBlurDecay();
            },
            nullptr);
        g_pEventLoopManager->addTimer(m_expiryTimer);
    }

    const auto DECAY = decayTimeout();
    if (DECAY.count() > 0) {
        // frame-cadence decay tick: redraw the shrinking smear until it is fully decayed; the tick resets the window once expired
        const auto PMONITOR = m_window.lock() ? m_window.lock()->m_monitor.lock() : nullptr;
        const int  TICK_MS  = PMONITOR ? std::clamp(sc<int>(1000.F / PMONITOR->m_refreshRate), 4, 33) : 16;

        m_expiryTimer->updateTimeout(std::chrono::milliseconds(TICK_MS));
        return;
    }

    m_expiryTimer->updateTimeout(std::chrono::milliseconds(110));
}

void CMotionBlurTransformer::disarmExpiryTimer() {
    if (m_expiryTimer)
        m_expiryTimer->updateTimeout(std::nullopt);
}
