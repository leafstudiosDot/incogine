#include "kodohandler.h"
#include "../../objects/objects.h"
#include <iostream>

KodoScriptHandler::KodoScriptHandler(const std::string& path)
    : scriptPath(path) {
}

KodoScriptHandler::~KodoScriptHandler() {
    OnDestroy(nullptr);
}

void KodoScriptHandler::Start(Object* owner) {
    if (started) return;
    std::cerr << "[Incogine] Kodo scripting not yet implemented. Script: " << scriptPath << std::endl;
    started = true;
}

void KodoScriptHandler::Update(Object* owner) {
    if (!started) return;
    // Placeholder — no-op until Kodo interpreter is implemented
}

void KodoScriptHandler::OnDestroy(Object* owner) {
    if (!started) return;
    started = false;
}

ScriptLanguage KodoScriptHandler::GetLanguage() const {
    return ScriptLanguage::Kodo;
}

std::string KodoScriptHandler::GetLanguageName() const {
    return "Kodo";
}
