add_executable(k2d_runner
    src/main.cpp
)

target_link_libraries(k2d_runner PRIVATE k2d k2d_zen k2d_physics2d)

if(WIN32 AND TARGET SDL2::SDL2main)
    target_link_libraries(k2d_runner PRIVATE SDL2::SDL2main)
endif()
if(MINGW)
    target_compile_definitions(k2d_runner PRIVATE SDL_MAIN_HANDLED)
endif()
target_compile_features(k2d_runner PRIVATE cxx_std_17)

if(EMSCRIPTEN)
    target_link_options(k2d_runner PRIVATE
        "-sASYNCIFY"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sFORCE_FILESYSTEM=1"
        "-sSTACK_SIZE=4mb"
        # miniaudio's Web Audio ScriptProcessor callback accesses the
        # interleaved PCM buffer through Module.HEAPF32. Emscripten keeps
        # this view private unless it is explicitly exported.
        "-sEXPORTED_RUNTIME_METHODS=HEAPF32"
        "-sMIN_WEBGL_VERSION=2"
        "-sMAX_WEBGL_VERSION=2"
        "-sFULL_ES3=1"
    )
endif()

set_target_properties(k2d_runner PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${K2D_RUNTIME_OUTPUT_DIRECTORY}
)
