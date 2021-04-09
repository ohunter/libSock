#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include <Sockets/socket.hpp>

void server(std::string address, uint16_t port) {
    size_t n;
    char   buf[256] = {0};

    auto sock = Sockets::UDPSocket::service(address, port, Sockets::Domain::IPv4);

    // Read the size of the incoming message
    sock->recv(buf, 1);
    n = (uint8_t)buf[0];
    sock->recv(buf, n);

    std::cout << "Server recieved " << n << " bytes containing: " << std::string(buf) << std::endl;

    char b[] = {(char)n, 0};
    sock->send(b, 1);
    sock->send(buf, n);

    // Terminate the socket
    sock->close();
}

int main(int argc, char *argv[]) {
    // Start server
    std::thread t0(server, "127.0.0.1", 12345);

    // Sleep for 1 second to let the server get to the `accept` call
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Connect to the server
    auto sock = Sockets::UDPSocket::connect("127.0.0.1", 12345, Sockets::Domain::IPv4);

    char        buf[256] = {0};
    std::string s        = "";

    std::getline(std::cin, s);
    char b[] = {(char)s.size(), 0};
    sock->send(b, 1);
    sock->send(s.c_str(), s.size());

    sock->recv(b, 1);
    sock->recv(buf, (int)b[0]);

    std::cout << "Client recieved " << (int)b[0] << " bytes containing: " << std::string(buf)
              << std::endl;

    // Close the connection
    sock->close();

    t0.join();
}