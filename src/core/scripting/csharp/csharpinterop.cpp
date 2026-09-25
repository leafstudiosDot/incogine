#include "csharpinterop.h"
#include "../../objects/objects.h"
#include "../../objects/square/square.h"
#include "../../components/sprite/sprite.h"
#include "../../engine/engine.h"
#include "../../engine/savedata/savedata.h"
#include "../../assets/audio/audio.h"
#include "../../../scenes/MainScene.h"
#include "../../../scenes/game/GameScene.h"
#include "../../../scenes/splash/splash.h"
#include "../../../scenes/settings/SettingsScene.h"
#include "../../../scenes/credits/Credits.h"

#include <SDL3/SDL.h>
#include <cstring>
#include <string>
#include <cctype>
#include <algorithm>

namespace {

inline Object* toObj(void* p) { return static_cast<Object*>(p); }

int clampColor(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

// Stable storage for returned strings (c_str of a temporary is UB).
thread_local std::string tlName;
thread_local std::string tlSaveValue;

void applyColor(Object* obj, const Color& c) {
    if (!obj) return;
    if (auto* sq = dynamic_cast<Square*>(obj)) {
        sq->setColor(c);
        return;
    }
    if (auto* comp = obj->getComponentByName("Sprite")) {
        if (auto* spr = dynamic_cast<Sprite*>(comp)) spr->setColor(c);
    }
}

Color readColor(Object* obj) {
    if (obj) {
        if (auto* sq = dynamic_cast<Square*>(obj)) return sq->getColor();
        if (auto* comp = obj->getComponentByName("Sprite")) {
            if (auto* spr = dynamic_cast<Sprite*>(comp)) return spr->getColor();
        }
    }
    return Color(255, 255, 255, 255);
}

// --- Save store: single implementation in SaveData (see savedata.h) ---
// (Get() needs stable storage: c_str of a temporary is UB; tlSaveValue above.)

// --- Input name mapping ---
SDL_GamepadButton gamepadButtonFromName(const char* name) {
    if (!name) return SDL_GAMEPAD_BUTTON_INVALID;
    std::string n(name);
    for (auto& c : n) c = (char)std::tolower((unsigned char)c);
    if (n == "a") return SDL_GAMEPAD_BUTTON_A;
    if (n == "b") return SDL_GAMEPAD_BUTTON_B;
    if (n == "x") return SDL_GAMEPAD_BUTTON_X;
    if (n == "y") return SDL_GAMEPAD_BUTTON_Y;
    if (n == "back") return SDL_GAMEPAD_BUTTON_BACK;
    if (n == "guide") return SDL_GAMEPAD_BUTTON_GUIDE;
    if (n == "start") return SDL_GAMEPAD_BUTTON_START;
    if (n == "leftstick" || n == "left_stick") return SDL_GAMEPAD_BUTTON_LEFT_STICK;
    if (n == "rightstick" || n == "right_stick") return SDL_GAMEPAD_BUTTON_RIGHT_STICK;
    if (n == "leftshoulder" || n == "left_shoulder") return SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    if (n == "rightshoulder" || n == "right_shoulder") return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    if (n == "dpadup" || n == "dpup" || n == "dpad_up") return SDL_GAMEPAD_BUTTON_DPAD_UP;
    if (n == "dpaddown" || n == "dpdown" || n == "dpad_down") return SDL_GAMEPAD_BUTTON_DPAD_DOWN;
    if (n == "dpadleft" || n == "dpleft" || n == "dpad_left") return SDL_GAMEPAD_BUTTON_DPAD_LEFT;
    if (n == "dpadright" || n == "dpright" || n == "dpad_right") return SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    return SDL_GAMEPAD_BUTTON_INVALID;
}

SDL_GamepadAxis gamepadAxisFromName(const char* name, bool& ok) {
    ok = true;
    if (!name) { ok = false; return SDL_GAMEPAD_AXIS_INVALID; }
    std::string n(name);
    for (auto& c : n) c = (char)std::tolower((unsigned char)c);
    if (n == "leftstickx" || n == "left_stick_x") return SDL_GAMEPAD_AXIS_LEFTX;
    if (n == "leftsticky" || n == "left_stick_y") return SDL_GAMEPAD_AXIS_LEFTY;
    if (n == "rightstickx" || n == "right_stick_x") return SDL_GAMEPAD_AXIS_RIGHTX;
    if (n == "rightsticky" || n == "right_stick_y") return SDL_GAMEPAD_AXIS_RIGHTY;
    if (n == "lefttrigger" || n == "left_trigger") return SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
    if (n == "righttrigger" || n == "right_trigger") return SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
    ok = false;
    return SDL_GAMEPAD_AXIS_INVALID;
}

SDL_Gamepad* openGamepadByPlayerIndex(int playerIndex) {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (!ids || playerIndex < 0 || playerIndex >= count) return nullptr;
    SDL_Gamepad* pad = SDL_OpenGamepad(ids[playerIndex]);
    SDL_free(ids);
    return pad;
}

} // namespace

// --- Object transform ---

ICG_API void Incogine_Object_SetPosition(void* obj, float x, float y, float z) {
    if (Object* o = toObj(obj)) o->setPosition(Position((double)x, (double)y, (double)z));
}

ICG_API void Incogine_Object_GetPosition(void* obj, float* x, float* y, float* z) {
    Position p;
    if (Object* o = toObj(obj)) p = o->getPosition();
    if (x) *x = (float)p.x;
    if (y) *y = (float)p.y;
    if (z) *z = (float)p.z;
}

ICG_API void Incogine_Object_SetScale(void* obj, float x, float y, float z) {
    if (Object* o = toObj(obj)) o->setScale(Scale((double)x, (double)y, (double)z));
}

ICG_API void Incogine_Object_GetScale(void* obj, float* x, float* y, float* z) {
    Scale s(1.0, 1.0, 1.0);
    if (Object* o = toObj(obj)) s = o->getScale();
    if (x) *x = (float)s.x;
    if (y) *y = (float)s.y;
    if (z) *z = (float)s.z;
}

ICG_API void Incogine_Object_SetRotation(void* obj, float x, float y, float z) {
    if (Object* o = toObj(obj)) o->setRotation(Rotation((double)x, (double)y, (double)z));
}

ICG_API void Incogine_Object_GetRotation(void* obj, float* x, float* y, float* z) {
    Rotation r(0.0, 0.0, 0.0);
    if (Object* o = toObj(obj)) r = o->getRotation();
    if (x) *x = (float)r.x;
    if (y) *y = (float)r.y;
    if (z) *z = (float)r.z;
}

ICG_API const char* Incogine_Object_GetName(void* obj) {
    tlName.clear();
    if (Object* o = toObj(obj)) tlName = o->getName();
    return tlName.c_str();
}

// --- Object extras ---

ICG_API void Incogine_Object_SetName(void* obj, const char* name) {
    if (Object* o = toObj(obj); o && name) o->setName(name);
}

ICG_API void Incogine_Object_SetColor(void* obj, int r, int g, int b, int a) {
    applyColor(toObj(obj), Color(clampColor(r), clampColor(g), clampColor(b), clampColor(a)));
}

ICG_API void Incogine_Object_GetColor(void* obj, int* r, int* g, int* b, int* a) {
    Color c = readColor(toObj(obj));
    if (r) *r = c.r;
    if (g) *g = c.g;
    if (b) *b = c.b;
    if (a) *a = c.a;
}

// --- Object registry / lifetime ---

ICG_API void* Incogine_Object_Find(const char* name) {
    if (!name) return nullptr;
    return static_cast<void*>(Object::FindByName(name));
}

ICG_API void* Incogine_Object_Create(const char* name, float x, float y, float z) {
    Object* o = new Object(name ? name : "Object",
        Position((double)x, (double)y, (double)z), Scale(1.0, 1.0, 1.0), Rotation(0.0, 0.0, 0.0));
    return static_cast<void*>(o);
}

ICG_API void Incogine_Object_Destroy(void* obj) {
    delete toObj(obj);
}

// --- Time (engine tracks milliseconds; scripts use seconds) ---

ICG_API double Incogine_Time_GetDeltaTime() {
    return Engine::Instance(0, nullptr)->getDeltaTime() / 1000.0;
}

ICG_API double Incogine_Time_GetTime() {
    return (double)SDL_GetTicks() / 1000.0;
}

ICG_API float Incogine_Time_GetFPS() {
    return Engine::Instance(0, nullptr)->getfps();
}

// --- Log ---

ICG_API void Incogine_Log(int level, const char* msg) {
    const char* m = msg ? msg : "";
    switch (level) {
        case 1: std::cerr << "[WARN] " << m << std::endl; break;
        case 2: std::cerr << "[ERROR] " << m << std::endl; break;
        case 3: SDL_Log("[DEBUG] %s", m); break;
        default: std::cout << "[INFO] " << m << std::endl; break;
    }
}

// --- Input ---

ICG_API int Incogine_Input_IsKeyDown(const char* keyName) {
    if (!keyName || !keyName[0]) return 0;
    SDL_Scancode sc = SDL_GetScancodeFromName(keyName);
    if (sc == SDL_SCANCODE_UNKNOWN) return 0;
    int count = 0;
    const bool* state = SDL_GetKeyboardState(&count);
    if (!state || (int)sc >= count) return 0;
    return state[sc] ? 1 : 0;
}

ICG_API void Incogine_Input_GetMousePosition(float* x, float* y) {
    float mx = 0.0f, my = 0.0f;
    SDL_GetMouseState(&mx, &my);
    if (x) *x = mx;
    if (y) *y = my;
}

ICG_API int Incogine_Input_IsMouseButtonDown(int button) {
    SDL_MouseButtonFlags flags = SDL_GetMouseState(nullptr, nullptr);
    Uint32 mask = 0;
    switch (button) {
        case 1: mask = SDL_BUTTON_LMASK; break;
        case 2: mask = SDL_BUTTON_MMASK; break;
        case 3: mask = SDL_BUTTON_RMASK; break;
        default: return 0;
    }
    return (flags & mask) ? 1 : 0;
}

ICG_API int Incogine_Input_IsGamepadButtonDown(int playerIndex, const char* buttonName) {
    SDL_GamepadButton btn = gamepadButtonFromName(buttonName);
    if (btn == SDL_GAMEPAD_BUTTON_INVALID) return 0;
    SDL_Gamepad* pad = openGamepadByPlayerIndex(playerIndex);
    if (!pad) return 0;
    int down = SDL_GetGamepadButton(pad, btn) ? 1 : 0;
    SDL_CloseGamepad(pad);
    return down;
}

ICG_API float Incogine_Input_GetGamepadAxis(int playerIndex, const char* axisName) {
    bool ok = false;
    SDL_GamepadAxis axis = gamepadAxisFromName(axisName, ok);
    if (!ok) return 0.0f;
    SDL_Gamepad* pad = openGamepadByPlayerIndex(playerIndex);
    if (!pad) return 0.0f;
    Sint16 v = SDL_GetGamepadAxis(pad, axis);
    SDL_CloseGamepad(pad);
    if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
        return (float)v / 32767.0f; // 0..1
    return (float)v / 32768.0f; // -1..1
}

// --- Scene ---

ICG_API void Incogine_Scene_SetByName(const char* name) {
    if (!name) return;
    std::string n(name);
    for (auto& c : n) c = (char)std::tolower((unsigned char)c);
    Scene* scene = nullptr;
    if (n == "mainscene" || n == "main") scene = new MainScene();
    else if (n == "gamescene" || n == "game") scene = new GameScene();
    else if (n == "splash") scene = new Splash();
    else if (n == "settings" || n == "settingsscene") scene = new SettingsScene();
    else if (n == "credits" || n == "creditsscene") scene = new CreditsScene();
    else {
        std::cerr << "[Incogine] Incogine_Scene_SetByName: unknown scene '" << name << "'" << std::endl;
        return;
    }
    Engine::Instance(0, nullptr)->SetScene(scene);
}

// --- Audio ---

ICG_API void* Incogine_Audio_Load(const char* path) {
    if (!path || !path[0]) return nullptr;
    Audio* a = new Audio(path);
    return static_cast<void*>(a);
}

ICG_API void Incogine_Audio_Play(void* handle, int loop) {
    if (Audio* a = static_cast<Audio*>(handle)) a->play(loop);
}

ICG_API void Incogine_Audio_Stop(void* handle) {
    if (Audio* a = static_cast<Audio*>(handle)) a->stop();
}

ICG_API void Incogine_Audio_Free(void* handle) {
    delete static_cast<Audio*>(handle);
}

// --- Save ---

ICG_API void Incogine_Save_Set(const char* key, const char* value) {
    if (!key) return;
    SharedScriptSave().Set(key, value ? value : "");
}

ICG_API const char* Incogine_Save_Get(const char* key) {
    tlSaveValue = key ? SharedScriptSave().Get(key) : "";
    return tlSaveValue.c_str();
}

ICG_API int Incogine_Save_Has(const char* key) {
    return (key && SharedScriptSave().Has(key)) ? 1 : 0;
}

ICG_API int Incogine_Save_Remove(const char* key) {
    return (key && SharedScriptSave().Remove(key)) ? 1 : 0;
}

ICG_API void Incogine_Save_Clear() {
    SharedScriptSave().Clear();
}

ICG_API int Incogine_Save_Save() {
    return SharedScriptSave().Save() ? 1 : 0;
}

ICG_API int Incogine_Save_Load() {
    return SharedScriptSave().Load() ? 1 : 0;
}
