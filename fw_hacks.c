#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netinet/in.h>

#ifndef DEBUG_PRINTENV
#define DEBUG_PRINTENV 0
#endif

#ifndef DEBUG_PRINTARG
#define DEBUG_PRINTARG 1
#endif

#define P_TARGET stderr
#define P(...) (fprintf(P_TARGET, "fw_hacks: "), fprintf(P_TARGET, __VA_ARGS__))
#define SS(s) (s ? s : "<NULL>")

#define SETUP_INJECT(f) (real_##f = dlsym(RTLD_NEXT, #f), (real_##f == 0) ? (P("%s failed to inject!\n", #f), 0) : 1)

typedef struct stat stat_t;

static int (*real_main)() = NULL;
static int (*real_open)() = NULL;
static int (*real_execve)() = NULL;
static int (*real_connect)() = NULL;
static int (*real___stat_time64)(const char *, struct stat *) = NULL;
static int (*real___libc_start_main)() = NULL;
static FILE* (*real_fopen)() = NULL;

static int is_injected = 0;

static char *progname = NULL;

#define progname_safe (progname ? progname : "unknown program!")


void dbgprintstrp(char* const* strp, char * pre) {
    size_t strp_size = 0;
    while (strp && strp[strp_size]) {
        P("%s: %s\n", pre, strp[strp_size]);
        strp_size++;
    }
}

void dbgprintenv(char* const* envp) {
    dbgprintstrp(envp, "env var");
}

void dbgprintargv(char* const* argv) {
    dbgprintstrp(argv, "argument");
}

int main_hook(int argc, char** argv, char** envp) {
    int res = -1;
#if DEBUG_PRINTENV
    dbgprintenv(envp);
#endif
#if DEBUG_PRINTARG
    dbgprintargv(argv);
#endif
    if (real_main) {
	if (is_injected) {
            res = real_main(argc, argv, envp);
	}
    } else {
         P("real main not found\n");
	 res = -1337;
    }
    if (progname) free(progname);
    return res;
}

void sanitize_path(char* new_path, const char* pathname) {
    // keep the new_path <= the pathname in size if possible
    if (pathname == NULL) {
        return;
    }
    if ('/' == *pathname) {
        while ('/' == pathname[1]) { pathname++; }
    }
    int doprint = 1;
    if (strncmp(pathname, "/proc/mtd", 9) == 0 && (pathname[9] == '\0' || pathname[9] == '/')) {
        sprintf(new_path, "/mtd%s", pathname + 9);
    } else if (strncmp(pathname, "/proc/device-tree", 17) == 0) {
        sprintf(new_path, "/device-tree%s", pathname + 5);
    } else {
	    doprint = 0;
	    sprintf(new_path, "%s", pathname);
    }
    if (doprint) P("SANITIZE_PATH: %s -> %s\n", pathname, new_path);
}

int open(const char *pathname, int flags, ...) {
    P("OPEN(%s,...) called by %s\n", SS(pathname), progname);

    if (!pathname) return real_open(pathname, flags);

    char *new_path = calloc(strlen(pathname), sizeof(char));
    sanitize_path(new_path, pathname);

    int fd;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list args;
        va_start(args, flags);
        mode_t mode = va_arg(args, mode_t);
        fd = real_open(new_path, flags, mode);
        va_end(args);
    } else {
        fd = real_open(new_path, flags);
    }

    free(new_path);
    return fd;
}

long ptrace(int request, int pid, void *addr, void *data) {
    P("intercepted PTRACE_request{%d}_pid{%d}_addr{%p}_data{%p} called by %s\n", request, pid, addr, data, progname_safe);
    return 0;
}


int __stat_time64(const char *path, stat_t * buf) {
    P("intercepted STAT(%s, %p) called by %s\n", SS(path), buf, progname_safe);
    fflush(stdout);

    if (!path) return real___stat_time64(path, buf);

    char *new_path = calloc(strlen(path), sizeof(char));
    sanitize_path(new_path, path);

    int res = real___stat_time64(new_path, buf);

    free(new_path);
    return res;
}

FILE * fopen(const char *filename, const char *modes) {
    P("intercepted FOPEN(%s, %s) called by %s\n", SS(filename), SS(modes), progname_safe);

    if (!filename) return real_fopen(filename, modes);
    char *new_path = calloc(strlen(filename), sizeof(char));
    sanitize_path(new_path, filename);

    FILE *res = real_fopen(filename, modes);
    free(new_path);

    return res;
}

void decode_sockaddr(const void* addr, socklen_t len) {
    if (!addr || len < sizeof(sa_family_t)) {
        P("Invalid sockaddr (null or too small)\n");
        return;
    }

    P("connect - TESTTEMP\n");

    const sa_family_t* family = (const sa_family_t*)addr;

    if (*family == AF_INET && len >= sizeof(struct sockaddr_in)) {
        const struct sockaddr_in* sin = (const struct sockaddr_in*)addr;
        char ip[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &(sin->sin_addr), ip, sizeof(ip));
        uint16_t port = ntohs(sin->sin_port);
        P("connect - Decoded sockaddr: IPv4 %s:%d\n", ip, port);
    } else if (*family == AF_UNIX && len >= sizeof(sa_family_t) + 1) {
        const struct sockaddr_un* sun = (const struct sockaddr_un*)addr;
        P("connect - Decoded sockaddr: UNIX socket path: %s\n", sun->sun_path);
    } else {
        P("connect - Unknown or unsupported sockaddr family: %d\n", *family);
    }
}

int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    P("connect - test");
    P("intercepted CONNECT(%d, %p, %p) called by %s\n", sockfd, addr, addrlen, progname_safe);
    decode_sockaddr(addr, addrlen); 

    return real_connect(sockfd, addr, addrlen);
}

int __libc_start_main(
        int (*main_orig)(int, char **, char **),
        int argc,
        char **argv,
        void (*fini)(void),
        void (*rtld_fini)(void),
        void *stack_end) {
    P("__libc_start_main()\n");
    real___libc_start_main = dlsym(RTLD_NEXT,"__libc_start_main");
    if (!real___libc_start_main ) {
        P("cannot inject orig libc start main!!!");
	return -1337;
    }
    P("Injecting funcs.\n");
    if (
        SETUP_INJECT(open)
        & SETUP_INJECT(execve)
        & SETUP_INJECT(__stat_time64)
        & SETUP_INJECT(fopen)
	& SETUP_INJECT(connect)
    ) {
        is_injected = 1;
        if (argc && argv && *argv) {
            P("Injection sucess on %s\n", *argv);
	    progname = strdup(*argv);
        } else {
            P("?? Injected without argv[0] or argc == 0\n");
        }
    } else {
        P("Injection failed\n");
        is_injected = 0;
    }
    real_main = main_orig;
    return real___libc_start_main(main_hook, argc, argv, fini, rtld_fini, stack_end);
}
