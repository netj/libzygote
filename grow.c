/*
 * Copyright 2013 Jaeho Shin <netj@cs.stanford.edu>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * libzygote -- Zygote Process Library
 *
 * See: https://github.com/netj/libzygote/#readme
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/uio.h>

extern char* *environ;

#include "zygote.h"

// read_fd/write_fd taken from Unix Network Programming
// See-Also: http://stackoverflow.com/a/2358843/390044
#define HAVE_MSGHDR_MSG_CONTROL
static ssize_t write_fd(int fd, void *ptr, size_t nbytes, int sendfd) {
    struct msghdr   msg;
    struct iovec    iov[1];

#ifdef  HAVE_MSGHDR_MSG_CONTROL
    union {
      struct cmsghdr    cm;
      char              control[CMSG_SPACE(sizeof(int))];
    } control_un;
    struct cmsghdr  *cmptr;

    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);

    cmptr = CMSG_FIRSTHDR(&msg);
    cmptr->cmsg_len = CMSG_LEN(sizeof(int));
    cmptr->cmsg_level = SOL_SOCKET;
    cmptr->cmsg_type = SCM_RIGHTS;
    *((int *) CMSG_DATA(cmptr)) = sendfd;
#else
    msg.msg_accrights = (caddr_t) &sendfd;
    msg.msg_accrightslen = sizeof(int);
#endif

    msg.msg_name = NULL;
    msg.msg_namelen = 0;

    iov[0].iov_base = ptr;
    iov[0].iov_len = nbytes;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    return(sendmsg(fd, &msg, 0));
}
/* end write_fd */


#ifndef UNIX_PATH_MAX
/* uh-oh, nothing safe to do here */
#define UNIX_PATH_MAX sizeof(unix_socket_name.sun_path)
#endif

// See-Also: http://www.thomasstover.com/uds.html
// See-Also: https://github.com/martylamb/nailgun/blob/master/nailgun-client/ng.c
int main(int argc, char* argv[]) {
    struct sockaddr_un unix_socket_name = {0};
    int socket_fd;
    char* socket_path;
    int i;
    char* code_path;
    char* *env;

    int num;
#define sendNum(NAME, VALUE) \
    do { \
        num = VALUE; \
        if (write(socket_fd, &num, sizeof(num)) == -1) { perror(#NAME" write"); goto error; } \
    } while (0)

    char buf[BUFSIZ] = {0};
#define sendStr(NAME, PTR) \
    do { \
        sendNum(NAME length, strlen(PTR)); \
        if (write(socket_fd, PTR, num) == -1) { perror(#NAME " write"); goto error; } \
    } while (0)

    // check arguments
    if (argc < 3) {
        fprintf(stdout,
                "grow -- Feed a runnable to grow the libzygote process\n"
                "Usage: grow ZYGOTE_SOCKET_PATH RUNNABLE_SHARED_OBJECT_PATH [ARG]...\n"
                "\n"
                "For more info, see: https://github.com/netj/libzygote/#readme\n"
                );
        return 1;
    }
    socket_path = argv[1];
    code_path = argv[2];

    // open socket
    unix_socket_name.sun_family = AF_UNIX;
    if (strlen(socket_path) >= UNIX_PATH_MAX - 1) {
        fprintf(stderr, "%s: pathname too long", socket_path);
        return -1;
    }
    strcpy(unix_socket_name.sun_path, socket_path);
    socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket");
        return -1;
    }
    if (connect(socket_fd, (struct sockaddr*)&unix_socket_name, sizeof(unix_socket_name))) {
        perror("connect");
        close(socket_fd);
        socket_fd = -1;
        return -1;
    }

    // send libzygote version
    sendNum(version, ZYGOTE_VERSION);

    // send environ
    for (i = 0, env = environ; *env; env++)
        i++;
    sendNum(envc, i);
    for (env = environ; *env; env++) {
        sendStr(environ_i, *env);
    }

    // send cwd
    getcwd(buf, sizeof(buf));
    sendStr(cwd, buf);

    // send argc
    sendNum(argc, argc - 2);

    // send code_path
    sendStr(argv_0, code_path);

    // send argv
    for (i=3; i<argc; i++) {
        sendStr(argv_i, argv[i]);
    }

    // pass file descriptors
    if (write_fd(socket_fd, buf, 1, 2) == -1) { perror("stderr send_fd"); goto error; }
    if (write_fd(socket_fd, buf, 1, 1) == -1) { perror("stdout send_fd"); goto error; }
    if (write_fd(socket_fd, buf, 1, 0) == -1) { perror("stdin  send_fd"); goto error; }

    // get exit code
    if (read(socket_fd, &num, sizeof(num)) == -1) { perror("read"); goto error; }
    close(socket_fd);
    return num;

error:
    close(socket_fd);
    return -1;
}
