cmake_minimum_required(VERSION 3.1)
project(ookla-mgr)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wno-psabi) #-Wno-psabi is to remove g++ warning notes
add_compile_options(-Os) # Optimise for size

add_executable(ookla-mgr Main.cpp OoklaOutputType.cpp SpeedTestTypes.cpp SpeedTest.cpp)

target_link_libraries(ookla-mgr dl rdb estd pthread stdc++fs)

INSTALL(TARGETS ookla-mgr RUNTIME DESTINATION /usr/bin/)