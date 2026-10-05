#include "scriptcomponent.h"
#include "scriptfactory.h"
#include "../../objects/objects.h"
#include <iostream>

ScriptComponent::ScriptComponent(Object* owner, const std::string& path, ScriptLanguage lang)
    : Component(owner), scriptPath(path) {
    setComponentName("Script");
    handler = CreateScriptHandler(path, lang);
}

ScriptComponent::~ScriptComponent() {
    OnDestroy();
}

void ScriptComponent::Start() {
    if (!started && handler) {
        handler->Start(getLinkedObject());
        started = true;
    }
}

void ScriptComponent::Update() {
    if (handler) {
        handler->Update(getLinkedObject());
    }
}

void ScriptComponent::OnDestroy() {
    if (handler) {
        handler->OnDestroy(getLinkedObject());
        started = false;
    }
}

ScriptLanguage ScriptComponent::GetLanguage() const {
    return handler ? handler->GetLanguage() : ScriptLanguage::CSharp;
}

const std::string& ScriptComponent::GetScriptPath() const {
    return scriptPath;
}

ScriptHandler* ScriptComponent::GetHandler() const {
    return handler.get();
}
