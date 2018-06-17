
cmake_minimum_required(VERSION 3.2)

if(0)
    project(wiiu-rpc C ASM)

    set(CMAKE_C_STANDARD 99)

    add_rpx_lite(wiiu-rpc
        src/main.c
        src/console.c
        src/server.c
        src/packet.c
        src/crt0.s)

    target_link_libraries(wiiu-rpc
        whb
        defaultheap
        coreinit
        sysapp
        nsysnet
        nn_ac
        proc_ui)
else()
    project(wiiu-rpc ASM CXX)
    include("${WUT_ROOT}/share/wut.cmake" REQUIRED)

    add_executable(wiiu-rpc
        src/main.cpp
        src/console.cpp
        src/server.cpp
        src/packet.cpp)
    target_link_libraries(wiiu-rpc
        whb
        defaultheap
        coreinit
        sysapp
        nsysnet
        nn_ac
        proc_ui)

    wut_create_rpx(wiiu-rpc.rpx wiiu-rpc)
endif()
