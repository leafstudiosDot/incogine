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