# Kodo Syntax Reference

> **Status:** Implemented as an MVP interpreter (`src/core/scripting/kodo/`: lexer, parser, tree-walk runtime with the bindings below). Known MVP limits: `Sprite("path")` records the path without rendering it, `Audio.seek/forward/reverse/setVolume/setLoop` are accepted but ignored, `Save.load()` returns success instead of data, no physics yet, `Object.create` objects are Kodo-owned until `destroy()`.

- Dynamically typed — all variables declared with `var`
- No semicolons — newlines terminate statements (semicolons only as separators inside `for` headers)
- Comments: `//` single-line and `/* */` multi-line ( `#` is not a comment)

---

## Overview

Kodo is a custom scripting language for the Incogine game engine, designed to be lightweight, readable, and game-dev friendly. Each `.kodo` file is a script attached to an `Object` via `ScriptComponent` (like Unity's `MonoBehaviour`).

```cpp
obj->addComponent(std::make_unique<ScriptComponent>(obj, "scripts/kodo/enemy.kodo", ScriptLanguage::Kodo))
```

---

## Variables

Dynamically typed. Every variable is declared with `var`.

```
// Uses `var` keyword for variable declaration
var string = "string"
var integer = 1
var bool = true
var array = [1, 2, 3]
var object = { key: "value" }
var vector = Vector(1, 2, 3)
var color = Color(1, 0, 0, 1) // RGBA
var audio = Audio("assets/audio/testbgm.ogg")
var transform = Transform(Position(1, 2, 3), Scale(1, 1, 1), Rotation(0, 0, 0))
var fn = function(a, b) { return a + b }
```

Reassignment does not use `var`:

```
var x = 10
x = 20
```

## Functions

```
function myFunction(param1, param2) {
    // code to execute
    return result
}

// Anonymous / first-class functions
var add = function(a, b) {
    return a + b
}
```

## Lifecycle

Every Kodo script can define these lifecycle methods (matching `ScriptComponent` — `src/core/components/script/scriptcomponent.h:16`):

- `start()` — called once when the script is first activated
- `update()` — called every frame
- `onDestroy()` — called when the script or its owner Object is destroyed

```
function start() {
    log("Hello from Kodo!")
}

function update() {
    // per-frame logic
}

function onDestroy() {
    log("Goodbye", WARN)
}
```

## Accessing the Owner Object

Each script is attached to an `Object` as a `ScriptComponent`. Access the owner through `this.owner` ( `this.object` is an alias — `this.owner` is canonical).

```
// `this` is the script instance; `this.owner` is the Object it is attached to
var owner = this.owner
var position = owner.transform.position
owner.transform.position = Vector(1, 2, 3)
var scale = owner.transform.scale
owner.transform.scale = Vector(2, 2, 2)
var rotation = owner.transform.rotation
owner.transform.rotation = Vector(0, 90, 0)

// Shorthand when the script only needs its own transform
var pos = this.owner.transform.position
```

`Vector`, `Color`, `Position`, `Scale`, `Rotation`, `Transform`, and `Audio` are built-in constructors that map to `src/core/objects/objects.h:13` and `src/core/assets/audio/audio.h`.

## Control Flow

```
// If / else
if (condition) {
    // code to execute if condition is true
} else {
    // code to execute if condition is false
}

// While loop
while (condition) {
    // code to execute while condition is true
}

// For loop — semicolons are allowed only as separators inside the header
for (var i = 0; i < 10; i++) {
    // code to execute 10 times
}

// Foreach loop — `in` is contextual, not a reserved keyword (you can still use `in` as an identifier elsewhere)
var array = [1, 2, 3, 4, 5]
foreach (var item in array) {
    // code to execute for each item in the array
}

// Switch statement
switch (variable) {
    case value1:
        // code to execute if variable == value1
        break
    case value2:
        // code to execute if variable == value2
        break
    default:
        // code to execute if variable doesn't match any case
}
```

## Built-in Functions

```
// Logging — level constants are INFO (default), WARN, ERROR, DEBUG
log("message")
log("message", INFO)
log("message", WARN)
log("message", ERROR)
log("message", DEBUG)
logf("formatted message: {variable}", variable=value)
logf("formatted message: {variable}", variable=value, level=INFO)
logf("formatted message: {variable}", variable=value, level=INFO, timestamp=true)

// Math — mirrors src/core/engine/math.h
lerp(a, b, t)
cubicBezier(t, x1, y1, x2, y2)

// Random
Random.range(0, 100)        // int in [min, max)
Random.rangeFloat(0.0, 1.0) // float in [min, max)
Random.pick([1, 2, 3])      // random element from array

// Scene — both spellings work (Engine.setScene is canonical, Scene.change is alias)
Engine.setScene("GameScene")
Engine.setScene("MainScene")
Scene.change("GameScene")   // alias for Engine.setScene

// Object — find / create / destroy
var player = Object.find("Player")                // find by name, returns Object or null
var enemies = Object.findAll("Enemy")             // all Objects with name "Enemy"
var go = Object.create("Bullet", Vector(0, 0, 0)) // create new Object at position
this.owner.destroy()                              // destroy the owner Object
player.destroy()                                  // destroy another Object

// Sprite — Color OR image from assets/* (jpeg, jpg, gif, png, bmp, tiff, etc.)
var spr1 = Sprite(Color(1, 0, 0, 1))               // solid color
var spr2 = Sprite("assets/images/hero.png")        // image file
this.owner.sprite = Sprite("assets/images/hero.png")
this.owner.sprite.color = Color(1, 1, 1, 1)
this.owner.sprite.texture = "assets/images/hero.png"

// Audio — Audio(path) loads from src/assets/*; methods use parentheses
var bgm = Audio("assets/audio/testbgm.ogg")
bgm.play()
bgm.stop()
bgm.play(loop=true)           // loop playback
bgm.seek(5.0)                 // jump to 5 seconds
bgm.forward(2.0)              // skip forward 2 seconds
bgm.reverse(2.0)              // skip backward 2 seconds
bgm.setVolume(0.8)            // 0.0 .. 1.0
bgm.setLoop(true)             // enable/disable looping after creation

// Save / Load — game-defined keys in one shared store (scriptsave.dat,
// see docs/save-data.md); engine persists via src/core/engine/savedata/savedata.h
Save.set("coins", 100)
var coins = Save.get("coins")   // "" when missing
var hasCoins = Save.has("coins")
Save.remove("coins")
Save.clear()
Save.save()                     // flush to disk -> bool
var ok = Save.load()            // load from disk -> bool
```

## Types

All values are dynamically typed. Use `var` for every declaration; the right-hand side determines the type.

```
var integer = 42
var decimal = 3.14
var text = "Hello, Kodo!"
var flag = true
var numbers = [1, 2, 3, 4, 5]
var person = { name: "Alice", age: 30 }
var fn = function(a) { return a * 2 }
var nothing = null
```

`null` represents absence of value.

## Operators

```
// Arithmetic: +  -  *  /  %
// Comparison: ==  !=  <  <=  >  >=
// Logical:    &&  ||  !
// Assignment: =  +=  -=  *=  /=
// Increment/Decrement: ++  --  (both i++ and i += 1 work — per programmer style)
 // Member:     .   []  (e.g. owner.transform.position, array[0])
// String concat with + ( "a" + "b" == "ab" )
var a = 1 + 2 * 3
var b = 0
b++          // same as b += 1
b--          // same as b -= 1
var cond = (a > 5 && flag) || text == "hi"
```

## Null & Truthiness

```
// null is falsy; 0, "" and false are falsy as well; everything else truthy
if (nothing) {
    log("unreachable when nothing is null")
}
```

## Time

```
var dt = Time.deltaTime   // seconds since last frame (mirrors Engine::getDeltaTime:67)
var t = Time.time         // seconds since startup
```

## Import / Modules

```
import "scripts/kodo/utils.kodo"   // execute another script file
from "scripts/kodo/utils.kodo" import { helperFunction, CONSTANT_VALUE }
import "scripts/kodo/other.kodo" as other
import "scripts/kodo/other.kodo" as *  // import all exports from other.kodo
```

## Error Handling

```
try {
    // code that may throw an error
} catch (error) {
    log("Caught an error: {error}", error=error, level=ERROR)
}
```

## Physics / Collision

```
// TODO — physics not yet implemented in engine (src/core/objects/objects.h:97)
// Proposed shape, change as you like:
// if (this.owner.collidesWith(player)) { ... }
// this.owner.onCollision = function(other) { log("hit {other}", other=other.name) }
```
