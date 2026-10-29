#include "Fadeout.hpp"
#include "../../render/pass/TexPassElement.hpp"

using namespace Desktop;
using namespace Desktop::View;

SP<Render::IFramebuffer> IFadeout::framebuffer() const {
    return m_framebuffer;
}

PHLWORKSPACEREF IFadeout::workspace() const {
    return m_workspace;
}

SFadeoutRenderEffects IFadeout::effects() const {
    return m_effects;
}

std::optional<SMotionBlurData> IFadeout::motionBlur() const {
    return std::nullopt;
}
