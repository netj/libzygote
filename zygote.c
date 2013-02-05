/*
 * libzygote -- Zygote Process Library
 *
 * Author: Jaeho Shin <netj@cs.stanford.edu>
 * Created: 2013-02-05
 *
 * See-Also: http://www.thomasstover.com/uds.html
 * See-Also: http://lists.canonical.org/pipermail/kragen-hacks/2002-January/000292.html
 * See-Also: https://github.com/martylamb/nailgun/blob/master/nailgun-client/ng.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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

extern char* *environ;

#include "zygote.h"


// read_fd/write_fd taken from Unix Network Programming
// See-Also: http://stackoverflow.com/a/2358843/390044
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



typedef int (*run_func_t)(int, char* [], int, void* []);

static int grow_this_zygote(int connection_fd, int objc, void* objv[]) {
    int i;
    int fds[3];
    int argc;
    char* *argv;
    char* code_path;
    char* *env;
    void* handle;
    run_func_t run_func;
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

    FILE *orig_stderr = fdopen(dup(2), "w");
#define log(args...) \
    fprintf(orig_stderr, args)

    // verify libzygote version
    recvNum(version);
    if (num != ZYGOTE_VERSION) {
        fprintf(stderr, "Fatal: expected version %d, but got %d\n", ZYGOTE_VERSION, num);
        goto error;
    }

    // replace environ
    recvNum(envc);
    env = environ = (char* *) malloc((num + 1) * sizeof(char*));
    for (i=num; i>0; i--, env++) {
        recvStr(environ_i); *env = strdup(buf);
    }
    *env = NULL;

    // chdir to cwd
    recvStr(cwd); chdir(buf);
    log("zygote: cd %s\n", buf);

    // get argc, argv
    recvNum(argc); argc = num;
    argv = (char* *) malloc(argc * sizeof(char*));

    // get code_path
    recvStr(argv_0); code_path = argv[0] = strdup(buf);
    log("zygote: %s", code_path);

    for (i=1; i<argc; i++) {
        recvStr(argv_i); argv[i] = strdup(buf);
        log(" %s", buf);
    }
    log("\n");

    // dynamically load the code
    handle = dlopen(code_path, RTLD_LAZY);
    if (handle == NULL) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        goto error;
    }
    dlerror();
    run_func = (run_func_t) dlsym(handle, "run");
    if ((error = dlerror()) != NULL) {
        fprintf(stderr, "dlsym: %s\n", error);
        goto error;
    }

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
    num = run_func(argc, argv, objc, objv);

    dlclose(handle);

    // send back return code
    if (write(connection_fd, &num, sizeof(num)) == -1) { perror("exitcode write"); goto error; }

    return num;

error:
    num = -1;
    write(connection_fd, &num, sizeof(num));
    close(connection_fd);
    return -1;
}



static int   zygote_socket_fd = -1;
static char* zygote_socket_path = NULL;
static void cleanup(void) {
    if (zygote_socket_fd != -1)
        close(zygote_socket_fd);
    if (zygote_socket_path != NULL)
        unlink(zygote_socket_path);
}

int run_multiple(char* socket_path, ...) {
    char argv0_orig[BUFSIZ];
    char argv0_new[BUFSIZ];
    int objc, i;
    va_list ap;
    void* *objv;
    struct sockaddr_un address = {0};
    socklen_t address_length;
    int socket_fd;

    if (strlen(socket_path) >= sizeof(address.sun_path)) {
        perror("run_multiple");
        return -1;
    }

    // prepare objc, objv from varargs
    objc = 0;
    va_start(ap, socket_path); objc++; va_end(ap);
    objv = (void* *) malloc(objc * sizeof(void *));
    va_start(ap, socket_path);
    for (i=0; i<objc; i++)
        objv[i] = va_arg(ap, void *);
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


int run_once(char* socket_path, ...) {
    char* argv[] = {""};
    int objc, i;
    va_list ap;
    void* *objv;

    // prepare objc, objv from varargs
    objc = 0;
    va_start(ap, socket_path); objc++; va_end(ap);
    objv = (void* *) malloc(objc * sizeof(void *));
    va_start(ap, socket_path);
    for (i=0; i<objc; i++)
        objv[i] = va_arg(ap, void *);
    va_end(ap);

    // pass arguments to the linked run()
    return run(1, argv, objc, objv);
}
