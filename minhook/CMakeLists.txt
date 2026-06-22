FILE(GLOB_RECURSE SOURCE_FILES include/*.h src/*.h src/*.c)

add_library(minhook ${SOURCE_FILES})
# Phase 7: 修正头文件路径（之前写的是文件路径，应该是目录路径）
target_include_directories(minhook INTERFACE include)
target_include_directories(minhook PUBLIC include)
