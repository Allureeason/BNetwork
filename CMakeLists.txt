cmake_minimum_required(VERSION 3.15)

# 设置项目名称
project(BNetwork)

# 生成文件索引
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# 设置库名称
set(LIBRARY_NAME "BNetwork")

# 源文件
file(GLOB SOURCE_FILE
    "./src/Net/*.cpp"
    "./src/Poller/*.cpp"
    "./src/Thread/*.cpp"
)

# 头文件
file(GLOB HEADER_FILE
    "./src/Net/*.h"
    "./src/Poller/*.h"
    "./src/Thread/*.h"
)

# 依赖库
set(DEPEND_LIBS
    pthread # 线程库
)

# 添加生成库, 动态库
add_library(${LIBRARY_NAME} ${HEADER_FILE} SHARED ${SOURCE_FILE})
target_link_directories(${LIBRARY_NAME} ${HEADER_FILE})
target_link_libraries(${LIBRARY_NAME} ${DEPEND_LIBS})
