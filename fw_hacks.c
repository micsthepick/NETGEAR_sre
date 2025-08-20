#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <pthread.h>
#include <sys/file.h>

#ifndef DEBUG_PRINTENV
#define DEBUG_PRINTENV 0
#endif

#ifndef DEBUG_PRINTARG
#define DEBUG_PRINTARG 1
#endif

#ifndef FW_HACKS_ENV_VAR
#define FW_HACKS_ENV_VAR "LD_PRELOAD=/fw_hacks.so"
#endif

#ifndef FWHACKS_OUTPUT_PATH
#define FWHACKS_OUTPUT_PATH "/dev/fw_hacks_con"
#endif

#define MATHPATH 4096

#define DUMMY_CONSOLE (FILE*)0x636f6e
#define DEVCONSOLE "/dev/console"
#define DEVTTY "/dev/tty"
#define DUMMY_CONSOLE_FD 99

#define DECL_INJECT(typ, f) static typ (*real_##f)() = NULL

DECL_INJECT(int, __libc_start_main);
DECL_INJECT(int, main);

// required injeets
DECL_INJECT(int, open);
DECL_INJECT(int, close);
DECL_INJECT(ssize_t, read);
DECL_INJECT(int, printf);
DECL_INJECT(int, fprintf);
DECL_INJECT(int, vprintf);
DECL_INJECT(int, vfprintf);
DECL_INJECT(int, execve);
DECL_INJECT(int, connect);
DECL_INJECT(int, bind);
DECL_INJECT(int, stat);
DECL_INJECT(size_t, fwrite);
DECL_INJECT(FILE*, fopen);
DECL_INJECT(int, fputs);
DECL_INJECT(char*, fgets);
DECL_INJECT(int, fclose);
DECL_INJECT(int, strlen);
DECL_INJECT(int, strcmp);
DECL_INJECT(int, strncmp);
DECL_INJECT(char*, strncpy);
DECL_INJECT(char*, strdup);
DECL_INJECT(char*, strerror);
DECL_INJECT(ssize_t, sendto);
DECL_INJECT(ssize_t, recvfrom);

// optional injects
DECL_INJECT(int, dni_strcmp_s);
DECL_INJECT(int, dni_strnlen_s);

static int get_fw_hacks_fd()
{
    int fd = real_open(FWHACKS_OUTPUT_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror("FWHACKS: open output file");
        return -1;
    }
    return fd;
}

int fw_hacks_vfprintf(const char * file_desc, FILE * file, const char * format, va_list args)
{
    int fd = get_fw_hacks_fd();
    if (fd >= 0) {
        flock(fd, LOCK_EX);
        dprintf(fd, "%s: %d: ", file_desc, getpid());
        vdprintf(fd, format, args);
        flock(fd, LOCK_UN);
        real_close(fd);
    }

    int res = 0;
    if (DUMMY_CONSOLE == file) file = stdout;
    real_vfprintf(file, format, args);

    return res;
}

int fw_hacks_print(const char * format, ...)
{
    va_list args;
    va_start(args, format);

    int fd = get_fw_hacks_fd();
    int res = 0;
    if (fd >= 0) {
        flock(fd, LOCK_EX);
        res = dprintf(fd, "fw_hacks: %d: ", getpid());
        res = vdprintf(fd, format, args) && res;
        flock(fd, LOCK_UN);
        real_close(fd);
    }

    va_end(args);

    return res;
}

#define SETUP_INJECT(f) (real_##f = dlsym(RTLD_NEXT, #f))
#define INJECT_AND_CHECK(f) (SETUP_INJECT(f), (real_##f == 0) ? (fw_hacks_print("%s failed to inject!\n", #f), 0) : 1)

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
    if (0 == real_strcmp(s, "<NULL>")) {
        return "this string used to be <NULL> including the braces";
    };
    return (char*)s;
}

int startswith(const char* str, const char* start)
{
    return real_strncmp(str, start, real_strlen(start));
}

void checkerror(char* loc, char* data)
{
    char empty[1] = "";
    if (!data) data = empty;
    if (9 == errno && 0 == real_strcmp("close", loc) && atoi(data) >= 13 && (!enable_noisy)) return;  // hackily ignore silly repeating errors
    if (errno) fw_hacks_print("errno from %s(\"%s\"): %d - %s\n", loc, SS(data), errno, real_strerror(errno));
}

void dbgprintstrp(char* const* strp, char * pre)
{
    size_t strp_size = 0;
    while (strp && strp[strp_size]) {
        fw_hacks_print("%s: %s\n", pre, strp[strp_size]);
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
        if (!startswith(*envp, "FHACKS_NOISE"))
            enable_noisy = 1;
    }
    if (enable_noisy) {
        fw_hacks_print("MAKING LOUD NOISES!\n");
    }
}

int main_hook(int argc, char** argv, char** envp)
{
    fw_hacks_print("MAIN: %s\n", progname_safe);
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
         fw_hacks_print("real main not found\n");
     res = -1337;
    }
    return res;
}

void sanitize_path(char* new_path, const char* pathname)
{
    if (NULL == pathname) {
        return;
    }
    if ('/' == *pathname) {
        while ('/' == pathname[1]) { pathname++; }
    }
    int doprint = 1;
    if (0 == real_strncmp(pathname, "/proc/mtd", 9)) {
        if ('\0' == pathname[9]) {
            sprintf(new_path, "/mtd/info");
        } else {
            sprintf(new_path, "/mtd/mtd%s", pathname + 9);
        }
    } else if (0 == real_strncmp(pathname, "/proc/device-tree", 17)) {
        sprintf(new_path, "/device-tree%s", pathname + 5);
    } else if (0 == real_strncmp(pathname, "/proc/", 6)) {
        char * pathname_checker = (char*)pathname+6;
        while (*pathname_checker >= '0' && *pathname_checker <= '9')
        {
            pathname_checker++;
        }
        if (0 == real_strcmp(pathname_checker, "/as")) {
            char * new_path_i = new_path;
            for (char * c = (char *)pathname; c <= pathname_checker; c++) {
                *(new_path_i++) = *c;
            }
            *(new_path_i++) = 'm';
            *(new_path_i++) = 'e';
            *(new_path_i++) = 'm';
            *new_path_i = '\0';
        }
        else {
            sprintf(new_path, "%s", pathname);
        }
    } else {
        doprint = 0;
        sprintf(new_path, "%s", pathname);
    }
    if (doprint || enable_noisy) fw_hacks_print("SANITIZE_PATH: %s -> %s\n", pathname, new_path);
}

void vsyslog(int pri, char * fmt, ...)
{
    if (enable_noisy) {
        fw_hacks_print("intercepted vsyslog()\n");
    }
}

ssize_t read(int fd,void* buf,size_t nbytes)
{
    if (enable_noisy) {
        fw_hacks_print("intercepted read(%d, void* buf=%p, %u)\n", fd, buf, nbytes);
    }

    int res = 0;
    if (DUMMY_CONSOLE_FD != fd)
    {
        res = real_read(fd, buf, nbytes);
    }

    char fd_str[40];
    sprintf(fd_str, "%d, buf=%p, %u", fd, buf, nbytes);

    checkerror("read", fd_str);
    return res;
}

int close(int fd) {
    if (enable_noisy) {
        fw_hacks_print("intercepted close(%d)\n", fd);
    }

    int res = 0;
    if (DUMMY_CONSOLE_FD != fd) {
        res = real_close(fd);
    }

    char fd_str[20];
    sprintf(fd_str, "%d", fd);

    checkerror("close", fd_str);
    return res;
}

int open(const char *pathname, int flags, ...)
{
    fw_hacks_print("intercepted open(\"%s\",%p...) called by %s\n", SS(pathname), flags, progname);

    if (!pathname) return real_open(pathname, flags);

    char *new_path = calloc(MATHPATH, sizeof(char));
    sanitize_path(new_path, pathname);

    int fd;
    if (0 == real_strcmp(new_path, DEVCONSOLE) || 0 == real_strcmp(new_path, DEVTTY)) {
        free(new_path);
        return DUMMY_CONSOLE_FD;
    }
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list args;
        va_start(args, flags);
        mode_t mode = va_arg(args, mode_t);
        fd = real_open(new_path, flags, mode);
        va_end(args);
    } else {
        errno = 0;
        fd = real_open(new_path, flags);
    }

    checkerror("open", new_path);
    free(new_path);
    return fd;
}

long ptrace(int request, int pid, void *addr, void *data)
{
    fw_hacks_print("intercepted ptrace(request={%d},pid={%d},addr={%p},data={%p}) called by %s\n", request, pid, addr, data, progname_safe);
    return 0;
}


int stat(const char *path, stat_t * buf)
{
    fw_hacks_print("intercepted stat(\"%s\", %p) called by %s\n", SS(path), buf, progname_safe);

    if (!path) return real_stat(path, buf);

    char *new_path = calloc(MATHPATH, sizeof(char));
    sanitize_path(new_path, path);

    int res = real_stat(new_path, buf);

    checkerror("stat", new_path);
    free(new_path);
    return res;
}

int vfprintf(FILE * file, const char * format, va_list args)
{
    int res = 0;
    if (stderr == file) {
        res = fw_hacks_vfprintf("stderr", file, format, args);
    }
    else if (stdout == file) {
        res = fw_hacks_vfprintf("stdout", file, format, args);
    }
    else if (DUMMY_CONSOLE == file) {
        res = fw_hacks_vfprintf("console", file, format, args);
    }
    return res;
}

int fputc(int c, FILE* file)
{
    char str[2] = {(unsigned char)c, 0};
    va_list args = {str};
    int res = vfprintf(file, "%s", args);
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

size_t fwrite(const void * buf, size_t size, size_t n, FILE *f)
{
    if (enable_noisy) {
        fw_hacks_print("intercepted fwrite(buf=%p, sz=%u, n=%u, f=%p) called by %s\n", buf, size, n, f, progname_safe);
    }

    int res = 0;
    if (DUMMY_CONSOLE != f) {
        res = real_fwrite(f);
    }

    char desc[16];
    sprintf(desc, "%p", f);
    checkerror("fwrite", desc);
}


FILE * fopen(const char *filename, const char *modes)
{
    if (enable_noisy) {
        fw_hacks_print("intercepted fopen(%s, %s) called by %s\n", SS(filename), SS(modes), progname_safe);
    }

    if (!filename) return real_fopen(filename, modes);
    char *new_path = calloc(MATHPATH, sizeof(char));
    sanitize_path(new_path, filename);

    if (0 == real_strcmp(new_path, DEVCONSOLE)  || 0 == real_strcmp(new_path, DEVTTY)) {
        free(new_path);
        return DUMMY_CONSOLE;
    }

    FILE *res = real_fopen(new_path, modes);
    checkerror("fopen", new_path);
    free(new_path);
    return res;
}

int fclose(FILE * f)
{
    int res = 0;
    if (DUMMY_CONSOLE == f) {
        return res;
    }

    res = real_fclose(f);
    char desc[16];
    sprintf(desc, "%p", f);
    checkerror("fclose", desc);
}

int fputs(const char * string, FILE * f)
{
    if (enable_noisy) {
        fw_hacks_print("intercepted fputs(\"%s\", %p) called by %s\n", string, f, progname_safe);
    }
    int res = 0;
    if (DUMMY_CONSOLE == f) {
        fprintf(f, string);
    } else {
        res = real_fputs(string, f);
    }
    char desc[MATHPATH+32];
    sprintf(desc, "%p<-\"%s\"", f, string);
    checkerror("fputs", desc);
}

char* fgets(char *str, int num, FILE *stream)
{
    if (enable_noisy) {
        fw_hacks_print("intercepted fgets(\"%s\", %d, %p) called by %s\n", SS(str), num, stream, progname_safe);
    }
    char *res = real_fgets(str, num, stream);
    checkerror("fgets", "...");
    return res;
}

int envp_does_not_have_fw_hacks(char ** const envp)
{
    for (int i = 0; envp[i] != NULL; i++) {
        if (0 == real_strcmp(envp[i], FW_HACKS_ENV_VAR)) return 0;
    }
    return 1;
}

char** newenvp_with_fw_hacks(char** envp)
{
    char ** oldenv = envp;
    int len = 0;
    if (oldenv) {
        while ((*oldenv) != 0)
        {
            oldenv++;
            len++;
        }
    }
    char ** newenvp = malloc((len+2)*sizeof(char*));
    for (int i = 0; i < len; i++) {
        newenvp[i] = oldenv[i];
    }
    newenvp[len] = real_strdup(FW_HACKS_ENV_VAR);
    newenvp[len+1] = NULL;
}

int freeenv_injected(char** envp)
{
    char** envp_lastp = envp;
    while (*(envp_lastp+1))
    {
        envp_lastp++;
    }
    free(*envp_lastp);
    free(envp);
}

int execve(const char *pathname, char * const argv[], char * const envp[])
{
    char ** newenvp = (char**) envp;
    int created_new_env = 0;
    if (envp_does_not_have_fw_hacks(newenvp)) {
        created_new_env = 1;
        newenvp = newenvp_with_fw_hacks(newenvp);
        fw_hacks_print("execve DID NOT have my preload\n");
    }
    else {
        if (enable_noisy) fw_hacks_print("execve DID have my preload\n");
    }
    int res = real_execve(pathname, argv, newenvp);

    if (created_new_env) freeenv_injected(newenvp);


    char desc[1024];
    sprintf(desc, "%s, argv=%p, env=%p", pathname, argv, newenvp);

    checkerror("execve", desc);
    return res;
}

void decode_sockaddr(const void* addr, socklen_t len, const char * src_call)
{
    if (!addr || len < sizeof(sa_family_t)) {
        fw_hacks_print("%s() Invalid sockaddr (null or too small)\n", src_call);
        return;
    }

    const sa_family_t* family = (const sa_family_t*)addr;

    switch (*family) {
        case AF_INET: {
            struct sockaddr_in *a = (struct sockaddr_in *)addr;
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(a->sin_addr), ip, sizeof(ip));
            fw_hacks_print("%s: IPv4 to %s:%d\n", src_call, ip, ntohs(a->sin_port));
            break;
        }
        case AF_INET6: {
            struct sockaddr_in6 *a6 = (struct sockaddr_in6 *)addr;
            char ip6[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &(a6->sin6_addr), ip6, sizeof(ip6));
            fw_hacks_print("%s: IPv6 to [%s]:%d\n", src_call, ip6, ntohs(a6->sin6_port));
            break;
        }
        case AF_UNIX: {
            struct sockaddr_un *u = (struct sockaddr_un *)addr;
            fw_hacks_print("%s: UNIX socket path: %s\n", src_call, u->sun_path);
            break;
        }
        default:
            fw_hacks_print("%s() Unknown or unsupported sockaddr family: %d\n", src_call, *family);
    }
}


ssize_t recvfrom(int sockfd, void* restrict buf, size_t buflen, int flags, struct sockaddr* restrict addr, socklen_t* restrict addrlen)
{
    if (enable_noisy) {
        fw_hacks_print("intercepted recvfrom(fd=%d, flags=%p, addr=%p, len=%p) called by %s\n", sockfd, flags, addr, addrlen, progname_safe);
    }
    int res = real_recvfrom(sockfd, buf, buflen, flags, addr, addrlen);

    if (enable_noisy) {
        if (addrlen) decode_sockaddr(addr, *addrlen, "recvfrom");
    }

    char fd_str[20];
    sprintf(fd_str, "%d...", sockfd);
    checkerror("recvfrom", fd_str);
    return res;
}


ssize_t sendto(int sockfd, const void* buf, size_t buflen, int flags, const struct sockaddr* addr, socklen_t addrlen)
{
    if (enable_noisy) {
        fw_hacks_print("intercepted sendto(fd=%d, flags=%p, addr=%p, len=%p) called by %s\n", sockfd, flags, addr, addrlen, progname_safe);
        decode_sockaddr(addr, addrlen, "sendto");
    }
    int res = real_sendto(sockfd, buf, buflen, flags, addr, addrlen);


    char fd_str[20];
    sprintf(fd_str, "%d...", sockfd);
    checkerror("sendto", fd_str);
    return res;
}


int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
    fw_hacks_print("intercepted connect(fd=%d, addr=%p, len=%u) called by %s\n", sockfd, addr, addrlen, progname_safe);
    decode_sockaddr(addr, addrlen, "connect");

    int res = real_connect(sockfd, addr, addrlen);

    char fd_str[20];
    sprintf(fd_str, "%d...", sockfd);
    checkerror("connect", fd_str);
    return res;
}

int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
    fw_hacks_print("intercepted bind(%d, %p, %p) called by %s\n", sockfd, addr, addrlen, progname_safe);
    decode_sockaddr(addr, addrlen, "connect");

    int res = real_bind(sockfd, addr, addrlen);

    char fd_str[20];
    sprintf(fd_str, "%d...", sockfd);
    checkerror("bind", fd_str);
    return res;
}

int strcmp(char* dest, char* src)
{
    if (enable_noisy) {
        fw_hacks_print("intercepted strcmp(dest='%.64s', src='%.64s') called by %s\n", SS(dest), SS(src), progname_safe);
    }
    return real_strcmp(dest, src);
}

int strncmp(char* dest, char* src, unsigned int n)
{
    if (enable_noisy) {
        fw_hacks_print("intercepted strncmp(dest='%.64s', src='%.64s', n=%u) called by %s\n", SS(dest), SS(src), n, progname_safe);
    }
    return real_strncmp(dest, src, n);
}

char* strncpy(char* dest, const char* src, size_t n)
{
    if (enable_noisy) {
        fw_hacks_print("intercepted strncpy(dest=%p, src='%.64s', n=%zu) called by %s\n", (void*)dest, SS(src), n, progname_safe);
    }
    return real_strncpy(dest, src, n);
}

int dni_strnlen_s (char* func, unsigned int lineno, char * dest, unsigned int dmax)
{
    if (enable_noisy) {
        fw_hacks_print("intercepted dni_strnlen_s(caller=%s, call_lineno=%u, dest='%.64s', dmax=%u) called by %s\n", func, lineno, SS(dest), dmax, progname_safe);
    }
    return real_dni_strnlen_s(func, lineno, dest, dmax);
}

int dni_strcmp_s(char* func, unsigned int lineno, char * dest, unsigned int dmax, char * src)
{
    if (enable_noisy) {
        fw_hacks_print("intercepted dni_strcmp_s(caller=%s, call_lineno=%u, dest='%.64s', dmax=%u, src='%.64s') called by %s\n", func, lineno, SS(dest), dmax, SS(src), progname_safe);
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
    SETUP_INJECT(close);
    fw_hacks_print("__libc_start_main()\n");
    real___libc_start_main = dlsym(RTLD_NEXT,"__libc_start_main");
    if (!real___libc_start_main ) {
        fw_hacks_print("cannot inject orig libc start main!!!");
    return -1337;
    }
    fw_hacks_print("Injecting mandatory funcs.\n");
    if (
        INJECT_AND_CHECK(fopen)
        & INJECT_AND_CHECK(fclose)
        & INJECT_AND_CHECK(fwrite)
        & INJECT_AND_CHECK(fputs)
        & INJECT_AND_CHECK(fgets)
        & INJECT_AND_CHECK(read)
        & INJECT_AND_CHECK(execve)
        & INJECT_AND_CHECK(connect)
        & INJECT_AND_CHECK(bind)
        & INJECT_AND_CHECK(stat)
        & INJECT_AND_CHECK(strlen)
        & INJECT_AND_CHECK(strcmp)
        & INJECT_AND_CHECK(strncmp)
        & INJECT_AND_CHECK(strncpy)
        & INJECT_AND_CHECK(strdup)
        & INJECT_AND_CHECK(strerror)
        & INJECT_AND_CHECK(sendto)
        & INJECT_AND_CHECK(recvfrom)
    ) {
        is_injected = 1;
        if (argc && argv && *argv) {
            fw_hacks_print("Injection sucess on %s\n", *argv);
        progname = real_strdup(*argv);
        } else {
            fw_hacks_print("?? Injected without argv[0] or argc == 0\n");
        }
    } else {
        fw_hacks_print("Injection failed\n");
        is_injected = 0;
    }
    fw_hacks_print("Injecting optional.\n");
    INJECT_AND_CHECK(dni_strcmp_s);
    INJECT_AND_CHECK(dni_strnlen_s);
    real_main = main_orig;
    int res = real___libc_start_main(main_hook, argc, argv, fini, rtld_fini, stack_end);
    if (progname) free(progname);
    return res;
}
