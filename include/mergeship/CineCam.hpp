// MergeShip2 -- the test harness's film camera ("cinecam"), shared by both engines' harnesses (SoH MergeShipTest.cpp,
// 2Ship MergeShipTest.cpp / MergeShipHostTest.cpp): where a camera stands (eye) and looks (at) for Link's position and
// facing each frame, smoothed like a drone that trails him. Each engine applies it with a sub camera of its own.
//
//   cinecam follow [dist] [height] [angle] [lag]   behind Link, `angle` degrees round from his back, trailing his turns
//   cinecam front  [dist] [height] [angle] [lag]   in front of Link (he faces the camera)
//   cinecam orbit  [dist] [height] [speed] [angle] circling Link, `speed` degrees a frame, starting `angle` off his front
//   cinecam fixed  <x> <y> <z> [lag]               standing still at a point, turning to follow Link
//   cinecam look   <height>                        the point looked at, this far above Link's feet (default 45)
//   cinecam cut                                    the next frame jumps to the wanted shot instead of gliding there
//   cinecam off                                    the game's own camera again
//
// A change of mode or numbers while a shot runs glides to the new one (lag: the share of the way covered each frame,
// 0.01 floaty .. 1 rigid); the first frame of a shot, and "cut", jump straight to it.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

struct MsCineCam {
    enum Mode { OFF, FOLLOW, FRONT, ORBIT, FIXED };

    int mode = OFF;
    float dist = 450.0f;
    float height = 150.0f;
    float angle = 0.0f;       // degrees
    float lag = 0.06f;        // per frame, 0..1
    float orbitSpeed = 0.6f;  // degrees per frame
    float lookHeight = 45.0f; // above Link's feet
    float fixedEye[3] = { 0.0f, 0.0f, 0.0f };

    bool primed = false; // false: the next frame jumps to the wanted shot
    float yaw = 0.0f;    // radians, the camera's bearing from Link
    float eye[3] = { 0.0f, 0.0f, 0.0f };
    float at[3] = { 0.0f, 0.0f, 0.0f };

    // One "cinecam ..." line (args without the op). Returns a line for the harness log.
    std::string Command(const std::vector<std::string>& args) {
        auto num = [&](size_t i, float fallback) {
            return (i < args.size() && !args[i].empty()) ? (float)atof(args[i].c_str()) : fallback;
        };
        const std::string what = args.empty() ? std::string("off") : args[0];
        const bool wasOff = (mode == OFF);

        if (what == "off") {
            mode = OFF;
        } else if ((what == "follow") || (what == "front")) {
            mode = (what == "follow") ? FOLLOW : FRONT;
            dist = num(1, 450.0f);
            height = num(2, 150.0f);
            angle = num(3, 0.0f);
            lag = num(4, 0.06f);
        } else if (what == "orbit") {
            mode = ORBIT;
            dist = num(1, 400.0f);
            height = num(2, 120.0f);
            orbitSpeed = num(3, 0.6f);
            angle = num(4, 0.0f);
            lag = 0.2f;
        } else if (what == "fixed") {
            mode = FIXED;
            fixedEye[0] = num(1, 0.0f);
            fixedEye[1] = num(2, 0.0f);
            fixedEye[2] = num(3, 0.0f);
            lag = num(4, 0.2f);
        } else if (what == "look") {
            lookHeight = num(1, 45.0f);
        } else if (what == "cut") {
            primed = false;
        } else {
            return "CINECAM unknown '" + what + "'";
        }
        if (wasOff && (mode != OFF)) {
            primed = false;
        }
        char line[160];
        snprintf(line, sizeof(line), "CINECAM %s mode=%d dist=%.0f height=%.0f angle=%.0f lag=%.2f look=%.0f",
                 what.c_str(), mode, dist, height, angle, lag, lookHeight);
        return line;
    }

    // Link at (px, py, pz) facing `shapeYaw` (binary angle): the camera's at and eye for this frame.
    void Update(float px, float py, float pz, int16_t shapeYaw, float outAt[3], float outEye[3]) {
        const float kPi = 3.14159265f;
        const float kDeg = kPi / 180.0f;
        const float linkYaw = (float)shapeYaw * (kPi / 32768.0f);
        float wantYaw = yaw;
        float wantEye[3];
        const float wantAt[3] = { px, py + lookHeight, pz };

        switch (mode) {
            case FOLLOW:
                wantYaw = linkYaw + kPi + (angle * kDeg);
                break;
            case FRONT:
                wantYaw = linkYaw + (angle * kDeg);
                break;
            case ORBIT:
                wantYaw = (primed ? yaw : (linkYaw + (angle * kDeg))) + (orbitSpeed * kDeg);
                break;
            default:
                break;
        }
        if (!primed || (mode == ORBIT)) {
            yaw = wantYaw;
        } else {
            float d = wantYaw - yaw;
            while (d > kPi) {
                d -= 2.0f * kPi;
            }
            while (d < -kPi) {
                d += 2.0f * kPi;
            }
            yaw += d * lag;
        }
        if (mode == FIXED) {
            wantEye[0] = fixedEye[0];
            wantEye[1] = fixedEye[1];
            wantEye[2] = fixedEye[2];
        } else {
            wantEye[0] = px + sinf(yaw) * dist;
            wantEye[1] = py + height;
            wantEye[2] = pz + cosf(yaw) * dist;
        }
        const float eyeLag = (lag * 2.0f > 1.0f) ? 1.0f : (lag * 2.0f);
        for (int i = 0; i < 3; i++) {
            if (!primed) {
                eye[i] = wantEye[i];
                at[i] = wantAt[i];
            } else {
                eye[i] += (wantEye[i] - eye[i]) * eyeLag;
                at[i] += (wantAt[i] - at[i]) * 0.25f;
            }
            outAt[i] = at[i];
            outEye[i] = eye[i];
        }
        primed = true;
    }
};
