add_executable(k2d_pack k2d_pack.cpp)
target_link_libraries(k2d_pack PRIVATE k2d)

add_executable(k2d_scriptc k2d_scriptc.cpp)
target_link_libraries(k2d_scriptc PRIVATE k2d_zen)
target_compile_features(k2d_scriptc PRIVATE cxx_std_17)

add_executable(k2d_webserver k2d_webserver.cpp)
target_compile_features(k2d_webserver PRIVATE cxx_std_17)
if(WIN32)
    target_link_libraries(k2d_webserver PRIVATE ws2_32)
endif()
