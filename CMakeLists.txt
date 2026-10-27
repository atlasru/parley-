cmake_minimum_required(VERSION 3.24)
project(HAU VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
include(CTest)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Sql Test)
qt_standard_project_setup(REQUIRES 6.5)

# toxcore's source is pinned; libsodium is provided by the OS or vcpkg.
if(WIN32)
  find_package(unofficial-sodium CONFIG REQUIRED)
  set(MSVC_STATIC_SODIUM ON CACHE BOOL "" FORCE)
endif()
set(UNITTEST OFF CACHE BOOL "" FORCE)
set(AUTOTEST OFF CACHE BOOL "" FORCE)
set(BUILD_TOXAV OFF CACHE BOOL "" FORCE)
set(DHT_BOOTSTRAP OFF CACHE BOOL "" FORCE)
set(BOOTSTRAP_DAEMON OFF CACHE BOOL "" FORCE)
set(ENABLE_SHARED OFF CACHE BOOL "" FORCE)
set(ENABLE_STATIC ON CACHE BOOL "" FORCE)
include(FetchContent)
FetchContent_Declare(toxcore
    GIT_REPOSITORY https://github.com/TokTok/c-toxcore.git
    GIT_TAG v0.2.23
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(toxcore)

add_library(hau_core STATIC
    core/profile/ProfileStore.cpp core/profile/ProfileStore.h
    core/storage/Database.cpp core/storage/Database.h
    core/storage/LocalProtection.cpp core/storage/LocalProtection.h
    core/tox/ToxService.cpp core/tox/ToxService.h)
target_include_directories(hau_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${toxcore_SOURCE_DIR})
target_link_libraries(hau_core PUBLIC Qt6::Core Qt6::Sql toxcore_static)
if(WIN32)
    target_link_libraries(hau_core PRIVATE crypt32)
endif()
if(MSVC)
    target_compile_options(hau_core PRIVATE /W4)
else()
    target_compile_options(hau_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

qt_add_executable(HAU app/main.cpp app/AppViewModel.cpp app/AppViewModel.h)
set_source_files_properties(ui/qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(HAU URI HAU VERSION 1.0 QML_FILES
    ui/qml/Main.qml ui/qml/Theme.qml)
target_link_libraries(HAU PRIVATE hau_core Qt6::Gui Qt6::Quick Qt6::QuickControls2)
set_target_properties(HAU PROPERTIES WIN32_EXECUTABLE TRUE
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(BUILD_TESTING)
    qt_add_executable(hau_tests tests/TestFoundation.cpp)
    target_link_libraries(hau_tests PRIVATE hau_core Qt6::Test)
    add_test(NAME hau_foundation COMMAND hau_tests)
endif()
