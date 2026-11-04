#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace MergeShipHost {

// --record FILE: every frame the host runs, captured from the game window and piped to ffmpeg (one video frame per
// game frame, 60 fps, so the video's timeline is the game's frame count). FILE.frames.csv maps each video frame to
// the host frame and the engine that drew it. The first frame's size is the video's; later frames of another size
// are cropped or padded to it.
class FrameRecorder {
  public:
    ~FrameRecorder();

    // ffmpeg: the ffmpeg executable (a bare name is looked up on PATH).
    void Open(const std::wstring& path, const std::wstring& ffmpeg);
    void Frame(uint64_t hostFrame, const char* engine);
    void Close();
    bool Active() const {
        return mWanted;
    }

  private:
    bool Start(int width, int height);

    bool mWanted = false;
    bool mFailed = false;
    std::wstring mPath;
    std::wstring mFfmpeg;
    void* mPipe = nullptr;    // HANDLE: ffmpeg's stdin
    void* mProcess = nullptr; // HANDLE
    int mWidth = 0;
    int mHeight = 0;
    uint64_t mWritten = 0;
    FILE* mIndex = nullptr;
    std::vector<unsigned char> mPixels;
    std::vector<unsigned char> mFrame;
};

} // namespace MergeShipHost
