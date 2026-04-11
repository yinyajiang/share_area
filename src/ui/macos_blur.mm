#include "macos_blur.h"

#ifdef Q_OS_MACOS
#import <AppKit/AppKit.h>
#include <QWindow>

static constexpr int kBlurTag = 9973;

void enableMacOSBlur(QWindow* window, bool enable) {
    if (!window) return;

    NSView* nsView = (__bridge NSView*)reinterpret_cast<void*>(window->winId());
    NSWindow* nsWindow = nsView.window;
    if (!nsWindow) return;

    if (enable) {
        // 移除旧的
        NSView* old = [nsWindow.contentView viewWithTag:kBlurTag];
        if (old) [old removeFromSuperview];

        NSVisualEffectView* blur = [[NSVisualEffectView alloc]
            initWithFrame:nsWindow.contentView.bounds];
        blur.material = NSVisualEffectMaterialSidebar;
        blur.blendingMode = NSVisualEffectBlendingModeBehindWindow;
        blur.state = NSVisualEffectStateFollowsWindowActiveState;
        blur.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        blur.tag = kBlurTag;
        [nsWindow.contentView addSubview:blur positioned:NSWindowBelow relativeTo:nil];
    } else {
        NSView* old = [nsWindow.contentView viewWithTag:kBlurTag];
        if (old) [old removeFromSuperview];
    }
}

#else

void enableMacOSBlur(QWindow*, bool) {}

#endif
