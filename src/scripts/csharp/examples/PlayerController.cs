using Incogine;

/// <summary>
/// Example C# script for Incogine.
/// Attach this to an Object to make it move across the screen.
///
/// Usage in C++:
///   auto obj = new Object("Mover", Position(0,0,0), Scale(1,1,1), Rotation(0,0,0));
///   obj->addComponent(std::make_unique&lt;ScriptComponent&gt;(
///       obj, "scripts/csharp/examples/PlayerController.cs", ScriptLanguage::CSharp));
/// </summary>
public class PlayerController : ScriptBehaviour
{
    private float speed = 100.0f;

    public override void Start()
    {
        System.Console.WriteLine("[PlayerController] Start");
        Object.SetPosition(100.0f, 200.0f, 0.0f);
    }

    public override void Update()
    {
        var pos = Object.GetPosition();
        pos.X += speed * 0.016f; // roughly 60fps delta
        Object.SetPosition(pos);
    }
}
