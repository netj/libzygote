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
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif

#define log(args...) \
    fprintf(stderr, args)

typedef int (*run_t)(int objc, void* objv[], int argc, char* argv[]);

extern char* *environ;

#include "zygote.h"

static char objvStr[BUFSIZ];

// read_fd/write_fd taken from Unix Network Programming
// See-Also: http://stackoverflow.com/a/2358843/390044
// See-Also: http://www.thomasstover.com/uds.html
// See-Also: http://lists.canonical.org/pipermail/kragen-hacks/2002-January/000292.html
#define HAVE_MSGHDR_MSG_CONTROL
static ssize_t read_fd(int fd, void *ptr, size_t nbytes, int *recvfd) {
    struct msghdr   msg;
    struct iovec    iov[1];
    ssize_t         n;

#ifdef  HAVE_MSGHDR_MSG_CONTROL
    union {
      struct cmsghdr    cm;
      char              control[CMSG_SPACE(sizeof(int))];
    } control_un;
    struct cmsghdr  *cmptr;

    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);
#else
    int             newfd;

    msg.msg_accrights = (caddr_t) &newfd;
    msg.msg_accrightslen = sizeof(int);
#endif

    msg.msg_name = NULL;
    msg.msg_namelen = 0;

    iov[0].iov_base = ptr;
    iov[0].iov_len = nbytes;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    if ( (n = recvmsg(fd, &msg, 0)) <= 0)
        return(n);

#ifdef  HAVE_MSGHDR_MSG_CONTROL
    if ( (cmptr = CMSG_FIRSTHDR(&msg)) != NULL &&
        cmptr->cmsg_len == CMSG_LEN(sizeof(int))) {
        if (cmptr->cmsg_level != SOL_SOCKET)
            return(n); //err_quit("control level != SOL_SOCKET");
        if (cmptr->cmsg_type != SCM_RIGHTS)
            return(n); //err_quit("control type != SCM_RIGHTS");
        *recvfd = *((int *) CMSG_DATA(cmptr));
    } else
        *recvfd = -1;       /* descriptor was not passed */
#else
/* *INDENT-OFF* */
    if (msg.msg_accrightslen == sizeof(int))
        *recvfd = newfd;
    else
        *recvfd = -1;       /* descriptor was not passed */
/* *INDENT-ON* */
#endif

    return(n);
}
/* end read_fd */


#ifdef _BSD_SOURCE
#define HAS_ON_EXIT
#endif
#ifdef HAS_ON_EXIT
// Try to reply with the final exit status if possible
// See: http://www.gnu.org/software/libc/manual/html_mono/libc.html#Cleanups-on-Exit
static int grow_connection_fd;
static void replyWithExitStatus(int status, void* arg) {
    if (grow_connection_fd != -1)
        write(grow_connection_fd, &status, sizeof(status));
}
#endif /* HAS_ON_EXIT */

// See-Also: https://github.com/martylamb/nailgun/blob/master/nailgun-client/ng.c
static int grow_this_zygote(int connection_fd, int objc, void* objv[]) {
    int i;
    int fds[3];
    int argc;
    char* *argv;
    char* code_path;
    char* *env;
    void* handle;
    run_t run;
    char* error;

    int num;
#define recvNum(NAME) \
    do { \
        if (read(connection_fd, &num, sizeof(num)) == -1) { perror(#NAME " read"); goto error; } \
    } while (0)

    char buf[BUFSIZ];
#define recvStr(NAME) \
    do { \
        recvNum(NAME length); \
        if (read(connection_fd, buf, num) == -1) { perror(#NAME " read"); goto error; } \
        buf[num] = '\0'; \
    } while (0)

    char logbuf[BUFSIZ];
#define resetLogBuf(args...) \
    snprintf(logbuf, sizeof(logbuf), args)
#define appendLogBuf(args...) \
    do { \
        num = strlen(logbuf); \
        snprintf(logbuf+num, sizeof(logbuf)-num, args); \
    } while (0)

    // verify libzygote version
    recvNum(version);
    if (num != ZYGOTE_VERSION) {
        fprintf(stderr, "zygote: FATAL: version mismatch, expected %d, but got %d\n", ZYGOTE_VERSION, num);
        goto error;
    }

    num = getpid();
    if (write(connection_fd, &num, sizeof(num)) == -1) { perror("pid write"); goto error; }

    // replace environ
    recvNum(envc);
    env = environ = (char* *) malloc((num + 1) * sizeof(char*));
    for (i=num; i>0; i--, env++) {
        recvStr(environ_i); *env = strdup(buf);
    }
    *env = NULL;

    // chdir to cwd
    recvStr(cwd); chdir(buf);
    resetLogBuf("zygote: cd %s\n", buf);

    // get argc
    recvNum(argc); argc = num;
    argv = (char* *) malloc(argc * sizeof(char*));

    // get code_path
    recvStr(argv_0); code_path = argv[0] = strdup(buf);
    appendLogBuf("zygote: %s: run( %s; ", code_path, objvStr);

    // get argv
    for (i=1; i<argc; i++) {
        recvStr(argv_i); argv[i] = strdup(buf);
        appendLogBuf("%s ", argv[i]);
    }
    appendLogBuf(");\n");

    // dynamically load the code
    handle = dlopen(code_path, RTLD_LAZY);
    if (handle == NULL) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        goto error;
    }
    dlerror();
    run = (run_t) dlsym(handle, "run");
    if ((error = dlerror()) != NULL) {
        fprintf(stderr, "dlsym: %s\n", error);
        goto error;
    }

    log("%s", logbuf);

    // receive and dup file descriptors
    if (read_fd(connection_fd, buf, 1, fds+2) == -1) { perror("stderr read_fd"); goto error; }
    if (read_fd(connection_fd, buf, 1, fds+1) == -1) { perror("stdout read_fd"); goto error; }
    if (read_fd(connection_fd, buf, 1, fds+0) == -1) { perror("stdin  read_fd"); goto error; }
    for (i=0; i<3; i++)
        if (dup2(fds[i], i) == -1) {
            perror("dup2");
            goto error;
        }

    // actually run the code
    num = run(objc, objv, argc, argv);

    dlclose(handle);

    // send back return code when this process exits
#ifdef HAS_ON_EXIT
    grow_connection_fd = connection_fd;
    on_exit(replyWithExitStatus, NULL);
#else /* HAS_ON_EXIT */
    if (write(connection_fd, &num, sizeof(num)) == -1) { perror("exitcode write"); goto error; }
#endif /* HAS_ON_EXIT */

    return num;

error:
    num = EXIT_FAILURE;
    write(connection_fd, &num, sizeof(num));
    close(connection_fd);
    exit(num);
}



static int   zygote_socket_fd = -1;
static char* zygote_socket_path = NULL;
static void cleanup(void) {
    if (zygote_socket_fd != -1)
        close(zygote_socket_fd);
    if (zygote_socket_path != NULL)
        unlink(zygote_socket_path);
}

int zygote(char* socket_path, ...) {
    struct sockaddr_un address = {0};
    socklen_t address_length;
    int socket_fd;
    va_list ap;
    int objc, i, num;
    void* *objv;
    char socket_path_real[PATH_MAX];
#ifdef __linux__
    char argv0_orig[BUFSIZ];
    char argv0_new[BUFSIZ];
#endif

    if (strlen(socket_path) >= sizeof(address.sun_path)) {
        perror("wait_as_zygote");
        return -1;
    }

    // prepare objc, objv from varargs
    objc = 0;
    va_start(ap, socket_path); while (va_arg(ap, void *) != NULL) objc++; va_end(ap);
    objv = (void* *) malloc(objc * sizeof(void *));
    va_start(ap, socket_path);
    for (i=0; i<objc; i++) {
        objv[i] = va_arg(ap, void *);
        num = strlen(objvStr);
        snprintf(objvStr+num, sizeof(objvStr)-num, "%p ", objv[i]);
    }
    va_end(ap);

    // open a PF_UNIX SOCK_DGRAM socket bound to socket_path
    if (unlink(socket_path) < 0) {
        if (errno != ENOENT) {
            fprintf(stderr, "%s: ", socket_path);
            perror("unlink");
            return -1;
        }
    }
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, socket_path);
    socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket");
        return -1;
    }
    if (bind(socket_fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        perror("bind");
        close(socket_fd);
        return -1;
    }
    if (listen(socket_fd, 5) != 0) {
        perror("listen");
        return -1;
    }
    // reap before children become zombies
    signal(SIGCHLD, SIG_IGN);
    // cleanup before exiting
    zygote_socket_fd   = socket_fd;
    zygote_socket_path = socket_path;
    atexit(cleanup);
#ifdef __linux__
    // mark this process as a zygote in its name
    prctl(PR_GET_NAME, (unsigned long) argv0_orig, 0, 0, 0);
    sprintf(argv0_new, "_%s (zygote)", argv0_orig);
    prctl(PR_SET_NAME, (unsigned long) argv0_new, 0, 0, 0);
#endif
    // listen to the socket
    if (realpath(socket_path, socket_path_real) == NULL)
        strcpy(socket_path_real, socket_path);
    log("zygote: listening to %s\n", socket_path_real);
    for (;;) {
        int connection_fd = accept(socket_fd, 
                        (struct sockaddr *) &address,
                        &address_length);
        if (connection_fd == -1)
            break;
        // fork with copy-on-write
        if (fork() == 0) {
            // make sure child doesn't do parent's jobs
#ifdef __linux__
            prctl(PR_SET_NAME, (unsigned long) argv0_orig, 0, 0, 0);
#endif
            zygote_socket_fd   = -1;
            zygote_socket_path = NULL;
            // and grow into a full process
            return grow_this_zygote(connection_fd, objc, objv);
        }
        close(connection_fd);
    }
    close(socket_fd);
    unlink(socket_path);
    return 0;
}

int zygote_skip(char* socket_path, ...) {
    char* argv[] = {""};
    va_list ap;
    int objc, i, num;
    void* *objv;
    void* handle;
    char* error;
    run_t run;

    // prepare objc, objv from varargs
    objc = 0;
    va_start(ap, socket_path); while (va_arg(ap, void *) != NULL) objc++; va_end(ap);
    objv = (void* *) malloc(objc * sizeof(void *));
    va_start(ap, socket_path);
    for (i=0; i<objc; i++) {
        objv[i] = va_arg(ap, void *);
        num = strlen(objvStr);
        snprintf(objvStr+num, sizeof(objvStr)-num, "%p ", objv[i]);
    }
    va_end(ap);

    log("zygote: not listening to %s\n", socket_path);
    log("zygote: run( %s; )\n", objvStr);

    // look for run in the current address space
    handle = dlopen(NULL, RTLD_LAZY);
    if (handle == NULL) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return -1;
    }
    dlerror();
    run = (run_t) dlsym(handle, "run");
    if ((error = dlerror()) != NULL) {
        fprintf(stderr, "dlsym: %s\n", error);
        return -1;
    }

    // pass arguments to the linked run()
    return run(objc, objv, 1, argv);
}
