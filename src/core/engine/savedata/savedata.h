// SaveData: string key-value store with file persistence.
//
// Game code and scripts share one file-backed store per filename. The
// scripting languages (C# SaveApi, Kodo Save module) both delegate to the
// shared "scriptsave" instance via SharedScriptSave(), so a value written
// from C# is visible to Kodo and vice versa.
//
// On-disk format is length-prefixed ("<klen> <key><vlen> <value>\n") so keys
// and values may contain spaces, newlines and arbitrary bytes except NUL.

#ifndef SAVEDATA_H
#define SAVEDATA_H

#include <map>
#include <string>

class SaveData {
    private:
        std::string path;
        std::map<std::string, std::string> store;

    public:
        // filename without extension; resolved under
        // SDL_GetPrefPath(PROJECT_AUTHOR, PROJECT_NAME) + ".dat".
        explicit SaveData(const std::string& filename);

        void Set(const std::string& key, const std::string& value);
        std::string Get(const std::string& key, const std::string& fallback = "") const;
        bool Has(const std::string& key) const;
        bool Remove(const std::string& key); // true when the key existed
        void Clear();

        bool Save(); // flush memory to disk
        bool Load(); // replace memory from disk (false when no file yet)

        const std::string& filePath() const { return path; }
};

// Script-facing store (file "scriptsave"). Single implementation behind
// Incogine_Save_* (C#) and the Kodo Save module.
SaveData& SharedScriptSave();

#endif
