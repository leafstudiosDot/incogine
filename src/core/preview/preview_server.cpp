// Incogine studio-preview producer implementation (no SDL/GL dependencies).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "preview_server.h"

#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

constexpr char kMagic[8] = {'I', 'C', 'G', 'P', 'R', 'E', 'V', '2'};

void InitHeader(IcgPreviewHeader* header) {
    std::memcpy(header->magic, kMagic, 8);
    header->version = ICG_PREVIEW_VERSION;
    header->frameSeq = 0;
    header->width = ICG_PREVIEW_WIDTH;
    header->height = ICG_PREVIEW_HEIGHT;
    header->pitch = ICG_PREVIEW_PITCH;
    header->cmdSeq = 0;
    header->cmdCode = ICG_PREVIEW_CMD_NOP;
    header->ackSeq = 0;
    std::memset(header->reserved, 0, sizeof(header->reserved));
}

} // namespace

bool PreviewServer::Start(std::string& error) {
    if (IsActive()) {
        return true;
    }
#ifdef __EMSCRIPTEN__
    error = "studio preview is not supported on Web (no shared memory)";
    return false;
#elif defined(_WIN32)
    wchar_t name[64];
    if (MultiByteToWideChar(CP_UTF8, 0, "Local\\" ICG_PREVIEW_SHM_NAME, -1, name, 64) <= 0) {
        error = "preview SHM name conversion failed";
        return false;
    }
    fileMap_ = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                  ICG_PREVIEW_SHM_SIZE, name);
    if (!fileMap_) {
        error = "preview SHM create failed";
        return false;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(fileMap_);
        fileMap_ = nullptr;
        error = "preview SHM already exists (another preview running?)";
        return false;
    }
    base_ = static_cast<uint8_t*>(MapViewOfFile(fileMap_, FILE_MAP_ALL_ACCESS, 0, 0,
                                                ICG_PREVIEW_SHM_SIZE));
    if (!base_) {
        CloseHandle(fileMap_);
        fileMap_ = nullptr;
        error = "preview SHM map failed";
        return false;
    }
#else
    const char* name = "/" ICG_PREVIEW_SHM_NAME;
    fd_ = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd_ < 0 && errno == EEXIST) {
        shm_unlink(name); // stale segment from a crashed run
        fd_ = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
    }
    if (fd_ < 0) {
        error = "preview shm_open failed";
        return false;
    }
    if (ftruncate(fd_, ICG_PREVIEW_SHM_SIZE) != 0) {
        close(fd_);
        fd_ = -1;
        shm_unlink(name);
        error = "preview ftruncate failed";
        return false;
    }
    base_ = static_cast<uint8_t*>(
        mmap(nullptr, ICG_PREVIEW_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));
    if (base_ == MAP_FAILED) {
        base_ = nullptr;
        close(fd_);
        fd_ = -1;
        shm_unlink(name);
        error = "preview mmap failed";
        return false;
    }
#endif
    InitHeader(Header());
    return true;
}

void PreviewServer::Shutdown() {
    if (!IsActive()) {
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
    shm_unlink("/" ICG_PREVIEW_SHM_NAME);
#endif
}

void PreviewServer::PushFrame(uint32_t width, uint32_t height, uint32_t pitch,
                              const void* pixelsBottomUp) {
    if (!IsActive() || !pixelsBottomUp) {
        return;
    }
    if (width != ICG_PREVIEW_WIDTH || height != ICG_PREVIEW_HEIGHT ||
        pitch != ICG_PREVIEW_PITCH) {
        return;
    }
    IcgPreviewHeader* header = Header();
    ++header->frameSeq; // odd: writing
    std::memcpy(base_ + ICG_PREVIEW_HEADER_SIZE, pixelsBottomUp,
                ICG_PREVIEW_PIXEL_BYTES);
    ++header->frameSeq; // even: ready
}

bool PreviewServer::PollCommand(uint32_t& code, uint64_t& id, float values[9]) {
    if (!IsActive()) {
        return false;
    }
    IcgPreviewHeader* header = Header();
    if (header->cmdSeq == header->ackSeq) {
        return false;
    }
    code = header->cmdCode;
    id = header->cmdId;
    for (int i = 0; i < 9; ++i) {
        values[i] = header->cmdF[i];
    }
    header->ackSeq = header->cmdSeq;
    return true;
}
