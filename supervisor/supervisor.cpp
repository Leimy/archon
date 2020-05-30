// -*- mode: c++; eval: (c-set-style "stroustrup"); eval: (c-set-style "archon-cc-mode"); -*-
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory.h>

#include "include/util.h"

using namespace std;

/**
 * Receive a file descriptor over a Unix Domain Socket
 */
int
recvfd(int sock)
{
    int fd;
    int n;
    char buf[1];
    struct iovec io {.iov_base = buf, .iov_len = 1};
    struct msghdr hdr = {};
    struct cmsghdr *chdr;
    char cms[CMSG_SPACE(sizeof(int))];

    hdr.msg_iov = &io;
    hdr.msg_iovlen = 1;
    hdr.msg_control = (caddr_t)chdr;
    hdr.msg_controllen = sizeof(chdr);

    if ((n = recvmsg(sock, &hdr, 0)) < 0) {
	return -1;
    }

    if (n == 0) {
	// error, EOF?
	return -1;
    }

    chdr = CMSG_FIRSTHDR(&hdr);
    memmove(&fd, CMSG_DATA(chdr), sizeof(int));

    return fd;
}

int main () {
// initialize logging
// daemonize
// Create command unix domain socket.
}
