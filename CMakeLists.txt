cmake_minimum_required(VERSION 3.14)
project(LitematicaPreview VERSION 1.0.0 LANGUAGES C CXX)

include(FetchContent)

# 设置C++标准（nbt_cpp需要C++20）
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GLFW（窗口管理和OpenGL上下文创建）
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

# GLAD（OpenGL函数加载器）
add_library(glad STATIC extern/glad/src/gl.c)
target_include_directories(glad PUBLIC extern/glad/include)

# stb_image（图片加载，纯头文件）
add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE ${CMAKE_SOURCE_DIR})

# zlib（NBT解压依赖）
add_library(zlib STATIC
    extern/zlib/inflate.c
    extern/zlib/deflate.c
    extern/zlib/adler32.c
    extern/zlib/crc32.c
    extern/zlib/zutil.c
    extern/zlib/inftrees.c
    extern/zlib/inffast.c
    extern/zlib/trees.c
    extern/zlib/gzread.c
    extern/zlib/gzwrite.c
    extern/zlib/gzlib.c
    extern/zlib/gzclose.c
    extern/zlib/compress.c
    extern/zlib/uncompr.c
)
target_include_directories(zlib PUBLIC extern/zlib)

# xxhash（nbt_cpp可选依赖）
add_library(xxhash STATIC extern/xxhash/xxhash.c)
target_include_directories(xxhash PUBLIC extern/xxhash)

# nbt_cpp（纯头文件库，NBT解析）
add_library(nbt_cpp INTERFACE)
target_include_directories(nbt_cpp INTERFACE extern/nbt_cpp)
target_link_libraries(nbt_cpp INTERFACE zlib xxhash)

# nlohmann/json（JSON解析库）
add_subdirectory(extern/nlohmann_json EXCLUDE_FROM_ALL)

# 设置源文件
set(SOURCES
    src/main.cpp
    src/data/loader/LitematicLoader.cpp
    src/data/loader/BlockDecoder.cpp
    src/data/block_size/parsers/BlockSizeParser.cpp
    src/data/block_size/parsers/BlockstateParser.cpp
    src/data/block_size/mappers/SizeToModelMapper.cpp
    src/data/block_size/resolvers/ModelInheritanceResolver.cpp
    src/data/BlockTypeClassifier.cpp
    src/render/platform/Window.cpp
    src/render/core/Shader.cpp
    src/render/core/Renderer.cpp
    src/render/scene/Camera.cpp
    src/render/resource/Texture.cpp
    src/app/App.cpp
    src/render/platform/RenderLoop.cpp
    src/pipeline/Pipeline.cpp
    src/stage/LoadStage.cpp
    src/stage/MeshStage.cpp
    src/stage/FaceCuller.cpp
)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

# 链接库
target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
    nbt_cpp
    glad
    glfw
    stb_image
    nlohmann_json::nlohmann_json
)

# 设置输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
