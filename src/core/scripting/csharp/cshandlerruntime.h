#include <string>
#include "../../components/script/scripthandler.h"

#ifndef CSHANDLERRUNTIME_H
#define CSHANDLERRUNTIME_H

class CSriptHandler : public ScriptHandler {
    private:
        std::string scriptPath;
        std::string assemblyPath;
        std::string typeName;
        bool started = false;

        // Function pointers to managed callbacks
        using managed_start_fn = void(*)(void*);
        using managed_update_fn = void(*)(void*);
        using managed_destroy_fn = void(*)(void*);

        managed_start_fn startFunc = nullptr;
        managed_update_fn updateFunc = nullptr;
        managed_destroy_fn destroyFunc = nullptr;

        bool ResolveManagedFunctions();

    public:
        explicit CSriptHandler(const std::string& path);
        ~CSriptHandler() override;

        void Start(Object* owner) override;
        void Update(Object* owner) override;
        void OnDestroy(Object* owner) override;
        ScriptLanguage GetLanguage() const override;
        std::string GetLanguageName() const override;
};

#endif
