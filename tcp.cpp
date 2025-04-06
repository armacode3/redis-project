#include <sys/socket.h>
#include <iostream>

int main() {
    try {
        // Creates a new TCP/IP socket
        // Stores file descriptor in variable fd
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            throw(fd);
        }
    } catch(int error) {
        std::cout << "Socket creation has failed" << error << std::endl;
    }

    // Setting an option on the socket to allow address reuse
    int val = 1;
    // Sets the SO_REUSEADDR option in the socket referenced by variable fd
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // 
    // Creates and initializes a socket address structure for IPv4
    struct sockaddr_in addr = {};
    // Sets the address family to IPv4
    addr.sin_family = AF_INET;
    // Sets the port number to 1234
    addr.sin_port = htons(1234);      // Port
    // Binds the socket to this address and port
    addr.sin_addr.s_addr = htonl(0);  // Wildcard IP 0.0.0.0
    // Checks if the binding operation failed, and if so, calls an error-handling function
    int rv = bind(fd, (const struct socksddr *)&addr, sizeof(addr));
    if (rv) { die("bind()"); }
}

