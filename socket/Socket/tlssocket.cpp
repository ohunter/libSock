#include <cstring>
#include <iostream>
#include <stdexcept>

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../Exceptions/exceptions.hpp"
#include "socket.hpp"

namespace Sockets {

    TLSSocket::TLSSocket(TCPSocket *tcp, SSL_CTX *ctx) : TCPSocket(tcp) {
        if ((this->ssl = SSL_new(ctx)) == NULL) {
            throw std::runtime_error("Error when creating SSL state");
        }

        if (SSL_set_fd(this->ssl, this->_fd) == 0) {
            throw std::runtime_error("Error when attempting to bind file "
                                     "descriptor to SSL state");
        }
    }

    TLSSocket::TLSSocket(struct addrinfo &info, Domain dom, SSL_CTX *ctx, Operation op)
        : TCPSocket(info, dom, op) {
        if ((this->ssl = SSL_new(ctx)) == NULL) {
            throw std::runtime_error("Error when creating SSL state");
        }

        if (SSL_set_fd(this->ssl, this->_fd) == 0) {
            throw std::runtime_error("Error when attempting to bind file "
                                     "descriptor to SSL state");
        }
    }

    TLSSocket::TLSSocket(TLSSocket &other) : TCPSocket(other) {
        if (SSL_set_fd(this->ssl, this->_fd) == 0) {
            throw std::runtime_error("Error when attempting to bind file "
                                     "descriptor to SSL state");
        }
    }

    TLSSocket::TLSSocket(TLSSocket &&other) : TCPSocket(other) {
        if (SSL_set_fd(this->ssl, this->_fd) == 0) {
            throw std::runtime_error("Error when attempting to bind file "
                                     "descriptor to SSL state");
        }
    }

    TLSSocket::~TLSSocket() { }

    void TLSSocket::connect() {
        int m;
        if (this->state != State::Instantiated)
            throw std::runtime_error("Cannot connect with a busy socket");

        TCPSocket::connect();

        // Start handshaking process
        if ((m = SSL_connect(this->ssl)) != 1)
            throw_ssl_error(SSL_get_error(this->ssl, m));

        // Check if the socket received a certificate
        X509 *cert = SSL_get_peer_certificate(this->ssl);

        if (!cert)
            throw std::runtime_error("No X509 certificate received from server");

        X509_free(cert);

        // Verify the received certificate
        if (SSL_get_verify_result(this->ssl) != X509_V_OK)
            throw std::runtime_error("Failed to verify received certificate");
    }

    void TLSSocket::service(int backlog) { TCPSocket::service(backlog); }

    TLSSocket *TLSSocket::connect(std::string address, uint16_t port, Domain dom, SSL_CTX *ctx,
                                  Operation op) {
        auto tcp = TCPSocket::connect(address, port, dom, op);

        TLSSocket *sock = new TLSSocket(tcp, ctx);

        sock->connect();

        return sock;
    }
    TLSSocket *TLSSocket::service(std::string address, uint16_t port, Domain dom, SSL_CTX *ctx,
                                  Operation op, int backlog) {
        auto tcp = TCPSocket::service(address, port, dom, op);

        TLSSocket *sock = new TLSSocket(tcp, ctx);

        sock->service(backlog);

        return sock;
    }

    TLSSocket *TLSSocket::accept(SSL_CTX *ctx, Operation op, int flag) {
        TCPSocket *tcp = TCPSocket::accept(Operation::Blocking, flag & ~SOCK_NONBLOCK);

        TLSSocket *out = new TLSSocket(tcp, ctx);
        int        m   = 0;

        if ((m = SSL_accept(out->ssl)) == 0)
            throw std::runtime_error("Graceful rejection of SSL handshake");
        else if (m < 0)
            throw std::runtime_error("Error when performing SSL handshake");

        if (op == Operation::Non_blocking)
            if (fcntl(this->_fd, F_SETFL, fcntl(this->_fd, F_GETFL, 0) | O_NONBLOCK) == -1) {
                perror("TLSSocket::accept(SSL_CTX*, Operation, int): ");
                throw std::runtime_error("Error when making socket non-blocking");
            }

        tcp->~TCPSocket();

        return out;
    }

    void TLSSocket::close() {
        int m;

        if (this->state != State::Closed) {

            // Force socket to be blocking to avoid needing another round of
            // polling
            if (fcntl(this->_fd, F_SETFL, fcntl(this->_fd, F_GETFL, 0) & ~O_NONBLOCK) == -1) {
                perror("TLSSocket::close(): ");
                throw std::runtime_error("Error when making socket blocking");
            }

            if ((m = SSL_shutdown(this->ssl)) == 0) {
            } else if (m < 0)
                throw_ssl_error(SSL_get_error(this->ssl, m));

            SSL_free(this->ssl);
        }

        TCPSocket::close();
    }

    size_t TLSSocket::send(const char *buf, size_t buflen) {
        std::lock_guard<std::mutex> lock(this->mtx);
        size_t                      n = 0;
        ssize_t                     m = 0;

        while (n < buflen) {
            if ((m = SSL_write(this->ssl, &buf[n], buflen - n)) <= 0)
                throw_ssl_error(SSL_get_error(this->ssl, m));

            n += m;
        }

        return n;
    }

    size_t TLSSocket::recv(char *buf, size_t buflen) {
        std::lock_guard<std::mutex> lock(this->mtx);
        size_t                      n = 0;
        ssize_t                     m = 0;

        while (n < buflen) {
            if ((m = SSL_read(this->ssl, &buf[n], buflen - n)) <= 0)
                throw_ssl_error(SSL_get_error(this->ssl, m));

            n += m;
        }

        return 0;
    }

} // namespace Sockets