#include <memory>
#include <string>
#include "../../components/script/scripthandler.h"

#ifndef KODOHANDLER_H
#define KODOHANDLER_H

namespace Kodo {
class Interpreter;
class Parser;
}

class KodoScriptHandler : public ScriptHandler {
    private:
        std::string scriptPath;
        bool started = false;
        bool loadFailed = false;
        bool parsed = false;
        std::unique_ptr<Kodo::Parser> parser;
        std::unique_ptr<Kodo::Interpreter> interp;

    public:
        explicit KodoScriptHandler(const std::string& path);
        ~KodoScriptHandler() override;

        void Start(Object* owner) override;
        void Update(Object* owner) override;
        void OnDestroy(Object* owner) override;
        ScriptLanguage GetLanguage() const override;
        std::string GetLanguageName() const override;
};

#endif
