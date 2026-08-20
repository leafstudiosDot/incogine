#include <string>
#include "../../components/script/scripthandler.h"

#ifndef KODOHANDLER_H
#define KODOHANDLER_H

class KodoScriptHandler : public ScriptHandler {
    private:
        std::string scriptPath;
        bool started = false;

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
