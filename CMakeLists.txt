cmake_minimum_required(VERSION 3.22)

project(Vyra VERSION 0.2.0 LANGUAGES C CXX)

include(CTest)
include(FetchContent)

if(APPLE)
    enable_language(OBJC)
endif()

option(VYRA_USE_SYSTEM_JUCE "Use an installed JUCE CMake package" OFF)

if(VYRA_USE_SYSTEM_JUCE)
    find_package(JUCE CONFIG REQUIRED)
else()
    set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
    set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 7.0.12
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

FetchContent_Declare(
    sqlite_amalgamation
    URL https://www.sqlite.org/2025/sqlite-amalgamation-3490100.zip
    URL_HASH SHA256=6CEBD1D8403FC58C30E93939B246F3E6E58D0765A5CD50546F16C00FD805D2C3
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_GetProperties(sqlite_amalgamation)
if(NOT sqlite_amalgamation_POPULATED)
    FetchContent_Populate(sqlite_amalgamation)
endif()

FetchContent_Declare(
    discord_rpc_sdk
    URL https://github.com/discord/discord-rpc/archive/refs/tags/v3.4.0.zip
    URL_HASH SHA256=EE39D9F7D756D7124FBBABD237A8D414EB461E873FD939562AB278582B918BDD
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_GetProperties(discord_rpc_sdk)
if(NOT discord_rpc_sdk_POPULATED)
    FetchContent_Populate(discord_rpc_sdk)
endif()

FetchContent_Declare(
    rapidjson
    URL https://github.com/Tencent/rapidjson/archive/refs/tags/v1.1.0.zip
    URL_HASH SHA256=8E00C38829D6785A2DFB951BB87C6974FA07DFE488AA5B25DEEC4B8BC0F6A3AB
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_GetProperties(rapidjson)
if(NOT rapidjson_POPULATED)
    FetchContent_Populate(rapidjson)
endif()

add_library(vyra_sqlite STATIC "${sqlite_amalgamation_SOURCE_DIR}/sqlite3.c")
target_include_directories(vyra_sqlite PUBLIC "${sqlite_amalgamation_SOURCE_DIR}")
target_compile_definitions(vyra_sqlite PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_OMIT_LOAD_EXTENSION=1
    SQLITE_DQS=0
)

set(DISCORD_RPC_SOURCE_DIR "${discord_rpc_sdk_SOURCE_DIR}/src")
add_library(vyra_discord_rpc STATIC
    "${discord_rpc_sdk_SOURCE_DIR}/include/discord_rpc.h"
    "${DISCORD_RPC_SOURCE_DIR}/discord_rpc.cpp"
    "${DISCORD_RPC_SOURCE_DIR}/rpc_connection.cpp"
    "${DISCORD_RPC_SOURCE_DIR}/serialization.cpp"
)
target_include_directories(vyra_discord_rpc
    PUBLIC "${discord_rpc_sdk_SOURCE_DIR}/include"
    PRIVATE "${DISCORD_RPC_SOURCE_DIR}" "${rapidjson_SOURCE_DIR}/include"
)
target_compile_features(vyra_discord_rpc PRIVATE cxx_std_14)
set_target_properties(vyra_discord_rpc PROPERTIES CXX_EXTENSIONS OFF)
if(WIN32)
    target_sources(vyra_discord_rpc PRIVATE
        "${DISCORD_RPC_SOURCE_DIR}/connection_win.cpp"
        "${DISCORD_RPC_SOURCE_DIR}/discord_register_win.cpp"
    )
    target_compile_definitions(vyra_discord_rpc PRIVATE DISCORD_WINDOWS)
    target_link_libraries(vyra_discord_rpc PRIVATE psapi advapi32)
elseif(APPLE)
    target_sources(vyra_discord_rpc PRIVATE
        "${DISCORD_RPC_SOURCE_DIR}/connection_unix.cpp"
        "${DISCORD_RPC_SOURCE_DIR}/discord_register_osx.m"
    )
    target_compile_definitions(vyra_discord_rpc PRIVATE DISCORD_OSX)
    target_link_libraries(vyra_discord_rpc PRIVATE "-framework AppKit")
else()
    target_sources(vyra_discord_rpc PRIVATE
        "${DISCORD_RPC_SOURCE_DIR}/connection_unix.cpp"
        "${DISCORD_RPC_SOURCE_DIR}/discord_register_linux.cpp"
    )
    target_compile_definitions(vyra_discord_rpc PRIVATE DISCORD_LINUX)
    target_link_libraries(vyra_discord_rpc PUBLIC pthread)
endif()

juce_add_gui_app(Vyra
    PRODUCT_NAME "Vyra"
    COMPANY_NAME "Vyra"
    ICON_BIG app/resources/vyra-icon-large.png
    ICON_SMALL app/resources/vyra-icon-small.png
)

target_sources(Vyra PRIVATE
    app/src/App.cpp
    app/src/audio/AudioEngine.cpp
    app/src/audio/BpmAnalysis.cpp
    app/src/audio/KeyAnalysis.cpp
    app/src/audio/LoopRegionAudioSource.cpp
    app/src/audio/Waveform.cpp
    app/src/library/TrackLibrary.cpp
    app/src/integrations/DiscordPresence.cpp
    app/src/integrations/MidiController.cpp
    app/src/ui/MainComponent.cpp
)

target_include_directories(Vyra PRIVATE app/include)
target_compile_features(Vyra PRIVATE cxx_std_20)
target_compile_definitions(Vyra PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)
target_link_libraries(Vyra PRIVATE
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_data_structures
    juce::juce_gui_extra
    vyra_discord_rpc
    vyra_sqlite
)

if(BUILD_TESTING)
    add_executable(vyra_audio_math_tests tests/audio/MixMathTests.cpp)
    target_include_directories(vyra_audio_math_tests PRIVATE app/include)
    target_compile_features(vyra_audio_math_tests PRIVATE cxx_std_20)
    add_test(NAME vyra_audio_math_tests COMMAND vyra_audio_math_tests)

    add_executable(vyra_loop_region_tests
        tests/audio/LoopRegionTests.cpp
        app/src/audio/LoopRegionAudioSource.cpp
    )
    target_include_directories(vyra_loop_region_tests PRIVATE app/include)
    target_compile_features(vyra_loop_region_tests PRIVATE cxx_std_20)
    target_compile_definitions(vyra_loop_region_tests PRIVATE
        JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        JUCE_USE_CURL=0
    )
    target_link_libraries(vyra_loop_region_tests PRIVATE
        juce::juce_audio_formats
        juce::juce_audio_devices
        juce::juce_events
        juce::juce_core
    )
    add_test(NAME vyra_loop_region_tests COMMAND vyra_loop_region_tests)

    add_executable(vyra_library_tests
        tests/library/TrackLibraryTests.cpp
        app/src/audio/BpmAnalysis.cpp
        app/src/audio/KeyAnalysis.cpp
        app/src/audio/Waveform.cpp
        app/src/library/TrackLibrary.cpp
    )
    target_include_directories(vyra_library_tests PRIVATE app/include)
    target_compile_features(vyra_library_tests PRIVATE cxx_std_20)
    target_compile_definitions(vyra_library_tests PRIVATE
        JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        JUCE_USE_CURL=0
    )
    target_link_libraries(vyra_library_tests PRIVATE
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_audio_devices
        juce::juce_events
        juce::juce_core
        vyra_sqlite
    )
    add_test(NAME vyra_library_tests COMMAND vyra_library_tests)

    add_executable(vyra_midi_controller_tests
        tests/controllers/MidiControllerTests.cpp
        app/src/integrations/MidiController.cpp
    )
    target_include_directories(vyra_midi_controller_tests PRIVATE app/include)
    target_compile_features(vyra_midi_controller_tests PRIVATE cxx_std_20)
    target_compile_definitions(vyra_midi_controller_tests PRIVATE
        JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        JUCE_USE_CURL=0
    )
    target_link_libraries(vyra_midi_controller_tests PRIVATE
        juce::juce_audio_devices
        juce::juce_data_structures
        juce::juce_events
        juce::juce_core
    )
    add_test(NAME vyra_midi_controller_tests COMMAND vyra_midi_controller_tests)
endif()
