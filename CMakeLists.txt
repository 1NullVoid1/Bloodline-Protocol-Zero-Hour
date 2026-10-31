cmake_minimum_required(VERSION 3.16)
project(NullVoid C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Download raylib automatically the first time you build
include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
)
FetchContent_MakeAvailable(raylib)

add_executable(game
    src/main.cpp
    src/GameData.cpp
    src/Assets.cpp
    src/Ui.cpp
    src/Common.cpp
    src/MainMenu.cpp
    src/Lobby.cpp
    src/Loadouts.cpp
    src/Workshop.cpp
)
target_link_libraries(game PRIVATE raylib)

# Copy the assets folder (fonts, and later your 3D models) next to game.exe
add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:game>/assets)
