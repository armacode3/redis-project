#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

static void die(const char *msg);

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
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);

    // Connecting to server
    int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
    // Error
    if (rv) {
        die("connect");
    }

    // Sending message to server
    char msg[] = "hello";
    write(fd, msg, strlen(msg));

    // Receiving response from server
    char rbuf[64] = {};
    ssize_t n = read(fd, rbuf, sizeof(rbuf) - 1);
    if (n < 0) {
        die("read");
    }

    // Display server response
    printf("Server says: %s\n", rbuf);
    close(fd);
}

static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}