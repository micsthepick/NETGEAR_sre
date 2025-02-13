#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <fcntl.h>

#ifndef DEBUG_PRINTENV
#define DEBUG_PRINTENV 0
#endif

#define P_TARGET stderr
#define P(...) (fprintf(P_TARGET, "fw_hacks: "), fprintf(P_TARGET, __VA_ARGS__))

#define SETUP_INJECT(f) (real_##f = dlsym(RTLD_NEXT, #f), (real_##f == 0) ? (P("%s failed to inject!\n", #f), 0) : 1)

typedef struct stat stat_t;

static int (*real_main)() = NULL;
static int (*real_open)() = NULL;
static int (*real_execve)() = NULL;
static int (*real___stat_time64)(const char *, struct stat *) = NULL;
static int (*real___libc_start_main)() = NULL;

static int is_injected = 0;

static char *progname = NULL;

#define progname_safe (progname ? progname : "unknown program!")
//static int (*real___xstat)(int, const char *, stat_t *) = NULL;
//static int (*real___lxstat)(int, const char *, stat_t *) = NULL;


void dbgprintenv(char* const* envp) {
    size_t env_size = 0;
    while (envp && envp[env_size]) {
        P("EXECVE env: %s\n", envp[env_size]);
        env_size++;
    }
}

int main_hook(int argc, char** argv, char** envp) {
    int res = -1;
#if DEBUG_PRINTENV
    dbgprintenv(envp);
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
    if (strncmp(pathname, "/proc/mtd", 9) == 0 && (pathname[9] == '\0' || pathname[9] == '/')) {
        sprintf(new_path, "/mtd%s", pathname + 5);
    } else if (strncmp(pathname, "/proc/device-tree", 17) == 0) {
        sprintf(new_path, "/device-tree%s", pathname + 5);
    } else {
	    sprintf(new_path, "%s", pathname);
    }
    P("SANITIZE_PATH: %s -> %s\n", pathname, new_path);
}

int open(const char *pathname, int flags, ...) {
    P("OPEN(%s,...) called by %s\n", pathname ? pathname : "<NULL>", progname);

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

#if 1

int __stat_time64(const char *path, stat_t * buf) {
    P("intercepted STAT(%s, %p)  called by %s\n", path ? path : "<NULL>", buf, progname_safe);
    fflush(stdout);

    if (!path) return real___stat_time64(path, buf);

    char *new_path = calloc(strlen(path), sizeof(char));
    sanitize_path(new_path, path);

    int res = real___stat_time64(new_path, buf);

    free(new_path);
    return res;
}
#endif


# if 0
int statx(int dirfd, const char *restrict path, int flags, unsigned mask, statx_t *restrict stx) {
    P("intercepted statx(%s)\n", path ? path : "<NULL>");
    char *new_path = calloc(strlen(path), sizeof(char));
    int res;
    if (path) {
        sanitize_path(new_path, path);
	res = real_statx(dirfd, new_path, flags, mask, stx);
    } else {
        res = real_statx(dirfd, path, flags, mask, stx);
    }
    free(new_path);
    return res;
}

int __lxstat(int ver, const char *path, struct stat *buf) {
    P("intercepted __xstat(%s)\n", path ? path : "<NULL>"); 
    char *new_path = calloc(strelen(path), sizeof(char));
    if (path) sanitize_path(new_path, path);
    int res = real___lxstat(ver, path ? new_path : NULL, buf);
    free(new_path);
    return res;
}

int __xstat(int ver, const char *path, struct stat *buf) {
    P("intercepted __xstat(%s)\n", path ? path : "<NULL>"); 
    char *new_path = calloc(strlen(path), sizeof(char));
    if (path) sanitize_path(new_path, path);
    int res = real___xstat(ver, path ? new_path : NULL, buf);
    free(new_path);
    return res;
}
#endif

int execve(const char *pathname, char *const argv[], char *const envp[]) {
    P("intercepted EXECVE: %s\n", pathname ? pathname : "<NULL>");

    const char *ld_preload = "LD_PRELOAD=/fw_hacks.so";
#if DEBUG_PRINTENV
    dbgprintenv(envp);
#endif

#if 0
    char **new_envp = calloc(env_size + 2, sizeof(char *));
    if (!new_envp) {
        perror("calloc failed");
        return -1;
    }

    for (size_t i = 0; i < env_size; i++) {
        new_envp[i] = envp[i];
    }
    new_envp[env_size] = strdup(ld_preload);
    if (!new_envp[env_size]) {
        perror("strdup failed");
        free(new_envp);
        return -1;
    }

    int result = real_execve(pathname, argv, new_envp);
    perror("execve failed");

    free(new_envp[env_size]);
    free(new_envp);
#endif
    int result = real_execve(pathname, argv, envp);
    return result;
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
