#include <string>
#include <memory>

#ifndef CSHARPHOST_H
#define CSHARPHOST_H

// CoreCLR hosting via nethost + hostfxr.
// When ICG_SCRIPTING_CSHARP is not defined, the entire C# subsystem compiles
// as stubs so the engine still builds on platforms without a .NET runtime.

class CSriptHost {
    private:
        bool initialized = false;
        void* hostfxrLib = nullptr;
        void* nethostLib = nullptr;

        // Function pointers resolved from hostfxr at runtime
        using hostfxr_initialize_for_runtime_config_fn = int(*)(const wchar_t*, void*, void**);
        using hostfxr_get_runtime_delegate_fn = int(*)(void*, int, void**);
        using hostfxr_close_fn = int(*)(void*);
        using load_assembly_and_get_function_pointer_fn = int(*)(const wchar_t*, const wchar_t*, const wchar_t*, const wchar_t*, void*, void**);

        hostfxr_initialize_for_runtime_config_fn init_fptr = nullptr;
        hostfxr_get_runtime_delegate_fn get_delegate_fptr = nullptr;
        hostfxr_close_fn close_fptr = nullptr;
        load_assembly_and_get_function_pointer_fn load_assembly_fptr = nullptr;

        void* hostfxr_handle = nullptr;

    public:
        CSriptHost();
        ~CSriptHost();

        bool Initialize();
        void Shutdown();
        bool IsInitialized() const;

        load_assembly_and_get_function_pointer_fn GetLoadAssemblyFunction() const;
};

CSriptHost* GetCSriptHost();

#endif
