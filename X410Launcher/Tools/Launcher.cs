using MemoryModule;
using System;
using System.Diagnostics;
using System.Reflection;
using System.Reflection.Metadata;
using System.Runtime.InteropServices;

namespace X410Launcher.Tools;

public static class Launcher
{
    private delegate bool StartProcessPreloadedDelegate(
        [MarshalAs(UnmanagedType.LPWStr)] string applicationName,
        [MarshalAs(UnmanagedType.LPWStr)] string dllName
    );

    private static readonly StartProcessPreloadedDelegate? StartProcessPreloaded;
#if LAUNCHER_FROM_MEMORY
    static Launcher()
    {
        using var launcherStream = Assembly.GetExecutingAssembly().GetManifestResourceStream(
            $"X410Launcher.Native.Launcher.{RuntimeInformation.ProcessArchitecture}.dll"
        );

        if (launcherStream is not null)
        {
            var _launcher = NativeAssembly.Load(launcherStream, "Launcher.dll");
            if (_launcher is not null)
            {
                StartProcessPreloaded =
                    _launcher.GetDelegate<StartProcessPreloadedDelegate>("StartProcessPreloadedW");
            }
        }
    }
#else
    static Launcher()
    {
        var dll = $"Launcher.{RuntimeInformation.ProcessArchitecture}.dll";
        var launcher = NativeLibrary.Load(dll);
        Debug.Assert(launcher != IntPtr.Zero);
        IntPtr procAddress = NativeLibrary.GetExport(launcher, "StartProcessPreloadedW");
        StartProcessPreloaded = Marshal.GetDelegateForFunctionPointer<StartProcessPreloadedDelegate>(procAddress);
        Debug.Assert(StartProcessPreloaded != null);
    }
#endif

    public static void Launch(string path)
    {
        if (StartProcessPreloaded is not null)
        {
            if (StartProcessPreloaded(path, Paths.GetHelperDllFile()))
            {
                return;
            }
        }

        Process.Start(path);
    }
}