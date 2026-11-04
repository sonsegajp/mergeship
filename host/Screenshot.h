#pragma once

#include <string>
#include <vector>

namespace MergeShipHost {

// The game window of this process (the largest visible top-level window it owns), or nullptr.
void* FindGameWindow();

// Captures the client area of `window` (an HWND) into a 24-bit BMP. Uses PrintWindow with
// PW_RENDERFULLCONTENT, which also captures DXGI flip-model swap chains. Returns false on failure.
bool CaptureWindowToBmp(void* window, const std::wstring& path);

// The client area of `window` as bottom-up 24-bit BGR rows, each padded to 4 bytes (a BMP's pixel data).
bool CaptureWindowPixels(void* window, std::vector<unsigned char>& pixels, int& width, int& height);

} // namespace MergeShipHost
