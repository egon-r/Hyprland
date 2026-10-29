<div align = center>

<img src="https://raw.githubusercontent.com/hyprwm/Hyprland/main/assets/header.svg" width="750" height="300" alt="banner">

<br>

[![Badge Workflow]][Workflow]
[![Badge License]][License] 
![Badge Language] 
[![Badge Pull Requests]][Pull Requests] 
[![Badge Issues]][Issues] 
![Badge Hi Mom]<br>

<br>

Hyprland is a 100% independent, dynamic tiling Wayland compositor that doesn't sacrifice on its looks.

It provides the latest Wayland features, is highly customizable, has all the eyecandy, the most powerful plugins,
easy IPC, much more QoL stuff than other compositors and more...
<br>
<br>

---

**[<kbd> <br> Install <br> </kbd>][Install]** 
**[<kbd> <br> Quick Start <br> </kbd>][Quick Start]** 
**[<kbd> <br> Configure <br> </kbd>][Configure]** 
**[<kbd> <br> Contribute <br> </kbd>][Contribute]**

---

<br>

</div>

> [!IMPORTANT]
> **Personal, unofficial build — not upstream Hyprland.** This repository is an independent
> personal derivative of [hyprwm/Hyprland](https://github.com/hyprwm/Hyprland), based on
> upstream **v0.56.2** and retaining upstream's README, license, and history. Its local
> features are `decoration:motion_blur:strength`, a smear-length multiplier for window motion
> blur, independent of the `:samples` count: range **0–10**, default **1** (smear matches the
> recorded motion), **2** doubles the smear length, **0** disables the effect; and
> `decoration:motion_blur:decay`, milliseconds (**0–1000**, default **0**) the recorded smear
> keeps shrinking toward the window's final box after the movement stops — a finite, linearly
> decaying smear re-rendered from the recorded motion, not a replay of cached frames (`strength`
> is purely spatial; `decay` only extends how long the trail lingers); and per-surface toggles
> selecting which motion records a trail (all default off unless stated, all subordinate to the
> global `:enabled`/`:samples`/`:strength` gates; `:decay` controls settling):
>
> | Option | Default | Blurs |
> |---|---|---|
> | `window_animations` | `true` | mapped window animations (open/move/resize/layout changes) — pre-existing behavior, kept as a toggle |
> | `workspaces` | `false` | workspace motion (keyboard switches, gestures/swipes, special-workspace toggles) for windows on the moving workspace; pinned windows are excluded |
> | `window_close` | `false` | the close animation: the trail is continued over the full snapshot mapping (the fadeout samples its own animated box, so the smear tracks the snapshot's slide/scale) |
> | `popups` | `false` | real spatial popup movement (parent-window motion, workspace motion, the popup's own repositioning); opacity-only fades/spawns never smear — a fade-only spawn has no trail by design |
> | `layers` | `false` | layer-shell slide/popin entry/exit animations; stationary alpha fades do not smear, and silent layout rearranges (teleports without an animation) do not either |
> | `fullscreen` | `false` | permits fullscreen windows to participate in the enabled window/workspace blur paths; does not create motion itself |
>
> Implementation: records are unified **screen-space box pairs** (`MotionBlur::CTracker`, `previous`/`current` with the
> workspace render offset and floating offset baked in at record time). Window records happen event-driven (animation
> tick, drags); pure workspace displacement is frame-sampled once per rendered frame on the render path (skip/anchor
> decisions prevent full-screen first-frame smears after occlusion, teleports or off-screen transit, and never clobber an
> event-driven record of the same frame). Rendering reuses the existing motion-blur shader/`CTex` infrastructure with the
> exact per-surface UV mapping (buffer scales and viewport crops preserved); popups/layer surfaces render
> through their normal passes, no extra framebuffers are allocated for ordinary unchanged surfaces, and trails render
> inside the recorded hull only. Toggling a sub-feature off stops recording and the stale trail is cleared by the bounded
> staleness/decay logic (≤100 ms without `:decay`, else the configured `:decay` window) — no unbounded timers.
>
> Known limits: popup/layer surfaces with non-normal buffer transforms use the normal unblurred path.
> Menus/tooltips that only fade in place have no spatial motion to smear; this does not add new popup animations.
> Trails on rotated or flipped outputs are not supported: the raw-surface trail path keeps unrotated
> texture UVs, so popups/layers can show a wrong orientation while a trail is active on such monitors.
> Outputs with a normal transform are unaffected.
> Validation bounds: `:samples` 1–64, `:strength` 0–10, `:decay` 0–1000; helper math is covered by
> `tests/helpers/MotionBlur.cpp`. The bundled
> [`example/hyprland.lua`](example/hyprland.lua) demonstrates enabling it with
> `enabled = true, samples = 8, strength = 2, decay = 200`.
>
> Verification status: all **299 unit tests** pass, and the built `Hyprland --version` command runs
> successfully. The build reported stale gcov profile checksum warnings after recompilation; the
> unit-test run completed without those warnings. A next-login isolated trial is authorized: the
> committed build is installed under `~/.local/opt/hyprland-animation-blur` (with the companion
> cursor plugin rebuilt against that trial's installed headers), and the new config
> `~/.config/hypr/animation-blur.lua` activates it at the next login; the trial artifact's version
> reports this source commit instead of the older embedded `db28a35e` identity. No new rendering or
> live verification was performed — the earlier UI checks do not verify the new rendering paths
> visually, and the workstation remains in use.
> A follow-up correctness fix keeps event-recorded layer animation pairs intact when the render-side
> sample catches up at zero decay (previously the pair collapsed to current/current and the mapped
> layer animation trail never rendered); the retained trail still clears on the bounded expiry. The
> full unit-test suite (299 tests) still passes.
>
> This repository is a **source/build artifact only**. The working installation under
> `~/.local/opt/hyprland-motion-blur` is preserved untouched and keeps running the `db28a35e`
> build (strength/decay) in the live session. Nothing here is official, supported by, affiliated
> with, or endorsed by upstream hyprwm — do not report this fork's behavior there.

# Features

- All of the eyecandy: gradient borders, blur, animations, shadows and much more
- A lot of customization
- 100% independent, no wlroots, no libweston, no kwin, no mutter.
- Custom bezier curves for the best animations
- Powerful plugin support
- Built-in plugin manager
- Tearing support for better gaming performance
- Easily expandable and readable codebase
- Fast and active development
- Not afraid to provide bleeding-edge features
- Config reloaded instantly upon saving
- Fully dynamic workspaces
- Two built-in layouts and more available as plugins
- Global keybinds passed to your apps of choice
- Tiling/pseudotiling/floating/fullscreen windows
- Special workspaces (scratchpads)
- Window groups (tabbed mode)
- Powerful window/monitor/layer rules
- Socket-based IPC
- Native IME and Input Panels Support
- and much more...

<br>
<br>

<div align = center>

# Gallery

<br>

![Preview A]

<br>

![Preview B]

<br>

![Preview C]

<br>
<br>

</div>

# Special Thanks

<br>

**[wlroots]** - *For powering Hyprland in the past*

**[tinywl]** - *For showing how 2 do stuff*

**[Sway]** - *For showing how 2 do stuff the overkill way*

**[Vivarium]** - *For showing how 2 do stuff the simple way*

**[dwl]** - *For showing how 2 do stuff the hacky way*

**[Wayfire]** - *For showing how 2 do some graphics stuff*


<!----------------------------------------------------------------------------->

[Configure]: https://wiki.hypr.land/Configuring/
[Stars]: https://starchart.cc/hyprwm/Hyprland
[Hypr]: https://github.com/hyprwm/Hypr

[Pull Requests]: https://github.com/hyprwm/Hyprland/pulls
[Issues]: https://github.com/hyprwm/Hyprland/issues
[Todo]: https://github.com/hyprwm/Hyprland/projects?type=beta

[Contribute]: https://wiki.hypr.land/Contributing-and-Debugging/
[Install]: https://wiki.hypr.land/Getting-Started/Installation/
[Quick Start]: https://wiki.hypr.land/Getting-Started/Master-Tutorial/
[Workflow]: https://github.com/hyprwm/Hyprland/actions/workflows/ci.yaml
[License]: LICENSE


<!----------------------------------{ Thanks }--------------------------------->

[Vivarium]: https://github.com/inclement/vivarium
[WlRoots]: https://gitlab.freedesktop.org/wlroots/wlroots
[Wayfire]: https://github.com/WayfireWM/wayfire
[TinyWl]: https://gitlab.freedesktop.org/wlroots/wlroots/-/blob/master/tinywl/tinywl.c
[Sway]: https://github.com/swaywm/sway
[DWL]: https://codeberg.org/dwl/dwl

<!----------------------------------{ Images }--------------------------------->

[Preview A]: ./assets/prev1.png
[Preview B]: ./assets/prev2.png
[Preview C]: ./assets/prev3.png


<!----------------------------------{ Badges }--------------------------------->

[Badge Workflow]: https://github.com/hyprwm/Hyprland/actions/workflows/ci.yaml/badge.svg

[Badge Issues]: https://img.shields.io/github/issues/hyprwm/Hyprland
[Badge Pull Requests]: https://img.shields.io/github/issues-pr/hyprwm/Hyprland
[Badge Language]: https://img.shields.io/github/languages/top/hyprwm/Hyprland
[Badge License]: https://img.shields.io/github/license/hyprwm/Hyprland
[Badge Lines]: https://img.shields.io/tokei/lines/github/hyprwm/Hyprland
[Badge Hi Mom]: https://img.shields.io/badge/Hi-mom!-ff69b4
