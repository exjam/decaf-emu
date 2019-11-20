if(WIN32)
   set(ENV{PATH} "${DEVKITPRO_WINPTHREAD_PATH};$ENV{PATH}")
endif()

execute_process(COMMAND ${WUT_BUILD_TOOL})
