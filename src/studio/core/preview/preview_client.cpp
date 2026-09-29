// Incogine Studio — studio-preview consumer implementation (Qt-free).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "preview_client.h"

#include <chrono>
#include <cstring>
#include <thread>

#include "../../../core/preview/preview_protocol.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace icg {
namespace studio {
namespace preview {
namespace {

constexpr char kMagic[8] = {'I', 'C', 'G', 'P', 'R', 'E', 'V', '3'};

const IcgPreviewHeader* HeaderOf(const uint8_t* base) {
    return reinterpret_cast<const IcgPreviewHeader*>(base);
}

bool HeaderValid(const IcgPreviewHeader* header) {
    return std::memcmp(header->magic, kMagic, 8) == 0 &&
           header->version == ICG_PREVIEW_VERSION &&
           header->width == ICG_PREVIEW_WIDTH &&
           header->height == ICG_PREVIEW_HEIGHT &&
           header->pitch == ICG_PREVIEW_PITCH;
}

} // namespace

bool PreviewClient::Connect(const std::string& name, std::string& error) {
    return Connect(name, 0, error);
}

bool PreviewClient::Connect(const std::string& name, uint64_t expectedToken,
                            std::string& error) {
    if (IsConnected()) {
        return true;
    }
#ifdef __EMSCRIPTEN__
    error = "studio preview is not supported on Web (no shared memory)";
    return false;
#elif defined(_WIN32)
    std::wstring wname;
    wname.assign(name.begin(), name.end());
    wname = L"Local\\" + wname;
    fileMap_ = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, wname.c_str());
    if (!fileMap_) {
        error = "preview segment not published yet";
        return false;
    }
    base_ = static_cast<uint8_t*>(
        MapViewOfFile(fileMap_, FILE_MAP_ALL_ACCESS, 0, 0, ICG_PREVIEW_SHM_SIZE));
    if (!base_) {
        CloseHandle(fileMap_);
        fileMap_ = nullptr;
        error = "preview segment map failed";
        return false;
    }
#else
    const std::string fullName = "/" + name;
    fd_ = shm_open(fullName.c_str(), O_RDWR, 0); // RDWR: commands write back
    if (fd_ < 0) {
        error = "preview segment not published yet";
        return false;
    }
    struct stat st {};
    if (fstat(fd_, &st) != 0 || st.st_size != ICG_PREVIEW_SHM_SIZE) {
        close(fd_);
        fd_ = -1;
        error = "preview segment has an unexpected size";
        return false;
    }
    base_ = static_cast<uint8_t*>(
        mmap(nullptr, ICG_PREVIEW_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));
    if (base_ == MAP_FAILED) {
        base_ = nullptr;
        close(fd_);
        fd_ = -1;
        error = "preview segment map failed";
        return false;
    }
#endif
    if (!HeaderValid(HeaderOf(base_))) {
        error = "preview segment failed validation (magic/version/size)";
        Disconnect();
        return false;
    }
    if (expectedToken != 0 && HeaderOf(base_)->sessionToken != expectedToken) {
        error = "preview session token mismatch (not the build Studio launched)";
        Disconnect();
        return false;
    }
    lastSeq_ = 0;
    return true;
}

void PreviewClient::Disconnect() {
    if (!IsConnected()) {
        return;
    }
#ifdef _WIN32
    UnmapViewOfFile(base_);
    base_ = nullptr;
    CloseHandle(fileMap_);
    fileMap_ = nullptr;
#else
    munmap(base_, ICG_PREVIEW_SHM_SIZE);
    base_ = nullptr;
    close(fd_);
    fd_ = -1;
#endif
    lastSeq_ = 0;
}

const uint8_t* PreviewClient::Pixels() const {
    return base_ + ICG_PREVIEW_HEADER_SIZE;
}

bool PreviewClient::TryFrame(std::vector<uint8_t>& out, uint32_t& width,
                             uint32_t& height, uint32_t& seq) {
    if (!IsConnected()) {
        return false;
    }
    const IcgPreviewHeader* header = HeaderOf(base_);
    const uint32_t first = header->frameSeq;
    if ((first & 1u) != 0 || first == lastSeq_ || first == 0) {
        return false; // writing, unchanged, or nothing published yet
    }
    if (out.size() != ICG_PREVIEW_PIXEL_BYTES) {
        out.resize(ICG_PREVIEW_PIXEL_BYTES);
    }
    std::memcpy(out.data(), Pixels(), ICG_PREVIEW_PIXEL_BYTES);
    if (header->frameSeq != first) {
        return false; // torn copy; retry on the next poll
    }
    width = header->width;
    height = header->height;
    seq = first;
    lastSeq_ = first;
    return true;
}

bool PreviewClient::SendCommand(uint32_t code, int timeoutMs, std::string& error) {
    return SendRaw(code, 0, nullptr, timeoutMs, error);
}

bool PreviewClient::SendTransform(uint64_t id, const float pos[3], const float rot[3],
                                  const float scale[3], int timeoutMs,
                                  std::string& error) {
    float values[9];
    for (int i = 0; i < 3; ++i) {
        values[i] = pos[i];
        values[3 + i] = rot[i];
        values[6 + i] = scale[i];
    }
    return SendRaw(ICG_PREVIEW_CMD_TRANSFORM, id, values, timeoutMs, error);
}

bool PreviewClient::SendRaw(uint32_t code, uint64_t id, const float values[9],
                            int timeoutMs, std::string& error) {
    if (!IsConnected()) {
        error = "not connected";
        return false;
    }
    IcgPreviewHeader* header = reinterpret_cast<IcgPreviewHeader*>(base_);
    header->cmdCode = code;
    header->cmdId = id;
    for (int i = 0; i < 9; ++i) {
        header->cmdF[i] = values ? values[i] : 0.0f;
    }
#if defined(_MSC_VER)
    _ReadWriteBarrier();
#else
    __sync_synchronize();
#endif
    ++header->cmdSeq;
    const uint32_t want = header->cmdSeq;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (header->ackSeq == want) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    error = "engine did not acknowledge the command";
    return false;
}

} // namespace preview
} // namespace studio
} // namespace icg
