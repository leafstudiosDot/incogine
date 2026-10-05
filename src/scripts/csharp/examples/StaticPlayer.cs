using System;
using System.Runtime.InteropServices;
using Incogine;

/// <summary>
/// Static C# script example for Incogine. The C++ CSriptHandler resolves
/// static Start/Update/OnDestroy(UnmanagedCallersOnly) by type name
/// (file stem "StaticPlayer" -> type "StaticPlayer"), passing the owner
/// Object as IntPtr. Use ObjectInterop/ObjectApi/TimeApi/... to talk back.
///
/// Attach in C++:
///   auto obj = new Object("Mover", Position(0,0,0), Scale(1,1,1), Rotation(0,0,0));
///   obj->addComponent(std::make_unique&lt;ScriptComponent&gt;(
///       obj, "scripts/csharp/examples/StaticPlayer.cs", ScriptLanguage::CSharp));
/// </summary>
public static class StaticPlayer
{
    private static float speed = 100.0f;

    [UnmanagedCallersOnly]
    public static void Start(IntPtr owner)
    {
        ObjectInterop.SetPosition(owner, 100.0f, 200.0f, 0.0f);
        LogApi.Info("[StaticPlayer] Start");
    }

    [UnmanagedCallersOnly]
    public static void Update(IntPtr owner)
    {
        float dt = (float)TimeApi.DeltaTime();

        if (InputApi.IsKeyDown("A"))
        {
            var pos = ObjectInterop.GetPosition(owner);
            pos.X -= speed * dt;
            ObjectInterop.SetPosition(owner, pos.X, pos.Y, pos.Z);
        }
        if (InputApi.IsKeyDown("D"))
        {
            var pos = ObjectInterop.GetPosition(owner);
            pos.X += speed * dt;
            ObjectInterop.SetPosition(owner, pos.X, pos.Y, pos.Z);
        }
    }

    [UnmanagedCallersOnly]
    public static void OnDestroy(IntPtr owner)
    {
        LogApi.Info("[StaticPlayer] OnDestroy");
    }
}
