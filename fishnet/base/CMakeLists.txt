set(base_SRCS
    async_logging.cc
    condition.cc
    countdown_latch.cc
    current_thread.cc
    date.cc
    exception.cc
    file_util.cc
    log_file.cc
    logging.cc
    log_stream.cc
    process_info.cc
    thread.cc
    timestamp.cc
    time_zone.cc
)

add_library(fishnet_base ${base_SRCS})
target_link_libraries(fishnet_base pthread rt)

install(TARGETS fishnet_base DESTINATION lib)

file(GLOB HEADERS "*.h")
install(FILES ${HEADERS} DESTINATION include/fishnet/base)

