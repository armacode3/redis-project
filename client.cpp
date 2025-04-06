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

static int32_t query(int fd, const char *text);

const size_t k_max_msg = 4096;

int main() {
    // Creates socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    // Error
    if (fd < 0) {
        die("socket()");
    }

    // Server address setup
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    // Converting from host to network byte order(short)
    addr.sin_port = ntohs(1234);
    // Converting from host to network byte order(long)
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK); // 127.0.0.1

    // Connecting to server
    int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
    // Error
    if (rv) {
        die("connect");
    }

    int32_t err = query(fd, "hello1");
    if (err) {
        goto L_DONE;
    }
    err = query(fd, "hello2");
    if (err) {
        goto L_DONE;
    }
    err = query(fd, "hello3");
    if (err) {
        goto L_DONE;
    }
L_DONE:
    close(fd);
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

static int32_t read_full(int fd, char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0) {
            return -1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int32_t write_all(int fd, const char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

// Sends a quer and recieves a response
static int32_t query(int fd, const char *text) {
    // Length of input text
    uint32_t len = (uint32_t)strlen(text);
    // Error
    if (len > k_max_msg) {
        return -1;
    }

    // Size of buffer
    char wbuf[4 + k_max_msg];
    // copy length to first 4 bytes of buffer
    memcpy(wbuf, &len, 4); // Asume little endian
    // Copy text after header
    memcpy(&wbuf[4], text, len);

    // Sending request
    if (int32_t err = write_all(fd, wbuf, 4 + len)) {
        return err;
    }
    // Buffer declaration for Response
    char rbuf[4 + k_max_msg + 1];
    errno = 0;

    // Reading response header
    int32_t err = read_full(fd, rbuf, 4);
    if (err) { // Error
        msg(errno == 0 ? "EOF" : "read() error");
        return err;
    }

    // Response length extraction
    memcpy(&len, rbuf, 4);
    if (len > k_max_msg) { // Error
        msg("too long");
        return -1;
    }

    // Reading response body
    err = read_full(fd, &rbuf[4], len);
    if (err) { // Error
        msg("read() error");
        return err;
    }

    // Processign response
    printf("Server says: %.*s\n", len, &rbuf[4]);
    return 0;
}
