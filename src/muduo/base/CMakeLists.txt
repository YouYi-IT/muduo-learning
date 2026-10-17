aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} base_srcs)
add_library(muduo_base_obj OBJECT ${base_srcs})
target_include_directories(muduo_base_obj PUBLIC ${PROJECT_SOURCE_DIR}/include)