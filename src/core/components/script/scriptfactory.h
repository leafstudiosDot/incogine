#include <string>
#include <memory>
#include "scripthandler.h"

#ifndef SCRIPTFACTORY_H
#define SCRIPTFACTORY_H

std::unique_ptr<ScriptHandler> CreateScriptHandler(const std::string& path, ScriptLanguage lang);

#endif
