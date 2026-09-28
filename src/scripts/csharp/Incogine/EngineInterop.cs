using System;
using System.Runtime.InteropServices;

namespace Incogine
{
    /// <summary>
    /// Managed wrappers over the native exports in
    /// src/core/scripting/csharp/csharpinterop.h. Intended for static C#
    /// scripts (see examples/StaticPlayer.cs): each callback receives the
    /// owner Object as IntPtr and calls back through these APIs.
    /// Native booleans are int (0/1); wrappers convert to bool.
    /// </summary>
    public static class TimeApi
    {
        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        private static extern double Incogine_Time_GetDeltaTime();

        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        private static extern double Incogine_Time_GetTime();

        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        private static extern float Incogine_Time_GetFPS();

        /// <summary>Seconds since the last frame.</summary>
        public static double DeltaTime() => Incogine_Time_GetDeltaTime();

        /// <summary>Seconds since SDL init.</summary>
        public static double Time() => Incogine_Time_GetTime();

        public static float FPS() => Incogine_Time_GetFPS();
    }

    public static class LogApi
    {
        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern void Incogine_Log(int level, string msg);

        public static void Info(string msg) => Incogine_Log(0, msg);
        public static void Warn(string msg) => Incogine_Log(1, msg);
        public static void Error(string msg) => Incogine_Log(2, msg);
        public static void Debug(string msg) => Incogine_Log(3, msg);
    }

    public static class SceneApi
    {
        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern void Incogine_Scene_SetByName(string name);

        /// <summary>
        /// Switch scene by name: "MainScene", "GameScene", "Splash",
        /// "Settings", "Credits" (case-insensitive, "Main"/"Game" shortcuts).
        /// </summary>
        public static void SetScene(string name) => Incogine_Scene_SetByName(name);
    }

    public static class InputApi
    {
        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern int Incogine_Input_IsKeyDown(string keyName);

        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        private static extern void Incogine_Input_GetMousePosition(out float x, out float y);

        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int Incogine_Input_IsMouseButtonDown(int button);

        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern int Incogine_Input_IsGamepadButtonDown(int playerIndex, string buttonName);

        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern float Incogine_Input_GetGamepadAxis(int playerIndex, string axisName);

        /// <summary>Key names follow SDL scancodes: "W", "Space", "Escape", "Return", ...</summary>
        public static bool IsKeyDown(string keyName) => Incogine_Input_IsKeyDown(keyName) != 0;

        public static Vector3 GetMousePosition()
        {
            Incogine_Input_GetMousePosition(out float x, out float y);
            return new Vector3(x, y, 0.0f);
        }

        /// <summary>Button: 1 = left, 2 = middle, 3 = right.</summary>
        public static bool IsMouseButtonDown(int button) => Incogine_Input_IsMouseButtonDown(button) != 0;

        /// <summary>Buttons: "A", "B", "X", "Y", "Back", "Start", "DpadUp", ...</summary>
        public static bool IsGamepadButtonDown(int playerIndex, string buttonName) =>
            Incogine_Input_IsGamepadButtonDown(playerIndex, buttonName) != 0;

        /// <summary>Axes: "LeftStickX/Y", "RightStickX/Y" (-1..1), "LeftTrigger"/"RightTrigger" (0..1).</summary>
        public static float GetGamepadAxis(int playerIndex, string axisName) =>
            Incogine_Input_GetGamepadAxis(playerIndex, axisName);
    }

    public static class SaveApi
    {
        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern void Incogine_Save_Set(string key, string value);

        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern IntPtr Incogine_Save_Get(string key);

        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern int Incogine_Save_Has(string key);

        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern int Incogine_Save_Remove(string key);

        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        private static extern void Incogine_Save_Clear();

        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int Incogine_Save_Save();

        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int Incogine_Save_Load();

        public static void Set(string key, string value) => Incogine_Save_Set(key, value);

        public static string Get(string key) =>
            Marshal.PtrToStringAnsi(Incogine_Save_Get(key)) ?? string.Empty;

        public static bool Has(string key) => Incogine_Save_Has(key) != 0;
        public static bool Remove(string key) => Incogine_Save_Remove(key) != 0;
        public static void Clear() => Incogine_Save_Clear();

        public static bool Save() => Incogine_Save_Save() != 0;
        public static bool Load() => Incogine_Save_Load() != 0;
    }

    /// <summary>
    /// Opaque handle over native Audio*. Load from an asset-root-relative
    /// path (e.g. "audio/testbgm.ogg"), Play(loop): -1 infinite, 0 once.
    /// Engine Audio has no volume/seek yet; those are future work.
    /// </summary>
    public sealed class AudioHandle : IDisposable
    {
        private IntPtr nativePtr;

        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern IntPtr Incogine_Audio_Load(string path);

        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        private static extern void Incogine_Audio_Play(IntPtr handle, int loop);

        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        private static extern void Incogine_Audio_Stop(IntPtr handle);

        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        private static extern void Incogine_Audio_Free(IntPtr handle);

        private AudioHandle(IntPtr ptr) { nativePtr = ptr; }

        public static AudioHandle? Load(string path)
        {
            IntPtr ptr = Incogine_Audio_Load(path);
            return ptr == IntPtr.Zero ? null : new AudioHandle(ptr);
        }

        public void Play(int loop = 0) => Incogine_Audio_Play(nativePtr, loop);
        public void Stop() => Incogine_Audio_Stop(nativePtr);

        public void Dispose()
        {
            if (nativePtr != IntPtr.Zero)
            {
                Incogine_Audio_Free(nativePtr);
                nativePtr = IntPtr.Zero;
            }
            GC.SuppressFinalize(this);
        }

        ~AudioHandle() => Dispose();
    }

    /// <summary>
    /// Object extras beyond ObjectInterop (transform + name): color access
    /// via the Sprite component (or Square), plus find/create/destroy
    /// through the engine's live-object registry.
    /// </summary>
    public static class ObjectApi
    {
        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern void Incogine_Object_SetName(IntPtr obj, string name);

        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        private static extern void Incogine_Object_SetColor(IntPtr obj, int r, int g, int b, int a);

        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        private static extern void Incogine_Object_GetColor(IntPtr obj, out int r, out int g, out int b, out int a);

        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern IntPtr Incogine_Object_Find(string name);

        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern IntPtr Incogine_Object_Create(string name, float x, float y, float z);

        [DllImport(InteropLib.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        private static extern void Incogine_Object_Destroy(IntPtr obj);

        public static void SetName(IntPtr obj, string name) => Incogine_Object_SetName(obj, name);

        public static void SetColor(IntPtr obj, int r, int g, int b, int a) =>
            Incogine_Object_SetColor(obj, r, g, b, a);

        public static void GetColor(IntPtr obj, out int r, out int g, out int b, out int a) =>
            Incogine_Object_GetColor(obj, out r, out g, out b, out a);

        /// <summary>First live Object with this name, or IntPtr.Zero.</summary>
        public static IntPtr Find(string name) => Incogine_Object_Find(name);

        /// <summary>New heap Object (scale 1, no rotation). Owner must destroy it.</summary>
        public static IntPtr Create(string name, float x, float y, float z) =>
            Incogine_Object_Create(name, x, y, z);

        public static void Destroy(IntPtr obj) => Incogine_Object_Destroy(obj);
    }
}
