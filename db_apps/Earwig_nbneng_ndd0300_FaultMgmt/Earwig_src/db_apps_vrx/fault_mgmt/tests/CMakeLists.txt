cmake_minimum_required(VERSION 2.6)

set(CMAKE_C_COMPILER "gcc")
set(CMAKE_CXX_COMPILER "g++")

if(NOT DEFINED CDCS_LIBSTAGE)
    set(CDCS_LIBSTAGE ${CMAKE_BINARY_DIR}/../../../../staging_vrx_l)
endif()
set(create_gmock_c_class ${CDCS_LIBSTAGE}/bin/create_gmock_c_class)

set(tests test_fault_mgmt_all)

# for gtest and gmock see
#   https://larry-price.com/blog/2013/10/13/installing-gtest-and-gmock-libs-in-ubuntu-13-dot-04/

#
# other libraries mock functions
#
set(mock_functions_rdb_lib
    rdb_open
    rdb_close
    rdb_lock
    rdb_unlock
    rdb_getinfo
    rdb_get
    rdb_get_alloc
    rdb_create
    rdb_delete
    rdb_set
    rdb_get_int)
string(REPLACE ";" "," mock_functions_rdb_lib_csv "${mock_functions_rdb_lib}")
add_custom_command(
    OUTPUT  mockrdblib.cpp mockrdblib.h
    COMMAND ${create_gmock_c_class} --externc --no-concrete --cflags -I"$(CDCS_LIBSTAGE)/include" "$(CDCS_LIBSTAGE)/include/rdb_ops.h" ${mock_functions_rdb_lib_csv} mockrdblib MockRdbLib
)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/main.cpp"
"
#include <limits.h>
#include \"gtest/gtest.h\"
#include \"gmock/gmock.h\"

int main(int argc, char **argv) {
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
"
)

set(test test_fault_mgmt_all)
list(APPEND test_harness_srcs
            "RdbHandler.cpp"
            "test_fm_shared.cpp"
            "test_fm_supported_alarm.cpp"
            "test_fm_event.cpp"
            "test_current_alarm.cpp"
            "test_fault_mgmt.cpp")
set(${test}_mock_funcs
    ${mock_functions_rdb_lib}
    )
set(${test}_wrap_link_flags "-Wl")
foreach(mock_func IN LISTS ${test}_mock_funcs)
    set(${test}_wrap_link_flags "${${test}_wrap_link_flags},--wrap=${mock_func}")
endforeach()

# add the mock files
set(${test}_mock_files
    mockrdblib.cpp
    )
foreach(src IN LISTS ${test}_mock_files)
    list(APPEND test_srcs "${CMAKE_CURRENT_BINARY_DIR}/${src}")
endforeach()

file(GLOB srcs "../src/*.c")
foreach(src IN LISTS srcs)
    list(APPEND test_srcs "${src}")
endforeach()

include_directories(../inc ${CDCS_LIBSTAGE}/host/include ${CDCS_LIBSTAGE}/include ${CMAKE_CURRENT_BINARY_DIR})

# TODO fix link problems with i386 cmocka
#set(CMAKE_C_FLAGS         "${CMAKE_C_FLAGS} -m32")
set(CMAKE_C_FLAGS           "${CMAKE_C_FLAGS} --coverage -g -DUNIT_TESTING")
set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} -std=c++11 -g -DUNIT_TESTING")

# object library to build source codes only once for all the tests
add_library(testlib STATIC ${test_srcs})

foreach(test_name IN LISTS tests)
    add_executable(${test_name} ${test_harness_srcs} "${CMAKE_CURRENT_BINARY_DIR}/main.cpp")
    target_link_libraries(${test_name} testlib gtest gmock pthread)
    set_target_properties(${test_name} PROPERTIES LINK_FLAGS "-L ${CDCS_LIBSTAGE}/host/lib --coverage ${${test_name}_wrap_link_flags}")
    add_test(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)

    add_custom_target(
        ${test_name}.unittest
        ALL
        COMMENT "Run tests"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        #COMMAND ${CMAKE_CTEST_COMMAND} -C $<CONFIGURATION> -R "^${test_name}$" --output-on-failures
        COMMAND "$<TARGET_FILE:${test_name}>" --gmock_verbose=error --gtest_stack_trace_depth=10
        COMMAND -rm -rf Coverage
        COMMAND test -d Coverage || mkdir Coverage
        COMMAND find CMakeFiles/testlib.dir -name "*.gcno" -exec cp {} Coverage/ '\;'
        COMMAND find CMakeFiles/testlib.dir -name "*.gcda" -exec mv {} Coverage/ '\;'
        COMMAND cd Coverage && lcov --output-file coverage.info --capture --rc lcov_branch_coverage=1 --directory .
        COMMAND cd Coverage && lcov  --rc lcov_branch_coverage=1 --output-file coverage.info.cleaned --remove coverage.info "*/rdb_lib/*" "*/dpu_app/*" "*daemon_lib/*" && mv -f coverage.info{.cleaned,}
        COMMAND cd Coverage && genhtml --output-directory coverage --branch-coverage --function-coverage coverage.info
        COMMENT "Generated Coverage Report Successfully"
        DEPENDS ${test_name}
    )
endforeach(test_name)

# python -m "SimpleHTTPServer" &
