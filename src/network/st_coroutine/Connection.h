#ifndef AFINA_NETWORK_ST_COROUTINE_CONNECTION_H
#define AFINA_NETWORK_ST_COROUTINE_CONNECTION_H

#include <cstring>
#include <deque>
#include <memory>
#include <sys/epoll.h>

#include "protocol/Parser.h"
#include <afina/Storage.h>
#include <afina/coroutine/Engine.h>
#include <afina/execute/Command.h>
#include <spdlog/logger.h>

namespace Afina {
namespace Network {
namespace STcoroutine {

class Connection {
public:
    Connection(int s, int epoll_descr, Afina::Coroutine::Engine *engine, std::shared_ptr<spdlog::logger> &logger_,
               std::shared_ptr<Afina::Storage> &pStorage_)
        : _epoll_descr(epoll_descr), _engine(engine), _socket(s), _logger{logger_}, _pStorage{pStorage_} {
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
    }

    inline bool isAlive() const { return _running; }

    void Start();

    Afina::Coroutine::Engine::context *ctx_read{nullptr};
    Afina::Coroutine::Engine::context *ctx_write{nullptr};

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class ServerImpl;

    int _socket;
    struct epoll_event _event;
    bool _running{true};
    int _epoll_descr;
    Afina::Coroutine::Engine *_engine;

    std::shared_ptr<spdlog::logger> _logger;
    std::shared_ptr<Afina::Storage> _pStorage;

    std::size_t _arg_remains;
    Protocol::Parser _parser;
    std::string _argument_for_command;
    std::unique_ptr<Execute::Command> _command_to_execute;
    char _client_buffer[4096];
    std::deque<std::string> _output;
    bool _eof{false};
};

} // namespace STcoroutine
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_ST_COROUTINE_CONNECTION_H