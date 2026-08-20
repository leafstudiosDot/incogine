#include <string>

#ifndef SCRIPTHANDLER_H
#define SCRIPTHANDLER_H

class Object;

enum class ScriptLanguage {
    CSharp,
    Kodo
};

class ScriptHandler {
    public:
        virtual ~ScriptHandler() = default;

        virtual void Start(Object* owner) = 0;
        virtual void Update(Object* owner) = 0;
        virtual void OnDestroy(Object* owner) = 0;
        virtual ScriptLanguage GetLanguage() const = 0;
        virtual std::string GetLanguageName() const = 0;
};

#endif
