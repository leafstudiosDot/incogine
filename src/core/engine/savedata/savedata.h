#include <iostream>
#include <fstream>
#include <string>
#include <SDL2/SDL.h>
#include <sys/stat.h>
#include <sys/types.h>
using namespace std;

class SaveData {
    private:
        string path;

        bool directoryExists() {
            char* _path = SDL_GetPrefPath(PROJECT_AUTHOR, PROJECT_NAME);
            struct stat info;
            if (stat(_path, &info) != 0) {
                return false;
            } else if (info.st_mode & S_IFDIR) {
                return true;
            }
            return false;
        }

        bool createDirectory() {
            char* _path = SDL_GetPrefPath(PROJECT_AUTHOR, PROJECT_NAME);

            if (mkdir(_path, 0755) == 0) {
                return true;
            } else if (errno == EEXIST) {
                return directoryExists();
            } else {
                return false;
            }
        }
    public:
        SaveData(string filename);

        bool Save(const string &data);
        bool Load();
};