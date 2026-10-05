#include "csharphost.h"
#include <iostream>

// nethost.h / hostfxr.h ship with the .NET SDK packs. CMake searches for
// them, but the pack layout is versioned and may not be found on every
// machine even when `dotnet` exists. Guard the include so a missing header
// degrades to the disabled-stub path below instead of breaking the build
// with C1083 (VS2026 reported this on Debug x64).
#if defined(__has_include)
#  if __has_include(<nethost.h>) && __has_include(<hostfxr.h>) && __has_include(<coreclr_delegates.h>)
#    define ICG_HAS_NETHOST 1
#  else
#    define ICG_HAS_NETHOST 0
#  endif
#else
#  define ICG_HAS_NETHOST 0
#endif

#if defined(ICG_SCRIPTING_CSHARP) && ICG_HAS_NETHOST

#if defined(_WIN32)
    #include <windows.h>
    // char_t is wchar_t on Windows (that is what get_hostfxr_path fills), so
    // the loader must be the wide variant. The library handle members are
    // kept as void* so csharphost.h stays free of windows.h, hence the casts
    // at the Win32 boundary.
    #define LOAD_LIBRARY(path) LoadLibraryW(path)
    #define GET_PROC_ADDRESS(lib, name) GetProcAddress(reinterpret_cast<HMODULE>(lib), name)
    #define FREE_LIBRARY(lib) FreeLibrary(reinterpret_cast<HMODULE>(lib))
    #define STR(x) L##x
    using char_t = wchar_t;
#else
    #include <dlfcn.h>
    #define LOAD_LIBRARY(path) dlopen(path, RTLD_NOW)
    #define GET_PROC_ADDRESS(lib, name) dlsym(lib, name)
    #define FREE_LIBRARY(lib) dlclose(lib)
    #define STR(x) x
    using char_t = char;
#endif

#include <nethost.h>
#include <hostfxr.h>
#include <coreclr_delegates.h>

static CSriptHost s_csharpHost;

CSriptHost::CSriptHost() {}

CSriptHost::~CSriptHost() {
    Shutdown();
}

bool CSriptHost::Initialize() {
    if (initialized) return true;

    // Step 1: Locate hostfxr via nethost
    char_t hostfxrPath[512];
    size_t pathSize = sizeof(hostfxrPath) / sizeof(char_t);
    int rc = get_hostfxr_path(hostfxrPath, &pathSize, nullptr);
    if (rc != 0) {
        std::cerr << "[Incogine] C# scripting: hostfxr not found (rc=" << std::hex << rc << std::dec << ")" << std::endl;
        return false;
    }

    // Step 2: Load hostfxr library
    hostfxrLib = LOAD_LIBRARY(hostfxrPath);
    if (!hostfxrLib) {
        std::cerr << "[Incogine] C# scripting: failed to load hostfxr library" << std::endl;
        return false;
    }

    // Step 3: Resolve hostfxr exports
    init_fptr = (hostfxr_initialize_for_runtime_config_fn)
        GET_PROC_ADDRESS(hostfxrLib, "hostfxr_initialize_for_runtime_config");
    get_delegate_fptr = (hostfxr_get_runtime_delegate_fn)
        GET_PROC_ADDRESS(hostfxrLib, "hostfxr_get_runtime_delegate");
    close_fptr = (hostfxr_close_fn)
        GET_PROC_ADDRESS(hostfxrLib, "hostfxr_close");

    if (!init_fptr || !get_delegate_fptr || !close_fptr) {
        std::cerr << "[Incogine] C# scripting: failed to resolve hostfxr exports" << std::endl;
        FREE_LIBRARY(hostfxrLib);
        hostfxrLib = nullptr;
        return false;
    }

    // Step 4: Initialize the .NET runtime from the runtimeconfig.json
    // The config file is expected next to the executable
    hostfxr_handle cxt = nullptr;
    rc = init_fptr(STR("Incogine.runtimeconfig.json"), nullptr, &cxt);
    if (rc != 0 || cxt == nullptr) {
        std::cerr << "[Incogine] C# scripting: runtime init failed (rc=" << std::hex << rc << std::dec << ")" << std::endl;
        if (cxt && close_fptr) close_fptr(cxt);
        return false;
    }

    // Step 5: Get the load_assembly_and_get_function_pointer delegate
    rc = get_delegate_fptr(
        cxt,
        hdt_load_assembly_and_get_function_pointer,
        (void**)&load_assembly_fptr);

    if (rc != 0 || !load_assembly_fptr) {
        std::cerr << "[Incogine] C# scripting: failed to get load_assembly delegate (rc=" << std::hex << rc << std::dec << ")" << std::endl;
        close_fptr(cxt);
        return false;
    }

    hostfxr_ctx = cxt;
    initialized = true;
    std::cout << "[Incogine] C# scripting initialized (.NET runtime loaded)" << std::endl;
    return true;
}

void CSriptHost::Shutdown() {
    if (hostfxr_ctx && close_fptr) {
        close_fptr(hostfxr_ctx);
        hostfxr_ctx = nullptr;
    }
    if (hostfxrLib) {
        FREE_LIBRARY(hostfxrLib);
        hostfxrLib = nullptr;
    }
    load_assembly_fptr = nullptr;
    init_fptr = nullptr;
    get_delegate_fptr = nullptr;
    close_fptr = nullptr;
    initialized = false;
}

bool CSriptHost::IsInitialized() const {
    return initialized;
}

CSriptHost::load_assembly_and_get_function_pointer_fn CSriptHost::GetLoadAssemblyFunction() const {
    return load_assembly_fptr;
}

CSriptHost* GetCSriptHost() {
    return &s_csharpHost;
}

#else

// Stubs when C# scripting is disabled
CSriptHost::CSriptHost() {}
CSriptHost::~CSriptHost() {}
bool CSriptHost::Initialize() {
    std::cout << "[Incogine] C# scripting disabled (not built with ICG_SCRIPTING_CSHARP)" << std::endl;
    return false;
}
void CSriptHost::Shutdown() {}
bool CSriptHost::IsInitialized() const { return false; }
CSriptHost::load_assembly_and_get_function_pointer_fn CSriptHost::GetLoadAssemblyFunction() const { return nullptr; }

static CSriptHost s_csharpHost;
CSriptHost* GetCSriptHost() { return &s_csharpHost; }

#endif
