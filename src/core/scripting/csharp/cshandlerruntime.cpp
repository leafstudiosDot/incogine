#include "cshandlerruntime.h"
#include "csharphost.h"
#include "../../objects/objects.h"
#include <iostream>
#include <filesystem>

CSriptHandler::CSriptHandler(const std::string& path)
    : scriptPath(path) {
    // Derive assembly path and type name from the script path.
    // Expected layout: scripts/csharp/MyScript.cs
    //   -> assembly: scripts/csharp/Incogine.dll (the compiled assembly)
    //   -> type: the filename without extension (e.g. "MyScript")
    std::filesystem::path p(path);
    typeName = p.stem().string();

    // The compiled assembly sits next to the script or in a known output dir
    assemblyPath = (p.parent_path() / "Incogine.dll").string();
}

CSriptHandler::~CSriptHandler() {
    OnDestroy(nullptr);
}

bool CSriptHandler::ResolveManagedFunctions() {
    CSriptHost* host = GetCSriptHost();
    if (!host || !host->IsInitialized()) return false;

    auto loadFn = host->GetLoadAssemblyFunction();
    if (!loadFn) return false;

    // Convert paths to wide strings for the .NET hosting API
    auto toWide = [](const std::string& s) -> std::wstring {
        return std::wstring(s.begin(), s.end());
    };

    std::wstring wAssembly = toWide(assemblyPath);
    std::wstring wType = toWide(typeName);
    std::wstring wStart = L"Start";
    std::wstring wUpdate = L"Update";
    std::wstring wDestroy = L"OnDestroy";

    // Load Start callback
    int rc = loadFn(
        wAssembly.c_str(),
        wType.c_str(),
        wStart.c_str(),
        nullptr,
        nullptr,
        (void**)&startFunc);

    if (rc != 0 || !startFunc) {
        std::cerr << "[Incogine] C# script: failed to resolve " << typeName << ".Start (rc=" << std::hex << rc << std::dec << ")" << std::endl;
        return false;
    }

    // Load Update callback
    rc = loadFn(
        wAssembly.c_str(),
        wType.c_str(),
        wUpdate.c_str(),
        nullptr,
        nullptr,
        (void**)&updateFunc);

    if (rc != 0 || !updateFunc) {
        std::cerr << "[Incogine] C# script: failed to resolve " << typeName << ".Update (rc=" << std::hex << rc << std::dec << ")" << std::endl;
        return false;
    }

    // Load OnDestroy callback (optional, don't fail if not found)
    loadFn(
        wAssembly.c_str(),
        wType.c_str(),
        wDestroy.c_str(),
        nullptr,
        nullptr,
        (void**)&destroyFunc);

    return true;
}

void CSriptHandler::Start(Object* owner) {
    if (started) return;

    CSriptHost* host = GetCSriptHost();
    if (!host->IsInitialized()) {
        if (!host->Initialize()) {
            std::cerr << "[Incogine] C# script: runtime not available, skipping " << scriptPath << std::endl;
            return;
        }
    }

    if (!ResolveManagedFunctions()) {
        std::cerr << "[Incogine] C# script: could not load managed functions for " << scriptPath << std::endl;
        return;
    }

    if (startFunc) {
        startFunc(owner);
    }
    started = true;
}

void CSriptHandler::Update(Object* owner) {
    if (!started || !updateFunc) return;
    updateFunc(owner);
}

void CSriptHandler::OnDestroy(Object* owner) {
    if (!started) return;
    if (destroyFunc) {
        destroyFunc(owner);
    }
    started = false;
}

ScriptLanguage CSriptHandler::GetLanguage() const {
    return ScriptLanguage::CSharp;
}

std::string CSriptHandler::GetLanguageName() const {
    return "C#";
}
