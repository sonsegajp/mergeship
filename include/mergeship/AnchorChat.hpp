// MergeShip2 -- Anchor text chat, the part both engines share. Header-only C++ (ImGui, nlohmann::json, libultraship).
//
//   T                  opens the chat box (bottom left) with a text field; the game gets no input while it is open
//   Enter              sends what was typed (an empty line just closes the box); the game's input stays off until
//                      Enter is let go, so the key never reaches the game as Start
//
// A message goes to the room as an MS_ANCHOR_CHAT packet (the server passes a packet type it does not know on to the
// room, with the sender's clientId). The chat box lists the recent messages with each player's name in their Anchor
// color; each engine also shows a player's latest message above their head (MsAnchorChat::Bubbles, drawn with that
// engine's name tags).
//
// In mergeship.exe the history is a host blob (MS_ANCHOR_BLOB_CHAT), so Hyrule and Termina show one conversation:
// a received message is added by the engine that claims its packet, a sent one by the engine that sent it. Without a
// host (soh.exe, 2ship.exe) the history is the engine's own.
#pragma once

#include <mergeship/AnchorHost.hpp>

#include <imgui.h>
#include <libultraship/bridge/controllerbridge.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <string>

struct MsChatMessage {
    uint32_t clientId = 0;
    bool self = false;
    std::string name;
    uint8_t r = 255, g = 255, b = 255;
    std::string text;
    int64_t timeMs = 0; // MsAnchorChat::NowMs() when it arrived (one clock for both engines: one process)
};

class MsAnchorChat {
  public:
    static constexpr size_t kMaxHistory = 50;
    static constexpr size_t kMaxLength = 120;       // characters in one message
    static constexpr int64_t kBubbleMs = 8000;      // a message stays above its sender's head this long
    static constexpr int64_t kBoxMs = 15000;        // the closed chat box stays up this long after the last message
    static constexpr int64_t kBoxFadeMs = 3000;     // ... fading out over its last part
    static constexpr int32_t kInputBlockId = 0x4D53; // ControllerBlockGameInput id ('MS')

    const MsAnchorHost* host = nullptr;

    static int64_t NowMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }

    // Printable ASCII only (both games' fonts), at most kMaxLength characters, trimmed.
    static std::string Clean(const std::string& text) {
        std::string out;
        for (char c : text) {
            if ((c >= ' ') && (c <= '~') && (out.size() < kMaxLength)) {
                out += c;
            }
        }
        size_t start = out.find_first_not_of(' ');
        if (start == std::string::npos) {
            return "";
        }
        return out.substr(start, out.find_last_not_of(' ') - start + 1);
    }

    // The packet for a message this player sends.
    static nlohmann::json Packet(const std::string& text, const std::string& name, uint8_t r, uint8_t g, uint8_t b) {
        return { { "type", MS_ANCHOR_CHAT },
                 { "message", text },
                 { "name", name },
                 { "color", { { "r", r }, { "g", g }, { "b", b } } } };
    }

    // A received MS_ANCHOR_CHAT packet as a message (name and color from the packet; the engine prefers its own
    // client list's, which the caller fills in when it knows the sender).
    static bool FromPacket(const nlohmann::json& payload, MsChatMessage& out) {
        if (!payload.contains("message") || !payload["message"].is_string()) {
            return false;
        }
        out.text = Clean(payload["message"].get<std::string>());
        if (out.text.empty()) {
            return false;
        }
        out.clientId = (payload.contains("clientId") && payload["clientId"].is_number())
                           ? payload["clientId"].get<uint32_t>()
                           : 0;
        out.name = (payload.contains("name") && payload["name"].is_string()) ? payload["name"].get<std::string>()
                                                                           : "Player";
        if (payload.contains("color") && payload["color"].is_object()) {
            const auto& color = payload["color"];
            out.r = color.value("r", (uint8_t)255);
            out.g = color.value("g", (uint8_t)255);
            out.b = color.value("b", (uint8_t)255);
        }
        out.timeMs = NowMs();
        return true;
    }

    void Add(const MsChatMessage& message) {
        Refresh();
        mMessages.push_back(message);
        while (mMessages.size() > kMaxHistory) {
            mMessages.pop_front();
        }
        mScrollToBottom = true;
        if ((host != nullptr) && host->Present()) {
            nlohmann::json list = nlohmann::json::array();
            for (const auto& m : mMessages) {
                list.push_back({ { "id", m.clientId },
                                 { "self", m.self },
                                 { "name", m.name },
                                 { "r", m.r },
                                 { "g", m.g },
                                 { "b", m.b },
                                 { "text", m.text },
                                 { "time", m.timeMs } });
            }
            mBlobText = list.dump();
            host->SetBlob(MS_ANCHOR_BLOB_CHAT, mBlobText);
        }
    }

    const std::deque<MsChatMessage>& Messages() {
        Refresh();
        return mMessages;
    }

    bool IsOpen() const {
        return mOpen;
    }

    // Each sender's latest message still above their head (clientId -> text; this player's own under `selfKey`).
    std::map<uint32_t, std::string> Bubbles(uint32_t selfKey) {
        std::map<uint32_t, std::string> bubbles;
        const int64_t now = NowMs();
        for (const auto& m : Messages()) {
            if (now - m.timeMs < kBubbleMs) {
                bubbles[m.self ? selfKey : m.clientId] = Wrap(m.text, 28, 4);
            }
        }
        return bubbles;
    }

    // Word-wrapped to `width` characters a line, at most `lines` lines.
    static std::string Wrap(const std::string& text, size_t width, size_t lines) {
        std::string out;
        std::string line;
        size_t count = 1;
        size_t pos = 0;
        while (pos < text.size()) {
            size_t end = text.find(' ', pos);
            std::string word = text.substr(pos, (end == std::string::npos) ? std::string::npos : end - pos);
            pos = (end == std::string::npos) ? text.size() : end + 1;
            while (word.size() > width) { // a word longer than a line is cut
                if (!line.empty()) {
                    if (count == lines) {
                        return out + line + "...";
                    }
                    out += line + "\n";
                    line.clear();
                    count++;
                }
                if (count == lines) {
                    return out + word.substr(0, width - 3) + "...";
                }
                out += word.substr(0, width) + "\n";
                word = word.substr(width);
                count++;
            }
            if (line.empty()) {
                line = word;
            } else if (line.size() + 1 + word.size() <= width) {
                line += " " + word;
            } else {
                if (count == lines) {
                    return out + line + "...";
                }
                out += line + "\n";
                line = word;
                count++;
            }
        }
        return out + line;
    }

    // Once per frame, in the engine's ImGui pass. `canChat`: Anchor is connected, the option is on, this engine is
    // the one being played, and no menu is open. `send` sends a message and adds it to the history.
    void Draw(bool canChat, bool menuOpen, const std::function<void(const std::string&)>& send) {
        ImGuiIO& io = ImGui::GetIO();

        if (!canChat && mOpen) {
            Close();
        }
        if (canChat && !mOpen && !menuOpen && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_T, false)) {
            mOpen = true;
            mFocus = true;
            mScrollToBottom = true;
            mInput[0] = '\0';
            ControllerBlockGameInput((uint16_t)kInputBlockId);
        }
        // Enter (Start on the keyboard) reaches the game only once it has been let go.
        if (!mOpen && mBlockedUntilRelease && !ImGui::IsKeyDown(ImGuiKey_Enter) &&
            !ImGui::IsKeyDown(ImGuiKey_KeypadEnter)) {
            mBlockedUntilRelease = false;
            ControllerUnblockGameInput((uint16_t)kInputBlockId);
        }

        const auto& messages = Messages();
        const int64_t now = NowMs();
        float alpha = 1.0f;
        if (!mOpen) {
            if (messages.empty()) {
                return;
            }
            const int64_t age = now - messages.back().timeMs;
            if (age >= kBoxMs) {
                return;
            }
            if (age > kBoxMs - kBoxFadeMs) {
                alpha = (float)(kBoxMs - age) / (float)kBoxFadeMs;
            }
        }

        const float fontSize = ImGui::GetFontSize();
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + fontSize * 0.75f,
                                       viewport->WorkPos.y + viewport->WorkSize.y - fontSize * 0.75f),
                                ImGuiCond_Always, ImVec2(0.0f, 1.0f));
        ImGui::SetNextWindowSize(ImVec2(std::min(fontSize * 30.0f, viewport->WorkSize.x - fontSize * 1.5f), 0.0f),
                                 ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.55f * alpha);
        if (mFocus) {
            ImGui::SetNextWindowFocus();
        }
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar |
                                 ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_AlwaysAutoResize;
        if (!mOpen) {
            flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
        }
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
        if (ImGui::Begin("##MergeShipAnchorChat", nullptr, flags)) {
            // Closed: the last few messages. Open: the history, scrollable.
            const size_t shown = mOpen ? messages.size() : std::min<size_t>(messages.size(), 6);
            const float height = mOpen ? fontSize * 12.0f : 0.0f;
            if (mOpen) {
                ImGui::BeginChild("##history", ImVec2(0.0f, height), false, ImGuiWindowFlags_NoScrollbar);
            }
            for (size_t i = messages.size() - shown; i < messages.size(); i++) {
                const MsChatMessage& m = messages[i];
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextColored(ImVec4(m.r / 255.0f, m.g / 255.0f, m.b / 255.0f, 1.0f), "%s:", m.name.c_str());
                ImGui::SameLine(0.0f, fontSize * 0.3f);
                ImGui::TextUnformatted(m.text.c_str());
                ImGui::PopTextWrapPos();
            }
            if (mOpen) {
                if (mScrollToBottom) {
                    ImGui::SetScrollHereY(1.0f);
                    mScrollToBottom = false;
                }
                ImGui::EndChild();
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (mFocus) {
                    ImGui::SetKeyboardFocusHere();
                    mFocus = false;
                }
                if (ImGui::InputTextWithHint("##chatInput", "Press Enter to send", mInput, sizeof(mInput),
                                             ImGuiInputTextFlags_EnterReturnsTrue)) {
                    const std::string text = Clean(mInput);
                    if (!text.empty()) {
                        send(text);
                    }
                    Close();
                } else if (!ImGui::IsItemActive()) {
                    // Clicked away (the history, another window, the game): the text field keeps the keyboard.
                    mFocus = true;
                }
            }
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

  private:
    std::deque<MsChatMessage> mMessages;
    std::string mBlobText;
    bool mOpen = false;
    bool mFocus = false;
    bool mScrollToBottom = false;
    bool mBlockedUntilRelease = false;
    char mInput[kMaxLength + 8] = {};

    void Close() {
        mOpen = false;
        mFocus = false;
        mInput[0] = '\0';
        mBlockedUntilRelease = true; // unblocked once Enter is up (Draw)
    }

    // The host's history, if another engine (or this one) changed it.
    void Refresh() {
        if ((host == nullptr) || !host->Present()) {
            return;
        }
        std::string text;
        if (!host->GetBlob(MS_ANCHOR_BLOB_CHAT, text) || (text == mBlobText)) {
            return;
        }
        mBlobText = text;
        try {
            nlohmann::json list = nlohmann::json::parse(text);
            std::deque<MsChatMessage> messages;
            for (const auto& m : list) {
                MsChatMessage message;
                message.clientId = m.value("id", (uint32_t)0);
                message.self = m.value("self", false);
                message.name = m.value("name", std::string("Player"));
                message.r = m.value("r", (uint8_t)255);
                message.g = m.value("g", (uint8_t)255);
                message.b = m.value("b", (uint8_t)255);
                message.text = m.value("text", std::string());
                message.timeMs = m.value("time", (int64_t)0);
                messages.push_back(message);
            }
            if (messages.size() != mMessages.size()) {
                mScrollToBottom = true;
            }
            mMessages.swap(messages);
        } catch (...) {}
    }
};
