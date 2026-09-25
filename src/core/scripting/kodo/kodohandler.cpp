#include "kodohandler.h"
#include "kodointerpreter.h"
#include "kodoparser.h"
#include "../../objects/objects.h"

#include <filesystem>
#include <iostream>

KodoScriptHandler::KodoScriptHandler(const std::string& path) : scriptPath(path) {}

KodoScriptHandler::~KodoScriptHandler() {
    OnDestroy(nullptr);
}

static std::string parentDirOf(const std::string& path) {
    try {
        auto parent = std::filesystem::path(path).parent_path().string();
        return parent;
    } catch (...) {
        return "";
    }
}

void KodoScriptHandler::Start(Object* owner) {
    if (started || loadFailed) return;
    if (!parsed) {
        parser = std::make_unique<Kodo::Parser>();
        if (!parser->Load(scriptPath)) {
            std::cerr << "[Kodo] cannot load script: " << parser->error() << std::endl;
            loadFailed = true;
            return;
        }
        if (!parser->Parse()) {
            std::cerr << "[Kodo] parse error: " << parser->error() << std::endl;
            loadFailed = true;
            return;
        }
        parsed = true;
        interp = std::make_unique<Kodo::Interpreter>(parentDirOf(scriptPath), scriptPath);
        if (!interp->run(parser->program())) {
            std::cerr << "[Kodo] runtime error in " << scriptPath << ": " << interp->error()
                      << std::endl;
            loadFailed = true;
            return;
        }
    }
    started = true;
    if (!interp->callLifecycle("start", owner)) {
        std::cerr << "[Kodo] error in start() (" << scriptPath << "): " << interp->error()
                  << std::endl;
        loadFailed = true;
    }
}

void KodoScriptHandler::Update(Object* owner) {
    if (!started || loadFailed || !interp) return;
    if (!interp->callLifecycle("update", owner)) {
        std::cerr << "[Kodo] error in update() (" << scriptPath << "): " << interp->error()
                  << std::endl;
        loadFailed = true; // deterministic scripts would spam every frame otherwise
    }
}

void KodoScriptHandler::OnDestroy(Object* owner) {
    if (!started) return;
    // Owner may be mid-destruction here (Object::~Object -> destroyScripts);
    // the pointer is only wrapped, never dereferenced, unless the script
    // itself touches this.owner.*.
    if (interp && !loadFailed) {
        if (!interp->callLifecycle("onDestroy", owner)) {
            std::cerr << "[Kodo] error in onDestroy() (" << scriptPath << "): " << interp->error()
                      << std::endl;
        }
    }
    started = false;
}

ScriptLanguage KodoScriptHandler::GetLanguage() const {
    return ScriptLanguage::Kodo;
}

std::string KodoScriptHandler::GetLanguageName() const {
    return "Kodo";
}
