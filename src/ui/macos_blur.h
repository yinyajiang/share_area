#pragma once

class QWindow;

/**
 * @brief 为 macOS 窗口启用原生毛玻璃 (NSVisualEffectView) 效果
 */
void enableMacOSBlur(QWindow* window, bool enable = true);
