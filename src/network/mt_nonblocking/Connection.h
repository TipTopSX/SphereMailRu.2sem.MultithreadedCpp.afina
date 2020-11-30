#ifndef AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H

#include <cstring>
#include <sys/epoll.h>

#include "../st_nonblocking/Connection.h"

namespace Afina {
namespace Network {
namespace MTnonblock {

class Connection : public Afina::Network::STnonblock::Connection {
public:
    Connection(int s, std::shared_ptr<spdlog::logger> &logger_, std::shared_ptr<Afina::Storage> &pStorage_)
        : Afina::Network::STnonblock::Connection(s, logger_, pStorage_) {
        _event.data.ptr = this;
    }

    void Start() override {
        std::unique_lock<std::mutex> lock(_mutex);
        Afina::Network::STnonblock::Connection::Start();
        _event.events |= EPOLLET;
    };

protected:
    void DoRead() override {
        std::unique_lock<std::mutex> lock(_mutex);
        Afina::Network::STnonblock::Connection::DoRead();
    };

    void DoWrite() override {
        std::unique_lock<std::mutex> lock(_mutex);
        Afina::Network::STnonblock::Connection::DoWrite();
    };

private:
    friend class Worker;
    friend class ServerImpl;

    std::mutex _mutex;
};

} // namespace MTnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
