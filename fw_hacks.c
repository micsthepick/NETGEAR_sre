#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <semaphore.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <pthread.h>

#ifndef DEBUG_PRINTENV
#define DEBUG_PRINTENV 0
#endif

#ifndef DEBUG_PRINTARG
#define DEBUG_PRINTARG 1
#endif

#ifndef FWHACKS_OUTPUT_PATH
#define FWHACKS_OUTPUT_PATH "/dev/fw_hacks_con"
#endif

#ifndef FWHACKS_PRINT_SEM_PATH
#define FWHACKS_PRINT_SEM_PATH "/ntgr_hax_print_sem"
#endif

#define DUMMY_CONSOLE (FILE*)0x636f6e
 
#define DECL_INJECT(typ, f) static typ (*real_##f)() = NULL

DECL_INJECT(int, __libc_start_main);
DECL_INJECT(int, main);

// required injeets
DECL_INJECT(int, open);
DECL_INJECT(int, printf);
DECL_INJECT(int, fprintf);
DECL_INJECT(int, vprintf);
DECL_INJECT(int, vfprintf);
DECL_INJECT(int, execve);
DECL_INJECT(int, connect);
DECL_INJECT(int, bind);
DECL_INJECT(int, stat);
DECL_INJECT(FILE*, fopen);
DECL_INJECT(int, strlen);
DECL_INJECT(int, strcmp);
DECL_INJECT(int, strncmp);
DECL_INJECT(char*, strdup);
DECL_INJECT(char*, strerror);

// optional injects
DECL_INJECT(int, dni_strcmp_s);
DECL_INJECT(int, dni_strnlen_s);

sem_t* sem_acquire()
{
    sem_t* sem = sem_open(FWHACKS_PRINT_SEM_PATH, O_CREAT, 0666, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 2;
    if (sem_timedwait(sem, &ts) == -1) {
        real_fprintf(stderr, "SEMAPHORE ERROR: unlinking %s\n", FWHACKS_PRINT_SEM_PATH);
        if (sem_unlink(FWHACKS_PRINT_SEM_PATH) == -1) {
            perror("FATAL: couldn't remove sem!");
            exit(4);
        }
        exit(3);
    }
    return sem;
}

void sem_done(sem_t* sem)
{
    sem_post(sem);
    sem_close(sem);
}

int S(const char * file_desc, FILE * file, const char * format, va_list args)
{
    // aquire semphore for FIFO
    sem_t* sem = sem_acquire();
    int fifo = real_open(FWHACKS_OUTPUT_PATH, O_WRONLY);
    
    // output to FIFO
    dprintf(fifo, "%s: %d: ", file_desc, getpid());
    vdprintf(fifo, format, args);
    dprintf(fifo, "\n");

    // release semaphore
    close(fifo);
    sem_done(sem);

    int res = 0;
    if (file != DUMMY_CONSOLE) real_vfprintf(file, format, args);

    return res;
}

int P(const char * format, ...)
{
    va_list args;
    va_start(args, format);

    // aquire semphore for FIFO
    sem_t* sem = sem_acquire();
    int fifo = real_open(FWHACKS_OUTPUT_PATH, O_WRONLY);
    
    // output to FIFO
    int res = dprintf(fifo, "fw_hacks: %d: ", getpid());
    res = vdprintf(fifo, format, args) && res;

    // release semaphore
    close(fifo);
    sem_done(sem);

    va_end(args);

    return res;
}

#define SETUP_INJECT(f) (real_##f = dlsym(RTLD_NEXT, #f))
#define INJECT_AND_CHECK(f) (SETUP_INJECT(f), (real_##f == 0) ? (P("%s failed to inject!\n", #f), 0) : 1)

typedef struct stat stat_t;

static int is_injected = 0;
static int enable_noisy = 0;

static char *progname = NULL;

#define progname_safe (progname ? progname : "unknown program!")


char* SS(const char* s)
{
    if (!s) {
       return "<NULL>";
    };
    if (real_strcmp(s, "<NULL>") == 0) {
        return "this string used to be <NULL> including the braces";
    };
    return (char*)s;
}

int startswith(const char* str, const char* start)
{
    return real_strncmp(str, start, real_strlen(start));
}

void checkerror()
{
    if (errno) P("errno: %d - %s\n", errno, real_strerror(errno));
}

void dbgprintstrp(char* const* strp, char * pre)
{
    size_t strp_size = 0;
    while (strp && strp[strp_size]) {
        P("%s: %s\n", pre, strp[strp_size]);
        strp_size++;
    }
}

void dbgprintenv(char* const* envp)
{
    dbgprintstrp(envp, "env var");
}

void dbgprintargv(char* const* argv)
{
    dbgprintstrp(argv, "argument");
}

void load_env_config(char** envp)
{
    if (!envp) return;
    for (; *envp != NULL; envp++) {
        if (!startswith(*envp, "FHACKS_NOISE")) enable_noisy=1;
    }
    if (enable_noisy) {
        P("MAKING LOUD NOISES!\n");
    }
}

int main_hook(int argc, char** argv, char** envp)
{
    int res = -1;
#if DEBUG_PRINTENV
    dbgprintenv(envp);
#endif
#if DEBUG_PRINTARG
    dbgprintargv(argv);
#endif
    load_env_config(envp);
    if (real_main) {
	if (is_injected) {
            res = real_main(argc, argv, envp);
	}
    } else {
         P("real main not found\n");
	 res = -1337;
    }
    return res;
}

void sanitize_path(char* new_path, const char* pathname)
{
    // keep the new_path <= the pathname in size if possible
    if (pathname == NULL) {
        return;
    }
    if ('/' == *pathname) {
        while ('/' == pathname[1]) { pathname++; }
    }
    int doprint = 1;
    if (real_strncmp(pathname, "/proc/mtd", 9) == 0 && (pathname[9] == '\0' || pathname[9] == '/')) {
        sprintf(new_path, "/mtd%s", pathname + 9);
    } else if (real_strncmp(pathname, "/proc/device-tree", 17) == 0) {
        sprintf(new_path, "/device-tree%s", pathname + 5);
    } else {
	    doprint = 0;
	    sprintf(new_path, "%s", pathname);
    }
    if (doprint || enable_noisy) P("SANITIZE_PATH: %s -> %s\n", pathname, new_path);
}

int open(const char *pathname, int flags, ...)
{
    P("intercepted open(%s,...) called by %s\n", SS(pathname), progname);

    if (!pathname) return real_open(pathname, flags);

    char *new_path = calloc(strlen(pathname), sizeof(char));
    sanitize_path(new_path, pathname);

    if (real_strncmp(pathname, "/dev/mtdpath", 12) == 0) {
        // Simulate opening a character device
        P("Pretending to open character device: %s\n", pathname);
        return dup(0);  // Return a dummy valid file descriptor (stdin)
    }

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

    checkerror();

    free(new_path);
    return fd;
}

long ptrace(int request, int pid, void *addr, void *data)
{
    P("intercepted ptrace(request={%d},pid={%d},addr={%p},data={%p}) called by %s\n", request, pid, addr, data, progname_safe);
    return 0;
}


int stat(const char *path, stat_t * buf)
{
    P("intercepted stat(%s, %p) called by %s\n", SS(path), buf, progname_safe);

    if (!path) return real_stat(path, buf);

    char *new_path = calloc(strlen(path), sizeof(char));
    sanitize_path(new_path, path);

    int res = real_stat(new_path, buf);

    if (real_strncmp(path, "/dev/mtdpath", 12) == 0) {
        P("Pretending to see a character device: %s\n", path);
        buf->st_mode = S_IFCHR | S_ISGID | 0666;
        buf->st_rdev = makedev(90, 0);
        buf->st_dev = makedev(0, 0);
        buf->st_nlink = 1;
        buf->st_uid = getuid();
        buf->st_gid = getgid();
    }
    checkerror();

    free(new_path);
    return res;
}

int vfprintf(FILE * file, const char * format, va_list args)
{
    int res = 0;
    if (file == stderr) {
        res = S("stderr", file, format, args);
    }
    else if (file == stdout) {
        res = S("stdout", file, format, args);
    }
    else if (file == DUMMY_CONSOLE) {
        res = S("console", file, format, args);
    }
    return res;
}

int fputc(int c, FILE* file) 
{
    va_list args = {};
    char str[2] = {(unsigned char)c, 0};
    int res = vfprintf(file, str, args);
    va_end(args);
    return res;
}

int vprintf(const char * format, va_list args)
{
    return vfprintf(stdout, format, args);
}

int fprintf(FILE * file, const char * format, ...) {
    va_list args;
    va_start(args, format);
    int res = 0;
    vfprintf(file, format, args);
    va_end(args);
    return res;
}

int printf(const char * format, ...) {
    va_list args;
    va_start(args, format);
    int res = vprintf(format, args);
    va_end(args);
    return res;
}


FILE * fopen(const char *filename, const char *modes)
{
    if (enable_noisy) {
        P("intercepted fopen(%s, %s) called by %s\n", SS(filename), SS(modes), progname_safe);
    }

    if (!filename) return real_fopen(filename, modes);
    char *new_path = calloc(strlen(filename), sizeof(char));
    sanitize_path(new_path, filename);

    if (real_strcmp(new_path, "/dev/console") == 0) {
        free(new_path);
        return DUMMY_CONSOLE;
        // return 'con' - don't open anything
    }

    FILE *res = real_fopen(new_path, modes);
    free(new_path);

    checkerror();

    return res;
}

void decode_sockaddr(const void* addr, socklen_t len)
{
    if (!addr || len < sizeof(sa_family_t)) {
        P("connect() Invalid sockaddr (null or too small)\n");
        return;
    }

    const sa_family_t* family = (const sa_family_t*)addr;

    switch (*family) {
        case AF_INET: {
            struct sockaddr_in *a = (struct sockaddr_in *)addr;
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(a->sin_addr), ip, sizeof(ip));
            P("connect: IPv4 to %s:%d\n", ip, ntohs(a->sin_port));
            break;
        }
        case AF_INET6: {
            struct sockaddr_in6 *a6 = (struct sockaddr_in6 *)addr;
            char ip6[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &(a6->sin6_addr), ip6, sizeof(ip6));
            P("connect: IPv6 to [%s]:%d\n", ip6, ntohs(a6->sin6_port));
            break;
        }
        case AF_UNIX: {
            struct sockaddr_un *u = (struct sockaddr_un *)addr;
            P("connect: UNIX socket path: %s\n", u->sun_path);
            break;
        }
        default:
            P("connect() Unknown or unsupported sockaddr family: %d\n", *family);
    }
}

int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
    P("intercepted connect(%d, %p, %p) called by %s\n", sockfd, addr, addrlen, progname_safe);
    decode_sockaddr(addr, addrlen); 

    int res = real_connect(sockfd, addr, addrlen);

    checkerror();

    return res;
}

int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
    P("intercepted bind(%d, %p, %p) called by %s\n", sockfd, addr, addrlen, progname_safe);
    decode_sockaddr(addr, addrlen); 

    int res = real_bind(sockfd, addr, addrlen);

    checkerror();

    return res;
}

int strcmp(char* dest, char* src)
{
    if (enable_noisy) {
        P("intercepted strcmp(dest='%.64s', src='%.64s') called by %s\n", SS(dest), SS(src), progname_safe);
    }
    return real_strcmp(dest, src);
}

int strncmp(char* dest, char* src, unsigned int n)
{
    if (enable_noisy) {
        P("intercepted strncmp(dest='%.64s', src='%.64s', n=%u) called by %s\n", SS(dest), SS(src), n, progname_safe);
    }
    return real_strncmp(dest, src, n);
}

int dni_strnlen_s (char* func, unsigned int lineno, char * dest, unsigned int dmax)
{
    if (enable_noisy) {
        P("intercepted dni_strnlen_s(caller=%s, call_lineno=%u, dest='%.64s', dmax=%u) called by %s\n", func, lineno, SS(dest), dmax, progname_safe);
    }
    return real_dni_strnlen_s(func, lineno, dest, dmax);
}

int dni_strcmp_s(char* func, unsigned int lineno, char * dest, unsigned int dmax, char * src)
{
    if (enable_noisy) {
        P("intercepted dni_strcmp_s(caller=%s, call_lineno=%u, dest='%.64s', dmax=%u, src='%.64s') called by %s\n", func, lineno, SS(dest), dmax, SS(src), progname_safe);
    }
    return real_dni_strcmp_s(func, lineno, dest, dmax, src);
}

int __libc_start_main(
        int (*main_orig)(int, char **, char **),
        int argc,
        char **argv,
        void (*fini)(void),
        void (*rtld_fini)(void),
        void *stack_end) {
    // we need thsese injected to start printing
    SETUP_INJECT(vfprintf);
    SETUP_INJECT(vprintf);
    SETUP_INJECT(fprintf);
    SETUP_INJECT(printf);
    SETUP_INJECT(open);
    P("__libc_start_main()\n");
    real___libc_start_main = dlsym(RTLD_NEXT,"__libc_start_main");
    if (!real___libc_start_main ) {
        P("cannot inject orig libc start main!!!");
	return -1337;
    }
    P("Injecting mandatory funcs.\n");
    if (
        INJECT_AND_CHECK(fopen)
        & INJECT_AND_CHECK(execve)
        & INJECT_AND_CHECK(connect)
        & INJECT_AND_CHECK(bind)
        & INJECT_AND_CHECK(stat)
        & INJECT_AND_CHECK(strlen)
        & INJECT_AND_CHECK(strcmp)
        & INJECT_AND_CHECK(strncmp)
        & INJECT_AND_CHECK(strdup)
        & INJECT_AND_CHECK(strerror)
    ) {
        is_injected = 1;
        if (argc && argv && *argv) {
            P("Injection sucess on %s\n", *argv);
	    progname = real_strdup(*argv);
        } else {
            P("?? Injected without argv[0] or argc == 0\n");
        }
    } else {
        P("Injection failed\n");
        is_injected = 0;
    }
    P("Injecting optional.\n");
    INJECT_AND_CHECK(dni_strcmp_s);
    INJECT_AND_CHECK(dni_strnlen_s);
    real_main = main_orig;
    int res = real___libc_start_main(main_hook, argc, argv, fini, rtld_fini, stack_end);
    if (progname) free(progname);
    return res;
}
