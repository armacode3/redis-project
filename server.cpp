#include <assert.h>
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

static int32_t read_full(int fd, char *buf, size_t n);

static int32_t write_all(int fd, const char *buf, size_t n);

static int32_t one_request(int connfd);

// Maximum msg length
const size_t k_max_msg = 4096;

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

        // Only serves one client connection at once
        while(true) {
            int32_t err = one_request(connfd);
            if (err) {
                break;
            }
        }
        close(connfd);
    }
    return 0;
}

static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

// Read complete buffer
static int32_t read_full(int fd, char *buf, size_t n) {
    /*
        Ensures all requested bytes are raed from a file descriptor.
        Handles the common case where a single read() call might not 
        return all requested bytes.
    */
    // Reading while checking if there are still bytes to read
    while (n > 0) {
        // Reads up to n bytes from fd into buffer
        ssize_t rv = read(fd, buf, n);
        // Error
        if (rv <= 0) {
            return -1;
        }

        // Verifies the condition and terminates if false
        assert((size_t)rv <= n);

        // Buffer Management
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

// Attempts to write a complete buffer
static int32_t write_all(int fd, const char *buf, size_t n) {
    /*
        Ensures all requested bytes are written to a file descriptor.
        Handles the common case where a single write() call might not 
        write all requested bytes.
    */
    while (n > 0) {
        // Write up to n bytes from buffer to fd
        ssize_t rv = write(fd, buf, n);
        // Error
        if (rv <= 0) {
            return -1;
        }
        assert((size_t)rv <= n);
        // Buffer management
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

// Handles a single client request
static int32_t one_request(int connfd) {
    /*
        Handle variable-length messages by prefixing each message with its length, 
        allowing the receiver to know exactly how many bytes to read for the complete message.
    */
    // Declares buffer
    char rbuf[4 + k_max_msg]; // Size of buffer
    // Stores error codes
    errno = 0;
    // Read exaclty 4 bytes
    int32_t err = read_full(connfd, rbuf, 4);
    // Error
    if (err) {
        msg(errno == 0 ? "EOF" : "read() error");
        return err;
    }

    uint32_t len = 0;
    // Function to copy memory
    // Assume little endian
    memcpy(&len, rbuf, 4); // Destination, source, byte count
    // Length validation
    if (len > k_max_msg) {
        msg("too long");
        return -1;
    }

    // Reading message body after header
    err = read_full(connfd, &rbuf[4], len);
    // Error
    if (err) {
        msg("read() error");
        return err;
    }

    // Processing Message
    fprintf(stderr, "Client says: %.*s\n", len, &rbuf[4]);

    // Preparing Response
    const char reply[] = "world";
    // Buffer size
    char wbuf[4 + sizeof(reply)];
    // Length of reply string
    len = (uint32_t)strlen(reply);

    // Building Response
    memcpy(wbuf, &len, 4); // Copy lenght to first 4 bytes of buffer
    memcpy(&wbuf[4], reply, len); // Coppy reply after header

    // Sending Response
    return write_all(connfd, wbuf, 4 + len);
}

