cmake_minimum_required(VERSION 2.5)
project(mymuduo)

# mymuduo最终编译成so动态库，设置动态库路径,放在根目录的lib文件夹下
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib) 
#设置调试信息
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11")

aux_source_directory(. SRC_LIST)
#编译生成动态库
add_library(mymuduo SHARED ${SRC_LIST})
