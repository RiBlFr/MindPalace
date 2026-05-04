cmake_minimum_required(VERSION 3.16)
project(MindPalace LANGUAGES CXX)

# 1. 自动处理 Qt 的元对象编译器 (MOC)、UI 编译器 (UIC) 和资源编译器 (RCC)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2. 查找 Qt6 库
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# 3. 包含路径：这样你在写代码时可以直接 #include "model/card.h"[cite: 2]
include_directories(src)

# 4. 搜集所有源文件
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/*.h"
    "src/view/*.ui"
)

# 5. 生成可执行文件[cite: 2]
add_executable(MindPalace ${SOURCES})

# 6. 链接 Qt 库
target_link_libraries(MindPalace PRIVATE Qt6::Widgets)