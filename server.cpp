// stdlib
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
// System
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
// C++
#include <vector>

static void msg(const char *msg);

static void msg_errno(const char *msg);

static void die(const char *msg);

static void fd_set_nb(int fd);

static void buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len);

static void buf_consume(std::vector<uint8_t> &buf, size_t n);

struct Conn {
    int fd = -1;

    // Application's intentions for even loop
    // fd list for the readiness API
    bool want_read = false;
    bool want_write = false;

    // Destroy connection
    bool want_close = false;

    // Buffered input and output
    std::vector<uint8_t> incoming; // Buffers data from socket for protocal parser
    std::vector<uint8_t> outgoing; // Buffers generated response written to socket
};

static Conn *handle_accept(int fd){
    /*
        Accepts nwe client connection on a listenging socket.
        Creates new connection object
        Sets the connection to non-blocking mode
        Initializes the connection to be ready for reading
    */
    // Holds client's address information
    struct sockaddr_in client_addr = {};
    // Holds the size of client_addr
    socklen_t addrlen = sizeof(client_addr);
    // Accepts a new connection
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
    if (connfd < 0) { // Error
        return NULL;
    }
    uint32_t ip = client_addr.sin_addr.s_addr;
    fprintf(stderr, "new client from %u.%u.%u.%u:%u\n", ip & 255, (ip >> 8) & 255, (ip >> 16) & 255, ip >> 24, ntohs(client_addr.sin_port));

    // Sets new connection fd to nonblocking mode
    fd_set_nb(connfd);
    Conn *conn = new Conn(); // Newly accepted connection
    conn->fd = connfd;
    conn->want_read = true; // Read data for this connection
    return conn;
}

static bool try_one_request(Conn *conn);

static void handle_write(Conn *conn);

static void handle_read(Conn *conn);

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

    // Bind
    // Creates and initializes a socket address structure for IPv4
    struct sockaddr_in addr = {};
    // Sets the address family to IPv4
    addr.sin_family = AF_INET;
    // Sets the port number to 1234
    addr.sin_port = htons(1234);      // Port
    // Binds the socket to this address and port
    addr.sin_addr.s_addr = htonl(0);  // Wildcard IP 0.0.0.0
    // Checks if the binding operation failed, and if so, calls an error-handling function
    int rv = bind(fd, (const sockaddr *)&addr, sizeof(addr));
    if (rv) { die("bind()"); }

    // set the listen fd to nonblocking mode
    fd_set_nb(fd);

    // Puts socket(fd) into listening mode
    // Ready to accept incoming connection requests
    rv = listen(fd, SOMAXCONN); // SOMAXCONN specifies how mnay pending connections cen be qeued
    if (rv) { die("Listen()"); }

    // A map of all client connections, keyed by fd
    std::vector<Conn *> fd2conn;
    // The event loop
    std::vector<struct pollfd> poll_args;
    while (true) {
        /*
            poll() system call
            Efficiently monitor mutiple file descriptors for I/O events
        */
        // Prepare the arguments of the poll()
        poll_args.clear();

        // Put the litstening sockets in the first position
        struct pollfd pfd = {fd, POLLIN, 0};
        poll_args.push_back(pfd);

        // The rest are connection sockets
        for (Conn *conn : fd2conn) {
            if (!conn) {
                continue; // Skips closed or invalid connections
            }
            struct pollfd pfd = {conn->fd, POLLERR, 0}; // ollfd structure for client connection
            // poll() flags from the application's intent
            if (conn->want_read) {
                pfd.events |= POLLIN; // Add POLLIN flag
            }
            if (conn->want_write) {
                pfd.events |= POLLOUT; // Add POLLOUT flag
                // Notifys when the socket is ready to accept data for writing
            }
            poll_args.push_back(pfd); // Adds client connection's to vector
        }

        // Wait for readiness
        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1); // Indicates how many fd's have events
        // Signal interuptions
        if (rv < 0 && errno == EINTR) { // Not considered a fatal error
            continue; // Not an error
        }
        if (rv < 0) {
            die("poll");
        }

        // Handle the listening socket
        if (poll_args[0].revents) { // If there are any events for the listening socket
            if (Conn *conn = handle_accept(fd)) { // If new connection was successfully accepted
                // Put it into the map
                if (fd2conn.size() <= (size_t)conn->fd) { // Checks if size is enough
                    fd2conn.resize(conn->fd + 1); // Resizes the vector
                }
                assert(!fd2conn[conn->fd]);
                fd2conn[conn->fd] = conn; // Stores Con object into vector
            }
        }

        // Processes all the connection sockets that were monitored by poll()
        for (size_t i = 1; i < poll_args.size(); ++i) { // Skips 0 because 0 is a listening socket
            uint32_t ready = poll_args[i].revents;
            if (ready == 0) {
                continue;
            }

            Conn *conn = fd2conn[poll_args[i].fd]; // Retrieves the Conn objectassociated with current fd
            if (ready & POLLIN) { // Checks if POLLIN is set
                assert(conn->want_read);
                handle_read(conn); // Reads
            }
            if (ready & POLLOUT) { // Checks if POLLOUT is set
                assert(conn->want_write);
                handle_write(conn); // Writes
            }

            // Closes the socket from socket error or application logic
            if ((ready & POLLERR) || conn->want_close) {
                (void)close(conn->fd);
                fd2conn[conn->fd] = NULL;
                delete conn;
            }
        }
    }
    return 0;
}

static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

static void msg_errno(const char *msg) {
    fprintf(stderr, "[errno:%d] %s\n", errno, msg);
}

static void die(const char *msg) {
    fprintf(stderr, "[%d] %s\n", errno, msg);
    abort();
}

static void fd_set_nb(int fd) {
    /*
        Set a fd to non-blocking mode
        Gets current flags of fd
        Adds non-blocking flag to existing flags
        Sets the updated flags back to fd
    */
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if (errno) {
        die("fcntl error");
        return;
    }

    flags |= O_NONBLOCK;

    errno = 0;
    (void)fcntl(fd, F_SETFL, flags);
    if (errno) {
        die("fcntl error");
    }
}

// Process 1 request if there is enough data
static bool try_one_request(Conn *conn) {
    // 3. Try to parse the accumulated buffer
    if (conn->incoming.size() < 4) { // If incoming message is fewer than 4 bytes returns false
        return false; // Want read
    } 

    // Copies the first 4 bytes of data into lwngth
    uint32_t len = 0;
    memcpy(&len, conn->incoming.data(), 4);

    // Checks if message length exceeds maximum length
    if (len > k_max_msg) {
        msg("too long");
        conn->want_close = true;
        return false; // Want close
    }

    // Varifies if buffer contains the full message
    if (4 + len > conn->incoming.size()) {
        return false; // Want read
    }
    // Pints to the start of the message body after the header
    const uint8_t *request = &conn->incoming[4];
    // 4. Process the parsed message
    // Got one request, do some application logic
    printf("client says: len:%d data:%.*s\n", len, len < 100 ? len : 100, request);
    // Appends the 4-byte length header than the message body(request)
    // Generate the response
    buf_append(conn->outgoing, (const uint8_t *)&len, 4);
    buf_append(conn->outgoing, request, len);
    // 5. Remove the message from `Conn::incoming`
    // Removes the processed message
    buf_consume(conn->incoming, 4 + len);
    return true; // Success
}

static void handle_write(Conn *conn) {
    assert(conn->outgoing.size() > 0); // Ensures there is data in the outgoing buffer to write
    // Write data from the buffer to fd
    ssize_t rv = write(conn->fd, &conn->outgoing[0], conn->outgoing.size()); // conn->outgoing.data(): start of data, conn->outgoing.size(): number of bytes available
    if (rv < 0 && errno == EAGAIN) { // Error
        return; // Not ready
    }
    if (rv < 0) {
        msg_errno("write() error");
        conn->want_close = true;
        return; // want close
    }

    // Remove written data from `outgoing`
    buf_consume(conn->outgoing, (size_t)rv);
    if (conn->outgoing.size() == 0) { // All data written
        conn->want_read = true;
        conn->want_write = false;
    } // else: want write
}

static void handle_read(Conn *conn) {
    // 1. Do a non-blocking read
    uint8_t buf[64 * 1024]; // Declares buffer with 64 KB
    ssize_t rv = read(conn->fd, buf, sizeof(buf)); // Reads data from fd into buffer
    if (rv < 0 && errno == EAGAIN) { 
        return; // Not ready
    }
    // Handle IO error
    if (rv < 0) {
        msg_errno("read() error");
        conn->want_close = true;
        return; // want close
    }
    // Handle EOF
    if (rv == 0) {
        if (conn->incoming.size() == 0) {
            msg("client closed");
        } else {
            msg("unexpected EOF");
        }
        conn->want_close = true;
        return; // Want close
    }

    // 2. Add new data to the `Conn::incoming` buffer
    // Appends the newly read data to the buffer in the Conn structure
    buf_append(conn->incoming, buf, (size_t)rv); // conn->incoming: incoming data is accumulated
    // 3. Try to parse the accumulated buffer
    // 4. Process the parsed message
    // 5. Remove the message from `Conn::incoming`
    while (try_one_request(conn)) {} // Atempt to parse one complete request from the accumulated data in the Conn Structure
    
    // Update the readiness intention
    if (conn->outgoing.size() > 0) { // Has a response
        conn->want_read = false;
        conn->want_write = true;

        // The socket is likely ready to write in a request-response protocal
        return handle_write(conn);
    } // else: want read
}

// Appends to the back of buffer
static void buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len) {
    buf.insert(buf.end(), data, data + len);
}

// Removes from the front of buffer
static void buf_consume(std::vector<uint8_t> &buf, size_t n) {
    buf.erase(buf.begin(), buf.begin() + n);
}
