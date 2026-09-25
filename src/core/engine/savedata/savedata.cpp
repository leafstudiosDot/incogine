#include "savedata.h"

#include <fstream>
#include <iostream>
#include <SDL3/SDL.h>

SaveData::SaveData(const std::string& filename) {
    char* pref = SDL_GetPrefPath(PROJECT_AUTHOR, PROJECT_NAME);
    if (pref) {
        path = std::string(pref) + filename + ".dat";
        SDL_free(pref);
    } else {
        std::cerr << "Error getting pref path: " << SDL_GetError() << std::endl;
        path = filename + ".dat"; // cwd fallback (dev / pref-path failure)
    }
}

void SaveData::Set(const std::string& key, const std::string& value) {
    store[key] = value;
}

std::string SaveData::Get(const std::string& key, const std::string& fallback) const {
    auto it = store.find(key);
    return it != store.end() ? it->second : fallback;
}

bool SaveData::Has(const std::string& key) const {
    return store.find(key) != store.end();
}

bool SaveData::Remove(const std::string& key) {
    return store.erase(key) > 0;
}

void SaveData::Clear() {
    store.clear();
}

bool SaveData::Save() {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    for (const auto& kv : store)
        out << kv.first.size() << ' ' << kv.first << kv.second.size() << ' ' << kv.second << '\n';
    out.flush();
    return (bool)out;
}

bool SaveData::Load() {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::map<std::string, std::string> loaded;
    while (in) {
        size_t kl = 0, vl = 0;
        if (!(in >> kl)) break;
        in.get();
        std::string k(kl, '\0');
        if (kl && !in.read(k.data(), (std::streamsize)kl)) break;
        if (!(in >> vl)) break;
        in.get();
        std::string v(vl, '\0');
        if (vl && !in.read(v.data(), (std::streamsize)vl)) break;
        std::string nl;
        std::getline(in, nl);
        loaded[std::move(k)] = std::move(v);
    }
    store = std::move(loaded);
    return true;
}

SaveData& SharedScriptSave() {
    static SaveData s("scriptsave");
    return s;
}
