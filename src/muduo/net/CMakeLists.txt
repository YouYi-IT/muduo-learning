aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} net_srcs)
add_library(muduo_net_obj OBJECT ${net_srcs})
target_include_directories(muduo_net_obj PUBLIC ${PROJECT_SOURCE_DIR}/include)