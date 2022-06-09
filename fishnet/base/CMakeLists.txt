set(base_SRCS
    condition.cc
    countdown_latch.cc
    current_thread.cc
    exception.cc
    thread.cc
    timestamp.cc
)

add_library(fishnet_base ${base_SRCS})
target_link_libraries(fishnet_base pthread rt)

install(TARGETS fishnet_base DESTINATION lib)

file(GLOB HEADERS "*.h")
install(FILES ${HEADERS} DESTINATION include/fishnet/base)

