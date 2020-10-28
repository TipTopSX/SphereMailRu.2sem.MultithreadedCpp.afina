#ifndef AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H

#include <cstring>

#include "../st_nonblocking/Connection.h"
#include <sys/epoll.h>

namespace Afina {
namespace Network {
namespace MTnonblock {
typedef Afina::Network::STnonblock::Connection super;

class Connection : public super {
public:
    Connection(int s, std::shared_ptr<spdlog::logger> &logger_, std::shared_ptr<Afina::Storage> &pStorage_)
        : super(s, logger_, pStorage_) {
        _event.data.ptr = this;
    }

    void Start() override {
        super::Start();
        _event.events |= EPOLLET;
    };

protected:
    void DoRead() override {
        std::unique_lock<std::mutex> lock(_mutex);
        super::DoRead();
    };

    void DoWrite() override {
        std::unique_lock<std::mutex> lock(_mutex);
        super::DoWrite();
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
