#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

static void msg(const char *msg);

static void die(const char *msg);

static void do_something(int connfd);

int main() {
    // Creates a new TCP/IP socket
    // Stores file descriptor in variable fd
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    // Setting an option on the socket to allow address reuse
    // Needed for most server applications
    int val = 1;
    // Sets the SO_REUSEADDR option in the socket referenced by variable fd
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // Creates and initializes a socket address structure for IPv4
    struct sockaddr_in addr = {};
    // Sets the address family to IPv4
    addr.sin_family = AF_INET;
    // Sets the port number to 1234
    addr.sin_port = htons(1234);      // Port
    // Binds the socket to this address and port
    addr.sin_addr.s_addr = htonl(0);  // Wildcard IP 0.0.0.0
    // Checks if the binding operation failed, and if so, calls an error-handling function
    int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv) { die("bind()"); }

    // Specify an endpoint address for the TCP/IP IPv4 networking
    struct sockaddr_in {
        // Unasinged 16-bit integer type
        uint16_t sin_family;     // AF_INET
        uint16_t sin_port;       // Port in big-endian 

        // Holds the IPv4 address
        struct in_addr sin_addr; // IPv4
    };

    // Holds the 32-bit IPv4 address value
    struct in_addr {
        // Unasigned 32-bit integer
        uint32_t s_addr;   // IPv4 in big-endian
    };

    // Puts socket(fd) into listening mode
    // Ready to accept incoming connection requests
    rv = listen(fd, SOMAXCONN); // SOMAXCONN specifies how mnay pending connections cen be qeued
    if (rv) { die("Listen()"); }
    
    /*
        Simple, single-threaded server that handles one client at a time.
        Each client is processed sequentially and the server returns to accepting new connections
        after each client interaction is cmplete
    */
    while (true) {
        // Client address
        struct sockaddr_in client_addr = {};
        // Socket address length
        socklen_t addrlen = sizeof(client_addr);
        // Extracts the first connection request from the queue
        int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
       
        // Error
        if (connfd < 0) {
            continue;
        }

        // Handle the client connection
        do_something(connfd);
        // Closes the fie descriptor terminating the connection
        close(connfd);
    }
}

static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

// Handles communication with a client
static void do_something(int connfd) {
    /*
        Receives a message from the client
        Displays it on the server side
        Sends a fixed response ("world") back to the client
    */

    // Buffer to recieve data from the client
    char rbuf[64] = {};
    // Reads up to 63 bytes from the client connection
    ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
    // Error handeling
    if (n < 0) {
        msg("read() error");
        return;
    }
    // Displays client message
    printf("Client says: %s\n", rbuf);

    // Responds with the message "world" to the client
    char wbuf[] = "world";
    write(connfd, wbuf, strlen(wbuf));
}

