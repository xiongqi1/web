cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
endif()

# This shows one way to build a standalone plugin
# outside of lws itself

project(netc_netstat C)

set(PACKAGE "netc_netstat")
set(CPACK_PACKAGE_NAME "${PACKAGE}")
set(CPACK_PACKAGE_VERSION "0.1")
set(CPACK_PACKAGE_VENDOR "netcommwireless@netcommwireless.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PACKAGE} ${PACKAGE_VERSION}")
set(SOVERSION "1")
set(VERSION "0.1")

set(PLUGIN_NAME "netc_netstat")
# space-separated list of sources
set(PLUGIN_SRCS netc_netstat.c ds_store.c netstat.c rdbstat.c)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")

message(STATUS "CMAKE_TOOLCHAIN_FILE='${CMAKE_TOOLCHAIN_FILE}'")

# Try to find the current Git hash.
find_package(Git)
if(GIT_EXECUTABLE)
	execute_process(
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMAND "${GIT_EXECUTABLE}" describe
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
	execute_process(
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMAND "whoami"
    OUTPUT_VARIABLE GIT_USER
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
	execute_process(
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMAND "hostname"
    OUTPUT_VARIABLE GIT_HOST
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
	string(REGEX REPLACE "([^\\])[\\]([^\\])" "\\1\\\\\\\\\\2" GIT_USER ${GIT_USER})
    set(LWS_BUILD_HASH ${GIT_USER}@${GIT_HOST}-${GIT_HASH})
    message("Git commit hash: ${LWS_BUILD_HASH}")
endif()

set(CMAKE_C_FLAGS "-W ${CMAKE_C_FLAGS}")
set(CMAKE_C_FLAGS "-fPIC ${CMAKE_C_FLAGS}")

source_group("Headers Private"   FILES ${PLUGIN_HDR})
source_group("Sources"   FILES ${PLUGIN_SRCS})
add_library(${PLUGIN_NAME} SHARED ${PLUGIN_SRCS} ${PLUGIN_HDR})

target_link_libraries(${PLUGIN_NAME} -lwebsockets)

# Set test app specific defines.
set_property(TARGET ${PLUGIN_NAME}
	     PROPERTY COMPILE_DEFINITIONS
	     INSTALL_DATADIR="${CMAKE_INSTALL_PREFIX}/plugins"
)

list(APPEND PLUGINS_LIST ${PLUGIN_NAME})

install(TARGETS ${PLUGINS_LIST}
	PERMISSIONS  OWNER_WRITE OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE OWNER_READ GROUP_READ WORLD_READ
	DESTINATION share/lwsws/plugins
	COMPONENT plugins)


# Data files for running the netc_netstat server
set(CONFIG_DATA
	"${PROJECT_SOURCE_DIR}/etc/lwsws/conf"
	"${PROJECT_SOURCE_DIR}/etc/lwsws/conf.d/netc-plugin" )

add_custom_command(TARGET conf
				POST_BUILD 
				COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:conf>/../etc/lwsws/conf.d")

add_custom_command(TARGET conf
				POST_BUILD 
				COMMAND "${CMAKE_COMMAND}" -E copy "${PROJECT_SOURCE_DIR}/etc/lwsws/conf" "$<TARGET_FILE_DIR:conf>/../etc/lwsws/" VERBATIM)

add_custom_command(TARGET conf
				POST_BUILD 
				COMMAND "${CMAKE_COMMAND}" -E copy "${PROJECT_SOURCE_DIR}/etc/lwsws/conf.d/netc-plugin" "$<TARGET_FILE_DIR:conf>/../etc/lwsws/conf.d" VERBATIM)

install(FILES "${PROJECT_SOURCE_DIR}/etc/lwsws/conf"
		DESTINATION etc/lwsws
	COMPONENT examples)

install(FILES "${PROJECT_SOURCE_DIR}/etc/lwsws/conf.d/netc-plugin"
		DESTINATION etc/lwsws/conf.d
	COMPONENT examples)

set(TEST_SERVER_DATA
        "${PROJECT_SOURCE_DIR}/static/favicon.ico"
        "${PROJECT_SOURCE_DIR}/static/logo.png"
        "${PROJECT_SOURCE_DIR}/static/jquery-3.2.1.min.js"
        "${PROJECT_SOURCE_DIR}/static/iperfclient.sh"
        "${PROJECT_SOURCE_DIR}/static/iperfserver.sh"
        "${PROJECT_SOURCE_DIR}/static/pktgen.sh"
        "${PROJECT_SOURCE_DIR}/static/ss.cgi"
        "${PROJECT_SOURCE_DIR}/static/RGraph.common.core.js"
        "${PROJECT_SOURCE_DIR}/static/RGraph.common.dynamic.js"
        "${PROJECT_SOURCE_DIR}/static/RGraph.common.effects.js"
        "${PROJECT_SOURCE_DIR}/static/RGraph.gauge.js"
        "${PROJECT_SOURCE_DIR}/static/RGraph.line.js"
	"${PROJECT_SOURCE_DIR}/static/test.html")

add_custom_command(TARGET static
				POST_BUILD 
				COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:static>/../share/lwsws/netc_netstat")

# Copy the file needed to run the server so that the test apps can
# reach them from their default output location
foreach (TEST_FILE ${TEST_SERVER_DATA})
	if (EXISTS ${TEST_FILE})
		add_custom_command(TARGET static
					POST_BUILD 
					COMMAND "${CMAKE_COMMAND}" -E copy "${TEST_FILE}" "$<TARGET_FILE_DIR:static>/../share/lwsws/netc_netstat" VERBATIM)
	endif()
endforeach()

install(FILES ${TEST_SERVER_DATA}
		DESTINATION share/lwsws/netc_netstat
	COMPONENT examples)
