using System;
using System.Runtime.InteropServices;

namespace Incogine
{
    /// <summary>
    /// Base class for all Incogine C# scripts. Extend this class and
    /// override Start(), Update(), and/or OnDestroy() to add behaviour.
    /// </summary>
    public abstract class ScriptBehaviour
    {
        /// <summary>
        /// Handle to the native Object that owns this script.
        /// Use Object.SetPosition(), Object.GetPosition(), etc.
        /// </summary>
        public ObjectHandle Object { get; internal set; }

        /// <summary>
        /// Called once when the script is first activated.
        /// </summary>
        public virtual void Start() { }

        /// <summary>
        /// Called every frame.
        /// </summary>
        public virtual void Update() { }

        /// <summary>
        /// Called when the script or its owner Object is destroyed.
        /// </summary>
        public virtual void OnDestroy() { }
    }
}
