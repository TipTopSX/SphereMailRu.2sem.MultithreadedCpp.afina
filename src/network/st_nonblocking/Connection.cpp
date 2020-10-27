#include "Connection.h"

#include <csignal>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

namespace Afina {
namespace Network {
namespace STnonblock {

// See Connection.h
void Connection::Start() {
    _event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
    _event.data.fd = _socket;
    _event.data.ptr = this;
    _logger->debug("Started connection on socket {}", _socket);
}

// See Connection.h
void Connection::OnError() {
    _running = false;
    _logger->error("Error on socket {}", _socket);
}

// See Connection.h
void Connection::OnClose() {
    _running = false;
    _logger->debug("Closed connection on socket {}", _socket);
}

// See Connection.h
void Connection::DoRead() {
    try {
        int readed_bytes = -1;
        while ((readed_bytes = read(_socket, _client_buffer, sizeof(_client_buffer))) > 0) {
            _logger->debug("Got {} bytes from socket", readed_bytes);

            // Single block of data readed from the socket could trigger inside actions a multiple times,
            // for example:
            // - read#0: [<command1 start>]
            // - read#1: [<command1 end> <argument> <command2> <argument for command 2> <command3> ... ]
            while (readed_bytes > 0) {
                _logger->debug("Process {} bytes", readed_bytes);
                // There is no command yet
                if (!_command_to_execute) {
                    std::size_t parsed = 0;
                    if (_parser.Parse(_client_buffer, readed_bytes, parsed)) {
                        // There is no command to be launched, continue to parse input stream
                        // Here we are, current chunk finished some command, process it
                        _logger->debug("Found new command: {} in {} bytes", _parser.Name(), parsed);
                        _command_to_execute = _parser.Build(_arg_remains);
                        if (_arg_remains > 0) {
                            _arg_remains += 2;
                        }
                    }

                    // Parsed might fails to consume any bytes from input stream. In real life that could happens,
                    // for example, because we are working with UTF-16 chars and only 1 byte left in stream
                    if (parsed == 0) {
                        break;
                    } else {
                        std::memmove(_client_buffer, _client_buffer + parsed, readed_bytes - parsed);
                        readed_bytes -= parsed;
                    }
                }

                // There is command, but we still wait for argument to arrive...
                if (_command_to_execute && _arg_remains > 0) {
                    _logger->debug("Fill argument: {} bytes of {}", readed_bytes, _arg_remains);
                    // There is some parsed command, and now we are reading argument
                    std::size_t to_read = std::min(_arg_remains, std::size_t(readed_bytes));
                    _argument_for_command.append(_client_buffer, to_read);

                    std::memmove(_client_buffer, _client_buffer + to_read, readed_bytes - to_read);
                    _arg_remains -= to_read;
                    readed_bytes -= to_read;
                }

                // Thre is command & argument - RUN!
                if (_command_to_execute && _arg_remains == 0) {
                    _logger->debug("Start command execution");

                    std::string result;
                    if (!_argument_for_command.empty()) {
                        _argument_for_command.resize(_argument_for_command.size() - 2);
                    }
                    _command_to_execute->Execute(*_pStorage, _argument_for_command, result);

                    // Send response
                    result += "\r\n";
                    auto was_empty = _output.empty();
                    _output.push_back(result);
                    if (was_empty) {
                        _event.events |= EPOLLOUT;
                    }

                    // Prepare for the next command
                    _command_to_execute.reset();
                    _argument_for_command.resize(0);
                    _parser.Reset();
                }
            } // while (readed_bytes)
        }

        if (readed_bytes == 0) {
            _logger->debug("Connection closed");
        } else {
            throw std::runtime_error(std::string(strerror(errno)));
        }
    } catch (std::runtime_error &ex) {
        if (errno == EAGAIN) {
            _eof = true;
        } else {
            _logger->error("Failed to read connection on descriptor {}: {}", _socket, ex.what());
        }
    }
}

// See Connection.h
void Connection::DoWrite() {
    _logger->debug("Writing on socket {}", _socket);
    try {
        while (!_output.empty()) {
            auto result = _output.front();
            if (send(_socket, result.data(), result.size(), 0) <= 0) {
                throw std::runtime_error("Failed to send response");
            }
            _output.pop_front();
        }
        if (_output.empty()) {
            _event.events &= !EPOLLOUT;
            if (_eof) {
                _running = false;
            }
        }
    } catch (std::runtime_error &ex) {
        if (errno != EAGAIN) {
            _logger->error("Failed to write connection on descriptor {}: {}", _socket, ex.what());
        }
    }
}

} // namespace STnonblock
} // namespace Network
} // namespace Afina
