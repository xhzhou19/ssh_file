# 设置CMake最低版本要求
cmake_minimum_required(VERSION 3.10)

# 定义项目名称和语言
project(SSHReader VERSION 1.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 查找必要的包
include_directories(/usr/local/include)
link_directories(/usr/local/lib)

# 添加源文件
set(SOURCES
    main.cpp
)

# 添加头文件目录
include_directories(
    ${PROJECT_SOURCE_DIR}/include
)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

# 链接库
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    -lssh2
)

# 设置编译选项
target_compile_options(${PROJECT_NAME}
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# 安装规则
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)