#include <unistd.h>

#include <cstdio>
#include <utility>

#include "fishnet/base/logging.h"
#include "fishnet/base/thread.h"
#include "fishnet/net/callbacks.h"
#include "fishnet/net/event_loop.h"
#include "fishnet/net/inet_address.h"
#include "fishnet/net/tcp_server.h"

using namespace fishnet;
using namespace fishnet::net;

void onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        conn->setTcpNoDelay(true);
    }
}

void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp) {
    conn->send(buf);
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: server <address> <port> <threads>\n");
        return 0;
    }

    LOG_INFO << "pid = " << getpid() << ", tid = " << current_thread::tid();
    Logger::setLogLevel(Logger::LogLevel::TRACE);

    const char* ip = argv[1];
    uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
    InetAddress listenAddr{ip, port};
    int threadCount = atoi(argv[3]);

    EventLoop loop;

    TcpServer server(&loop, listenAddr, "PingPong");

    server.setConnectionCallback(onConnection);
    server.setMessageCallback(onMessage);

    if (threadCount > 1) {
        server.setThreadNum(threadCount);
    }

    server.start();

    loop.loop();

    return 0;
}
