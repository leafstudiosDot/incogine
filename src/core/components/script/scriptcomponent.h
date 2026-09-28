#include <string>
#include <memory>
#include "../components.h"
#include "scripthandler.h"

#ifndef SCRIPTCOMPONENT_H
#define SCRIPTCOMPONENT_H

class ScriptComponent : public Component {
    private:
        std::unique_ptr<ScriptHandler> handler;
        std::string scriptPath;
        bool started = false;

    public:
        ScriptComponent(Object* owner, const std::string& path, ScriptLanguage lang);
        ~ScriptComponent() override;

        void Start() override;
        void Update() override;
        void OnDestroy() override;

        ScriptLanguage GetLanguage() const;
        const std::string& GetScriptPath() const;
        ScriptHandler* GetHandler() const;
};

#endif
