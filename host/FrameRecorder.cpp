#include "FrameRecorder.h"
#include "Screenshot.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace MergeShipHost {

FrameRecorder::~FrameRecorder() {
    Close();
}

void FrameRecorder::Open(const std::wstring& path, const std::wstring& ffmpeg) {
    mPath = path;
    mFfmpeg = ffmpeg.empty() ? L"ffmpeg" : ffmpeg;
    mWanted = !path.empty();
}

bool FrameRecorder::Start(int width, int height) {
    // yuv420p wants even sizes; the odd row or column is dropped.
    mWidth = width & ~1;
    mHeight = height & ~1;

    SECURITY_ATTRIBUTES security = { sizeof(security), nullptr, TRUE };
    HANDLE readEnd = nullptr;
    HANDLE writeEnd = nullptr;
    if (!CreatePipe(&readEnd, &writeEnd, &security, 64u << 20)) {
        return false;
    }
    SetHandleInformation(writeEnd, HANDLE_FLAG_INHERIT, 0);

    // Raw bottom-up BGR frames in; vflip turns them upright. x264 at CRF 16 keeps text and edges readable.
    wchar_t command[4096];
    swprintf(command, 4096,
             L"\"%ls\" -hide_banner -loglevel error -y -f rawvideo -pixel_format bgr24 -video_size %dx%d "
             L"-framerate 60 -i - -vf vflip -c:v libx264 -preset ultrafast -crf 16 -pix_fmt yuv420p \"%ls\"",
             mFfmpeg.c_str(), mWidth, mHeight, mPath.c_str());
    STARTUPINFOW startup = { sizeof(startup) };
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = readEnd;
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION process = {};
    const BOOL started =
        CreateProcessW(nullptr, command, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    CloseHandle(readEnd);
    if (!started) {
        CloseHandle(writeEnd);
        return false;
    }
    CloseHandle(process.hThread);
    mProcess = process.hProcess;
    mPipe = writeEnd;
    mFrame.assign((size_t)mWidth * mHeight * 3, 0);

    const std::wstring indexPath = mPath + L".frames.csv";
    mIndex = _wfopen(indexPath.c_str(), L"w");
    if (mIndex != nullptr) {
        fputs("video_frame,host_frame,engine,window_w,window_h\n", mIndex);
    }
    return true;
}

void FrameRecorder::Frame(uint64_t hostFrame, const char* engine) {
    if (!mWanted || mFailed) {
        return;
    }
    int width = 0;
    int height = 0;
    if (!CaptureWindowPixels(FindGameWindow(), mPixels, width, height)) {
        return;
    }
    if ((mPipe == nullptr) && !Start(width, height)) {
        mFailed = true;
        return;
    }
    // Rows are bottom-up, padded to 4 bytes. A resized window keeps its bottom-left corner (the image's top-left
    // after vflip is its top row, so crop and pad at the top/right).
    const size_t srcStride = ((size_t)width * 3 + 3) & ~(size_t)3;
    const size_t dstStride = (size_t)mWidth * 3;
    const int rows = std::min(height, mHeight);
    const size_t bytes = std::min((size_t)width * 3, dstStride);
    if ((width != mWidth) || (height < mHeight)) {
        std::fill(mFrame.begin(), mFrame.end(), 0);
    }
    for (int y = 0; y < rows; y++) {
        memcpy(&mFrame[(size_t)y * dstStride], &mPixels[(size_t)y * srcStride], bytes);
    }
    DWORD written = 0;
    if (!WriteFile(static_cast<HANDLE>(mPipe), mFrame.data(), (DWORD)mFrame.size(), &written, nullptr) ||
        (written != mFrame.size())) {
        mFailed = true;
        return;
    }
    if (mIndex != nullptr) {
        fprintf(mIndex, "%llu,%llu,%s,%d,%d\n", (unsigned long long)mWritten, (unsigned long long)hostFrame, engine,
                width, height);
        // Tests end the process with TerminateProcess; keep the index whole every second.
        if ((mWritten % 60) == 0) {
            fflush(mIndex);
        }
    }
    mWritten++;
}

void FrameRecorder::Close() {
    if (mIndex != nullptr) {
        fclose(mIndex);
        mIndex = nullptr;
    }
    if (mPipe != nullptr) {
        // EOF on its stdin: ffmpeg finishes the file.
        CloseHandle(static_cast<HANDLE>(mPipe));
        mPipe = nullptr;
    }
    if (mProcess != nullptr) {
        WaitForSingleObject(static_cast<HANDLE>(mProcess), 30000);
        CloseHandle(static_cast<HANDLE>(mProcess));
        mProcess = nullptr;
    }
}

} // namespace MergeShipHost
