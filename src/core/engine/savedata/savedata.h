#include <iostream>
#include <fstream>
#include <string>
#include <SDL2/SDL.h>
using namespace std;

#ifdef _WIN32
    #include <direct.h>
    #else
    #include <sys/stat.h>
    #include <sys/types.h>
#endif

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

            #ifdef _WIN32
                if (_mkdir(_path) == 0) {
                    return true;
                } else if (errno == EEXIST) {
                    return directoryExists();
                } else {
                    return false;
                }
            #else
                if (mkdir(_path, 0755) == 0) {
                    return true;
                } else if (errno == EEXIST) {
                    return directoryExists();
                } else {
                    return false;
                }
            #endif
        }
    public:
        SaveData(string filename);

        bool Save(const string &data);
        bool Load();
};