# Incogine
A leafstudiosDot engine. Now open source!

## Dependencies
- CMake
- Python3 (Python 3.11+)
- C++17
- SDL2
- SDL2_image
- SDL2_ttf

## Get Started

## Build
> Recommended using Visual Studio Code with CMake Tools extension.

### Windows
[Coming soon]

### Windows (Terminal compiler), macOS/Linux or others
For Terminal compilers, run the following commands in the project root directory:
```
$ mkdir build
$ cd build
$ cmake ..
$ make
```

### WebAssembly

Before you build Incogine for WebAssembly for the first time, run the following command:
```
$ cd emsdk
$ ./emsdk update (or git pull if you have local changes within emsdk directory)
$ ./emsdk install latest
$ ./emsdk activate latest
$ source ./emsdk_env.sh
```

To build Incogine for WebAssembly after you do above, run the following command:
```
$ mkdir build
$ cd build
$ emcmake cmake ..
$ emmake make
```


## Notes
### Can I use the space at <name> in project.xml like `My First Game`?
Make sure the name key in project.xml has no spaces like `<name>Incogine</name>`, not `<name>Incogine Engine</name>` so it means you should name that like `<name>MyFirstGame</name>`.