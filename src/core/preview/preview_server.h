// Incogine studio-preview producer (engine side, no SDL/GL dependencies).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Owns the shared-memory segment (Windows named mapping, POSIX shm) and
// publishes bottom-up RGBA frames plus the command ack slot. Frame capture
// itself (glReadPixels) lives with the renderer; this class only moves
// bytes. Emscripten has no shared memory: Start() fails there by design.
#pragma once

#include <cstdint>
#include <string>

#include "preview_protocol.h"

class PreviewServer {
public:
    PreviewServer() = default;
    ~PreviewServer() { Shutdown(); }

    PreviewServer(const PreviewServer&) = delete;
    PreviewServer& operator=(const PreviewServer&) = delete;

    bool Start(std::string& error);
    void Shutdown();
    bool IsActive() const { return base_ != nullptr; }

    // Binds this session to a Studio launch (from --studio-token). Must be
    // called before Start(); 0 leaves the segment unbound.
    void SetSessionToken(uint64_t token) { pendingToken_ = token; }

    // Publishes one frame (bottom-up RGBA rows). Dimensions must match the
    // protocol exactly; anything else is ignored.
    void PushFrame(uint32_t width, uint32_t height, uint32_t pitch,
                   const void* pixelsBottomUp);

    // Returns true once per newly arrived command (see ICG_PREVIEW_CMD_*),
    // with its id + float payload.
    bool PollCommand(uint32_t& code, uint64_t& id, float values[9]);

private:
    IcgPreviewHeader* Header() {
        return reinterpret_cast<IcgPreviewHeader*>(base_);
    }

    uint8_t* base_ = nullptr;
    uint64_t pendingToken_ = 0;
#ifdef _WIN32
    void* fileMap_ = nullptr;
#else
    int fd_ = -1;
#endif
};
