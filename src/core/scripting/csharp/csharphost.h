#include <string>
#include <memory>

#ifndef CSHARPHOST_H
#define CSHARPHOST_H

// CoreCLR hosting via nethost + hostfxr.
// When ICG_SCRIPTING_CSHARP is not defined, the entire C# subsystem compiles
// as stubs so the engine still builds on platforms without a .NET runtime.

// hostfxr spells every string parameter as `char_t`, which is wchar_t on
// Windows and plain char on Linux/macOS. hostfxr.h cannot be included here (see
// the note on hostfxr_handle below), so mirror that choice locally: declaring
// these pointers with an unconditional wchar_t made csharphost.cpp fail to
// compile off Windows with "cannot convert 'const char*' to 'const wchar_t*'".
#if defined(_WIN32)
using icg_hostfxr_char = wchar_t;
#else
using icg_hostfxr_char = char;
#endif

class CSriptHost {
    private:
        bool initialized = false;
        void* hostfxrLib = nullptr;
        void* nethostLib = nullptr;

        // Function pointers resolved from hostfxr at runtime
        using hostfxr_initialize_for_runtime_config_fn = int(*)(const icg_hostfxr_char*, void*, void**);
        using hostfxr_get_runtime_delegate_fn = int(*)(void*, int, void**);
        using hostfxr_close_fn = int(*)(void*);
        using load_assembly_and_get_function_pointer_fn = int(*)(const icg_hostfxr_char*, const icg_hostfxr_char*, const icg_hostfxr_char*, const icg_hostfxr_char*, void*, void**);

        hostfxr_initialize_for_runtime_config_fn init_fptr = nullptr;
        hostfxr_get_runtime_delegate_fn get_delegate_fptr = nullptr;
        hostfxr_close_fn close_fptr = nullptr;
        load_assembly_and_get_function_pointer_fn load_assembly_fptr = nullptr;

        // Deliberately NOT named `hostfxr_handle`: hostfxr.h declares a
        // typedef of that name, and a member with the same name would shadow
        // the typedef inside every member function below, turning
        // `hostfxr_handle cxt = nullptr;` into a syntax error. This header
        // intentionally does not include hostfxr.h so it also compiles in
        // stub mode, hence the locally declared function pointer types.
        void* hostfxr_ctx = nullptr;

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
