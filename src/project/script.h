// Redirect: the Script class has been replaced by the scripting system
// in src/core/components/script/ and src/core/scripting/.
//
// Use ScriptComponent instead:
//   #include "core/components/script/scriptcomponent.h"
//   obj->addComponent(std::make_unique<ScriptComponent>(obj, "scripts/csharp/MyScript.cs", ScriptLanguage::CSharp));
//
// See docs/scripting.md for the full scripting guide.

#ifndef PRSCRIPT_H
#define PRSCRIPT_H

#include "../core/components/script/scriptcomponent.h"
#include "../core/components/script/scripthandler.h"

#endif
