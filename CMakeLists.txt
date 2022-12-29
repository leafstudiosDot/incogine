cmake_minimum_required(VERSION 3.10)
project(Incogine)
set(CMAKE_CXX_STANDARD 11)

add_definitions("-Wall" "-g")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

INCLUDE_DIRECTORIES(./reqs/include)
LINK_DIRECTORIES(./reqs/lib)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})
find_package(SDL2_ttf REQUIRED)
include_directories(${SDL2_TTF_INCLUDE_DIRS})
find_package(SDL2_image REQUIRED)
include_directories(${SDL2_IMAGE_INCLUDE_DIRS})
find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIRS})
find_package(GLEW REQUIRED)
include_directories(${GLEW_INCLUDE_DIRS})

include_directories(./src)
file(GLOB SOURCES 
    "./src/*.cpp"
    "./src/*.h"
    "./src/*.hpp"
)

include_directories(./src/core)
file(GLOB SOURCES_CORE 
    "./src/core/*.cpp"
    "./src/core/*.h"
    "./src/core/*.hpp"
)

include_directories(./src/core/objects)
file(GLOB SOURCES_CORE_OBJECTS 
    "./src/core/objects/*.cpp"
    "./src/core/objects/*.h"
    "./src/core/objects/*.hpp"
)

include_directories(./src/core/misc)
file(GLOB SOURCES_CORE_MISC 
    "./src/core/misc/*.cpp"
    "./src/core/misc/*.h"
    "./src/core/misc/*.hpp"
)

include_directories(./src/core/misc/console)
file(GLOB SOURCES_CORE_MISC_CONSOLE 
    "./src/core/misc/console/*.cpp"
    "./src/core/misc/console/*.h"
    "./src/core/misc/console/*.hpp"
)

include_directories(./src/core/objects/components)
file(GLOB SOURCES_CORE_OBJECTS_COMPONENTS 
    "./src/core/objects/components/*.cpp"
    "./src/core/objects/components/*.h"
    "./src/core/objects/components/*.hpp"
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Main" FILES ${SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src/core PREFIX "Main/Core" FILES ${SOURCES_CORE})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src/core/misc PREFIX "Main/Core/Misc" FILES ${SOURCES_CORE_MISC})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src/core/misc/console PREFIX "Main/Core/Misc/Console" FILES ${SOURCES_CORE_MISC_CONSOLE})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src/core/objects PREFIX "Main/Core/Objects" FILES ${SOURCES_CORE_OBJECTS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src/core/objects/components PREFIX "Main/Core/Objects/Components" FILES ${SOURCES_CORE_OBJECTS_COMPONENTS})

include_directories(./src/game)
file(GLOB SOURCES_GAME
    "./src/game/*.cpp"
    "./src/game/*.h"
    "./src/game/*.hpp"
)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src/game PREFIX "Main/Game" FILES ${SOURCES_GAME})

include_directories(./src/game/scenes)
file(GLOB SOURCES_GAME_SCENES 
    "./src/game/scenes/*.cpp"
    "./src/game/scenes/*.h"
    "./src/game/scenes/*.hpp"
)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src/game PREFIX "Main/Game/Scenes" FILES ${SOURCES_GAME_SCENES})

add_executable(${PROJECT_NAME} ${SOURCES} ${SOURCES_CORE} ${SOURCES_CORE_MISC} ${SOURCES_CORE_MISC_CONSOLE} ${SOURCES_CORE_OBJECTS} ${SOURCES_CORE_OBJECTS_COMPONENTS} ${SOURCES_GAME} ${SOURCES_GAME_SCENES})

target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES} ${SDL2_TTF_LIBRARIES} ${SDL2_IMAGE_LIBRARIES} ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES})