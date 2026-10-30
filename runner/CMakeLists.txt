# On Android the game is a library the NativeActivity loads (libk2d_runner.so).
if(ANDROID)
    add_library(k2d_runner SHARED src/main.cpp)
else()
    add_executable(k2d_runner src/main.cpp)
endif()

target_link_libraries(k2d_runner PRIVATE k2d k2d_zen k2d_physics2d)

target_compile_features(k2d_runner PRIVATE cxx_std_17)

if(EMSCRIPTEN)
    target_link_options(k2d_runner PRIVATE
        "-sALLOW_MEMORY_GROWTH=1"
        "-sFORCE_FILESYSTEM=1"
        "-sSTACK_SIZE=4mb"
        # miniaudio's Web Audio ScriptProcessor callback accesses the
        # interleaved PCM buffer through Module.HEAPF32. Emscripten keeps
        # this view private unless it is explicitly exported.
        "-sEXPORTED_RUNTIME_METHODS=HEAPF32,FS,addRunDependency,removeRunDependency"
        "-sMIN_WEBGL_VERSION=2"
        "-sMAX_WEBGL_VERSION=2"
        "-sFULL_ES3=1"
    )
endif()

set_target_properties(k2d_runner PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${K2D_RUNTIME_OUTPUT_DIRECTORY}
)

# The runner the editor's debugger starts: the same game on zen_debug, the VM
# with the debug hook. What ships is k2d_runner, which has none.
if(TARGET k2d_zen_debug)
    add_executable(k2d_runner_debug src/main.cpp)
    target_link_libraries(k2d_runner_debug PRIVATE k2d k2d_zen_debug k2d_physics2d)
    target_compile_features(k2d_runner_debug PRIVATE cxx_std_17)
    set_target_properties(k2d_runner_debug PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${K2D_RUNTIME_OUTPUT_DIRECTORY})
endif()
