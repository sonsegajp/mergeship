#include "Screenshot.h"

#include <windows.h>

#include <cstdio>
#include <vector>

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

namespace MergeShipHost {

namespace {
struct FindState {
    DWORD pid;
    HWND best;
    LONG bestArea;
};

BOOL CALLBACK FindProc(HWND window, LPARAM param) {
    auto* state = reinterpret_cast<FindState*>(param);
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (pid != state->pid || !IsWindowVisible(window) || GetWindow(window, GW_OWNER) != nullptr ||
        window == GetConsoleWindow()) {
        return TRUE;
    }
    RECT rect;
    GetClientRect(window, &rect);
    const LONG area = (rect.right - rect.left) * (rect.bottom - rect.top);
    if (area > state->bestArea) {
        state->best = window;
        state->bestArea = area;
    }
    return TRUE;
}
} // namespace

void* FindGameWindow() {
    FindState state = { GetCurrentProcessId(), nullptr, 0 };
    EnumWindows(FindProc, reinterpret_cast<LPARAM>(&state));
    return state.best;
}

bool CaptureWindowPixels(void* windowHandle, std::vector<unsigned char>& pixels, int& width, int& height) {
    HWND window = static_cast<HWND>(windowHandle);
    if (window == nullptr) {
        return false;
    }
    RECT client;
    GetClientRect(window, &client);
    width = client.right - client.left;
    height = client.bottom - client.top;
    if (width <= 0 || height <= 0) {
        return false;
    }

    // PrintWindow renders the whole window (with frame); capture it, then crop to the client area.
    RECT windowRect;
    GetWindowRect(window, &windowRect);
    POINT origin = { 0, 0 };
    ClientToScreen(window, &origin);
    const int fullWidth = windowRect.right - windowRect.left;
    const int fullHeight = windowRect.bottom - windowRect.top;
    const int offsetX = origin.x - windowRect.left;
    const int offsetY = origin.y - windowRect.top;

    HDC screen = GetDC(nullptr);
    HDC memory = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, fullWidth, fullHeight);
    HGDIOBJ previous = SelectObject(memory, bitmap);
    BOOL printed = PrintWindow(window, memory, PW_RENDERFULLCONTENT);
    if (!printed) {
        // Fall back to the screen contents under the client area.
        BitBlt(memory, offsetX, offsetY, width, height, screen, origin.x, origin.y, SRCCOPY);
    }

    BITMAPINFOHEADER header = {};
    header.biSize = sizeof(header);
    header.biWidth = width;
    header.biHeight = height; // bottom-up
    header.biPlanes = 1;
    header.biBitCount = 24;
    header.biCompression = BI_RGB;
    const int stride = ((width * 3) + 3) & ~3;
    header.biSizeImage = stride * height;

    // Copy the client part out of the full-window bitmap.
    HDC clientDc = CreateCompatibleDC(screen);
    HBITMAP clientBitmap = CreateCompatibleBitmap(screen, width, height);
    HGDIOBJ previousClient = SelectObject(clientDc, clientBitmap);
    BitBlt(clientDc, 0, 0, width, height, memory, offsetX, offsetY, SRCCOPY);
    SelectObject(clientDc, previousClient);

    pixels.resize(header.biSizeImage);
    BITMAPINFO info = {};
    info.bmiHeader = header;
    const int lines = GetDIBits(clientDc, clientBitmap, 0, height, pixels.data(), &info, DIB_RGB_COLORS);

    DeleteObject(clientBitmap);
    DeleteDC(clientDc);
    SelectObject(memory, previous);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    return lines == height;
}

bool CaptureWindowToBmp(void* windowHandle, const std::wstring& path) {
    std::vector<unsigned char> pixels;
    int width = 0;
    int height = 0;
    if (!CaptureWindowPixels(windowHandle, pixels, width, height)) {
        return false;
    }
    BITMAPINFOHEADER header = {};
    header.biSize = sizeof(header);
    header.biWidth = width;
    header.biHeight = height; // bottom-up
    header.biPlanes = 1;
    header.biBitCount = 24;
    header.biCompression = BI_RGB;
    header.biSizeImage = (DWORD)pixels.size();

    BITMAPFILEHEADER file = {};
    file.bfType = 0x4D42;
    file.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    file.bfSize = file.bfOffBits + header.biSizeImage;
    FILE* out = _wfopen(path.c_str(), L"wb");
    if (out == nullptr) {
        return false;
    }
    fwrite(&file, sizeof(file), 1, out);
    fwrite(&header, sizeof(header), 1, out);
    fwrite(pixels.data(), 1, pixels.size(), out);
    fclose(out);
    return true;
}

} // namespace MergeShipHost
