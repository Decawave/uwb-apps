project(slave_node)

set(CMAKE_VERBOSE_MAKEFILE ON)

#Copy syscfg.h from mynewt build system
#configure_file (
#  "${PROJECT_BINARY_DIR}/../../../bin/targets/${PROJECT_NAME}/generated/include/syscfg/syscfg.h"
#  "${PROJECT_SOURCE_DIR}/../include/syscfg/syscfg.h"
#)

if(NOT EXISTS "${PROJECT_BINARY_DIR}/../../../bin/targets/${PROJECT_NAME}/generated/include/syscfg/syscfg.h")
    message(WARNING "Building newt dependencies")
    execute_process(COMMAND newt target create ${PROJECT_NAME})
    execute_process(COMMAND newt target set ${PROJECT_NAME} app=apps/twr_nranges_tdma)
    execute_process(COMMAND newt target set ${PROJECT_NAME} bsp=@mynewt-dw1000-core/hw/bsp/dwm1001)
    execute_process(COMMAND newt target amend ${PROJECT_NAME} syscfg=NRANGES_ANCHOR=1)
    execute_process(COMMAND newt build ${PROJECT_NAME})
if(NOT EXISTS "${PROJECT_BINARY_DIR}/../../../bin/targets/${PROJECT_NAME}/generated/include/syscfg/syscfg.h")
    message( FATAL_ERROR "Building newt dependencies failed." )
endif() 
endif()

file(GLOB ${PROJECT_NAME}_SOURCES 
	Src/*.c
    	"${PROJECT_BINARY_DIR}/../../../bin/targets/${PROJECT_NAME}/generated/include/syscfg/*.h"
    	"${PROJECT_BINARY_DIR}/../../../bin/targets/${PROJECT_NAME}/generated/include/sysflash/*.h"
)
file(GLOB ${PROJECT_NAME}_HEADERS 
    include/*.h
    "${PROJECT_BINARY_DIR}/../../../bin/targets/${PROJECT_NAME}/generated/src/*.c"
)

add_executable(
    ${PROJECT_NAME} 
    ${${PROJECT_NAME}_SOURCES}
)
#target_link_libraries(
#    ${DW1000_DRIVER_ROOT}/dw1000
#    dpl
#    dpl_linux
#    dpl_os
#    Threads::Threads
#)

