// Incogine Studio — studio-preview consumer (Qt-free, stdlib only).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Reads frames published by a game running with --studio-preview and sends
// commands back (currently QUIT). Frame copies use the header seqlock so
// torn frames are discarded, never shown.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace icg {
namespace studio {
namespace preview {

class PreviewClient {
public:
    PreviewClient() = default;
    ~PreviewClient() { Disconnect(); }

    PreviewClient(const PreviewClient&) = delete;
    PreviewClient& operator=(const PreviewClient&) = delete;

    bool Connect(const std::string& name, std::string& error);
    void Disconnect();
    bool IsConnected() const { return base_ != nullptr; }

    // Copies the latest complete frame when its sequence advanced since
    // the last successful copy. Returns false when nothing new arrived or
    // the copy tore (caller retries on the next poll).
    bool TryFrame(std::vector<uint8_t>& out, uint32_t& width, uint32_t& height,
                  uint32_t& seq);

    // Sends a command (see ICG_PREVIEW_CMD_*) and waits for the engine ack.
    bool SendCommand(uint32_t code, int timeoutMs, std::string& error);
    // TRANSFORM command: moves the live object with `id` (pos/rot/scale).
    bool SendTransform(uint64_t id, const float pos[3], const float rot[3],
                       const float scale[3], int timeoutMs, std::string& error);

private:
    bool SendRaw(uint32_t code, uint64_t id, const float values[9],
                 int timeoutMs, std::string& error);
    const uint8_t* Pixels() const;

    uint8_t* base_ = nullptr;
    uint32_t lastSeq_ = 0;
#ifdef _WIN32
    void* fileMap_ = nullptr;
#else
    int fd_ = -1;
#endif
};

} // namespace preview
} // namespace studio
} // namespace icg
