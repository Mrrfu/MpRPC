set(SRC_LIST userservice.cc ../user.pb.cc)
add_executable(proivder ${SRC_LIST})
target_link_libraries(proivder mprpc protobuf)