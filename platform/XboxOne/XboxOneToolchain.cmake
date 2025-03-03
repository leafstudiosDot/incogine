# XboxOneToolchain.cmake
set(CMAKE_SYSTEM_NAME XboxOne)
set(CMAKE_SYSTEM_VERSION 10.0)

# Specify the compiler
set(CMAKE_C_COMPILER "cl.exe")
set(CMAKE_CXX_COMPILER "cl.exe")

# Specify the paths to the Xbox One SDK (if available)
set(XBOXONE_SDK_PATH "C:/Path/To/XboxOneSDK")
include_directories("${XBOXONE_SDK_PATH}/include")
link_directories("${XBOXONE_SDK_PATH}/lib")

# Set the target architecture
set(CMAKE_GENERATOR_PLATFORM x64)

# Other necessary flags and settings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_XBOX_ONE")
