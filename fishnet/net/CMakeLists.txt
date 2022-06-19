include(CheckFunctionExists)

check_function_exists(accept4 HAVE_ACCEPT4)
if(NOT HAVE_ACCEPT4)
    set_source_files_properties(sockets_ops.cc PROPERTIES COMPILE_FLAGS "-DNO_ACCEPT4")
endif() 

set(net_SRCS
    sockets_ops.cc
    socket.cc
    inet_address.cc
)

add_library(fishnet_net ${net_SRCS})
target_link_libraries(fishnet_net fishnet_base)

install(TARGETS fishnet_net DESTINATION lib)

file(GLOB HEADERS "*.h")
install(FILES ${HEADERS} DESTINATION include/fishnet/net)

