#include "savedata.h"

SaveData::SaveData(string filename) {
    char* _path = SDL_GetPrefPath(PROJECT_AUTHOR, PROJECT_NAME);
    if (_path) {
        path = std::string(_path) + filename + ".dat";
        SDL_free(_path);
    } else {
        std::cerr << "Error getting pref path: " << SDL_GetError() << std::endl;
        path = "";
    }
}

bool SaveData::Save(const string &data) {
    return false;
}

bool SaveData::Load() {
    return false;
}