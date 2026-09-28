#include "scriptfactory.h"
#include "../../scripting/csharp/cshandlerruntime.h"
#include "../../scripting/kodo/kodohandler.h"

std::unique_ptr<ScriptHandler> CreateScriptHandler(const std::string& path, ScriptLanguage lang) {
    switch (lang) {
        case ScriptLanguage::CSharp:
            return std::make_unique<CSriptHandler>(path);
        case ScriptLanguage::Kodo:
            return std::make_unique<KodoScriptHandler>(path);
        default:
            return nullptr;
    }
}
