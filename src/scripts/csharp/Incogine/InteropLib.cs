using System;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Incogine
{
    /// <summary>
    /// Resolves DllImport("Incogine") to the host executable.
    /// The engine exports its C ABI (csharpinterop.h) from the game
    /// executable itself (ENABLE_EXPORTS), not from a separate native DLL,
    /// so the managed assembly must look up "Incogine" in the main program.
    /// </summary>
    internal static class InteropLib
    {
        public const string LibraryName = "Incogine";

        [ModuleInitializer]
        internal static void Resolve()
        {
            NativeLibrary.SetDllImportResolver(
                typeof(InteropLib).Assembly,
                (string name, Assembly assembly, DllImportSearchPath? searchPath) =>
                {
                    if (name == LibraryName)
                    {
                        try { return NativeLibrary.GetMainProgramHandle(); }
                        catch { return IntPtr.Zero; }
                    }
                    return IntPtr.Zero;
                });
        }
    }
}
