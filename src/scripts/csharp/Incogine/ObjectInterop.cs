using System;
using System.Runtime.InteropServices;

namespace Incogine
{
    /// <summary>
    /// P/Invoke bindings to native Incogine Object functions.
    /// These are exported from the engine's native code via
    /// [UnmanagedCallersOnly] or DLL exports.
    /// </summary>
    internal static class ObjectInterop
    {
        private const string LibName = "Incogine";

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void Incogine_Object_SetPosition(IntPtr obj, float x, float y, float z);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void Incogine_Object_GetPosition(IntPtr obj, out float x, out float y, out float z);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void Incogine_Object_SetScale(IntPtr obj, float x, float y, float z);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void Incogine_Object_GetScale(IntPtr obj, out float x, out float y, out float z);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void Incogine_Object_SetRotation(IntPtr obj, float x, float y, float z);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void Incogine_Object_GetRotation(IntPtr obj, out float x, out float y, out float z);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr Incogine_Object_GetName(IntPtr obj);

        // Managed convenience wrappers

        public static void SetPosition(IntPtr obj, float x, float y, float z)
        {
            Incogine_Object_SetPosition(obj, x, y, z);
        }

        public static Vector3 GetPosition(IntPtr obj)
        {
            Incogine_Object_GetPosition(obj, out float x, out float y, out float z);
            return new Vector3(x, y, z);
        }

        public static void SetScale(IntPtr obj, float x, float y, float z)
        {
            Incogine_Object_SetScale(obj, x, y, z);
        }

        public static Vector3 GetScale(IntPtr obj)
        {
            Incogine_Object_GetScale(obj, out float x, out float y, out float z);
            return new Vector3(x, y, z);
        }

        public static void SetRotation(IntPtr obj, float x, float y, float z)
        {
            Incogine_Object_SetRotation(obj, x, y, z);
        }

        public static Vector3 GetRotation(IntPtr obj)
        {
            Incogine_Object_GetRotation(obj, out float x, out float y, out float z);
            return new Vector3(x, y, z);
        }

        public static string GetName(IntPtr obj)
        {
            IntPtr strPtr = Incogine_Object_GetName(obj);
            return Marshal.PtrToStringAnsi(strPtr) ?? string.Empty;
        }
    }
}
