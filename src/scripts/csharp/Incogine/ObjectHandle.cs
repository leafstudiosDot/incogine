using System;
using System.Runtime.InteropServices;

namespace Incogine
{
    /// <summary>
    /// Managed handle to a native Incogine Object. Provides access to
    /// position, scale, rotation, and other properties via P/Invoke
    /// into the engine.
    /// </summary>
    public class ObjectHandle
    {
        private IntPtr nativePtr;

        internal ObjectHandle(IntPtr ptr)
        {
            nativePtr = ptr;
        }

        public void SetPosition(float x, float y, float z)
        {
            ObjectInterop.SetPosition(nativePtr, x, y, z);
        }

        public void SetPosition(Vector3 pos)
        {
            ObjectInterop.SetPosition(nativePtr, pos.X, pos.Y, pos.Z);
        }

        public Vector3 GetPosition()
        {
            return ObjectInterop.GetPosition(nativePtr);
        }

        public void SetScale(float x, float y, float z)
        {
            ObjectInterop.SetScale(nativePtr, x, y, z);
        }

        public Vector3 GetScale()
        {
            return ObjectInterop.GetScale(nativePtr);
        }

        public void SetRotation(float x, float y, float z)
        {
            ObjectInterop.SetRotation(nativePtr, x, y, z);
        }

        public Vector3 GetRotation()
        {
            return ObjectInterop.GetRotation(nativePtr);
        }

        public string GetName()
        {
            return ObjectInterop.GetName(nativePtr);
        }
    }

    /// <summary>
    /// Simple 3-component vector used by the scripting API.
    /// </summary>
    public struct Vector3
    {
        public float X, Y, Z;

        public Vector3(float x, float y, float z)
        {
            X = x;
            Y = y;
            Z = z;
        }
    }
}
