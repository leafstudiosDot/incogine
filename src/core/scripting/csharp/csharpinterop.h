// C ABI exported from the Incogine executable for C# P/Invoke.
//
// Managed declarations live in src/scripts/csharp/Incogine/*Interop.cs and
// must match these signatures exactly (Cdecl, C names, float = 32-bit).
//
// Managed code resolves LibName "Incogine" to the host executable via
// InteropLib.cs (NativeLibrary.SetDllImportResolver -> main program handle),
// so the game target must be built with ENABLE_EXPORTS (see CMakeLists.txt).
//
// New code: prefer adding a function here + a wrapper in EngineInterop.cs
// over ad-hoc DllImports elsewhere.

#ifndef CSHARPINTEROP_H
#define CSHARPINTEROP_H

#if defined(_WIN32)
    #define ICG_API extern "C" __declspec(dllexport)
#else
    #define ICG_API extern "C" __attribute__((visibility("default")))
#endif

// Boolean results are int (0/1): C++ bool is 1 byte, C# P/Invoke bool
// defaults to 4-byte BOOL, so int avoids marshalling mismatches.

// --- Object transform (signatures must match ObjectInterop.cs) ---
ICG_API void Incogine_Object_SetPosition(void* obj, float x, float y, float z);
ICG_API void Incogine_Object_GetPosition(void* obj, float* x, float* y, float* z);
ICG_API void Incogine_Object_SetScale(void* obj, float x, float y, float z);
ICG_API void Incogine_Object_GetScale(void* obj, float* x, float* y, float* z);
ICG_API void Incogine_Object_SetRotation(void* obj, float x, float y, float z);
ICG_API void Incogine_Object_GetRotation(void* obj, float* x, float* y, float* z);
ICG_API const char* Incogine_Object_GetName(void* obj);

// --- Object extras ---
ICG_API void Incogine_Object_SetName(void* obj, const char* name);
ICG_API void Incogine_Object_SetColor(void* obj, int r, int g, int b, int a);
ICG_API void Incogine_Object_GetColor(void* obj, int* r, int* g, int* b, int* a);

// --- Object registry / lifetime ---
// Find returns null when no live Object has that name. Create returns a
// heap Object the caller owns (delete via Destroy, e.g. from a Scene).
ICG_API void* Incogine_Object_Find(const char* name);
ICG_API void* Incogine_Object_Create(const char* name, float x, float y, float z);
ICG_API void Incogine_Object_Destroy(void* obj);

// --- Time (seconds) ---
ICG_API double Incogine_Time_GetDeltaTime();
ICG_API double Incogine_Time_GetTime();
ICG_API float Incogine_Time_GetFPS();

// --- Log (level: 0=INFO, 1=WARN, 2=ERROR, 3=DEBUG) ---
ICG_API void Incogine_Log(int level, const char* msg);

// --- Input (keyboard/mouse/gamepad polling) ---
ICG_API int Incogine_Input_IsKeyDown(const char* keyName);
ICG_API void Incogine_Input_GetMousePosition(float* x, float* y);
ICG_API int Incogine_Input_IsMouseButtonDown(int button);
ICG_API int Incogine_Input_IsGamepadButtonDown(int playerIndex, const char* buttonName);
ICG_API float Incogine_Input_GetGamepadAxis(int playerIndex, const char* axisName);

// --- Scene ---
// Name is matched case-insensitively against known scenes
// ("MainScene", "GameScene", "Splash", "Settings", "Credits").
// Unknown names log an error and no-op.
ICG_API void Incogine_Scene_SetByName(const char* name);

// --- Audio (opaque Audio* handle; null-safe) ---
ICG_API void* Incogine_Audio_Load(const char* path);
ICG_API void Incogine_Audio_Play(void* handle, int loop);
ICG_API void Incogine_Audio_Stop(void* handle);
ICG_API void Incogine_Audio_Free(void* handle);

// --- Save (string key-value store, file-backed) ---
ICG_API void Incogine_Save_Set(const char* key, const char* value);
ICG_API const char* Incogine_Save_Get(const char* key);
ICG_API int Incogine_Save_Has(const char* key);
ICG_API int Incogine_Save_Remove(const char* key);
ICG_API void Incogine_Save_Clear();
ICG_API int Incogine_Save_Save();
ICG_API int Incogine_Save_Load();

#endif
