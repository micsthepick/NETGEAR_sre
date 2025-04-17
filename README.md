# netgear router reversing/emulation project

## first steps
loosely following https://boschko.ca/qemu-emulating-firmware/ install qemu
(it's okay if the first command fails, as long as qemu-system, qemu-user-binfmt and qemu-user-static install)
```
sudo apt-get install qemu
sudo apt-get install qemu-user-static
sudo apt-get install qemu-user-binfmt
sudo apt-get install qemu-system
```

use binwalk to extract the squashfs (this article assumes you put the squashfs file in ~/NETGEAR\_sre), and `sudo unsquash` to extract that. Now run:

```
sudo cp $(which qemu-arm-static) squashfs-root/$(which qemu-arm-static)
```

to put the arm binary in place

if at this point you still don't have the capability to chroot, then
ensure `/etc/binfmt.d/` or `/usr/lib/binfmt.d/` contains `qemu-arm.conf`:
```
:qemu-arm:M::\x7fELF\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x28\x00:\xff\xff\xff\xff\xff\xff\xff\x00\xff\xff\xff\xff\xff\xff\xff\xff\xfe\xff\xff\xff:./scratch/usr/local/bin/qemu-arm:
```
then try
```
sudo systemctl reload systemd-binfmt
```

## chroot
now you can start doing the chroot,
first you must create /dev/null and others:

```
sudo env SHELL=/bin/sh PATH=/usr/bin:/usr/sbin:/sbin:/bin chroot . /bin/mknod -m 666 /dev/null c 1 3
sudo env SHELL=/bin/sh PATH=/usr/bin:/usr/sbin:/sbin:/bin chroot . mount -t proc none /proc

mkdir tmp/lock
echo -n Base tmp/orbi_type
```

now runnigng pretty much any program, you will encounter "Function not defined"
You can diagnose this by adding QEMU\_STRACE= as another Env var

```
QEMU_STRACE= sudo -E chroot . env LD_PRELOAD=/ptrace.so SHELL=/bin/sh PATH=/usr/bin:/usr/sbin:/sbin:/bin etc/init.d/lighttpd start
```

the way I will fix this is by (cross) compiling a LD\_PRELOAD stub for syscalls, and will change it as this article continues as necessary.
The first step is to disable ptrace, so I've gone ahead and returned 0 by default in the code that follows. After doing this though, we get a complaint about missing the /proc/mtd path.

## introduction to the stub
Here is a sample stub to fix both of the above.
```
#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <string.h>

static int (*real_open)(const char *, int, ...) = NULL;

int open(const char *pathname, int flags, ...) {
    if (!real_open) {
        real_open = (int (*)(const char *, int, ...)) dlsym(RTLD_NEXT, "open");
        if (!real_open) {
            perror("dlsym failed to find open");
            return -1;
        }
    }

    // if we start pathname with /proc/mtd
    if (strncmp(pathname, "/proc/mtd", 9) == 0) {
        char new_path[256];
        snprintf(new_path, sizeof(new_path), "/tmp%s", pathname + 5);  // Redirect to /tmp, e.g. /proc/mtdblahblah goes to /mtdblahbalh
        return real_open(new_path, flags);
    }

    // For all other paths, use the real open function
    return real_open(pathname, flags);
}

long ptrace(int request, int pid, void *addr, void *data) {
    return 0;
}
```

an explanation:

> first we define `_GNU_SOURCE` - this is needed to have some of the internal GNU symbols like `RTLD_NEXT`
> next we include some standard headers for string and file operations, but interestingly, we include dlfcn.h
> this will allow us to do the actual `LD_PRELOAD` system call overrides by calling the function returned by dlsym

> our first override is open, which conditionally just passes to open, or if the prefix is /proc/mtd, then it diverts to a new prefix, /mtd
> the second one just stubs out the ptrace call and pretends like everything worked fine.

You likely will need to cross compile if the target architecture is different.

Easiest is to use the musl toolchain (as appropriate for your router firmware):

download and compile with the musl cross-compiler (outside of the chroot is preferred)

### download
```
cd ~/NETGEAR_sre
wget https://musl.libc.org/releases/musl-1.2.5.tar.gz
tar xf musl-1.2.5.tar.gz
```

### configure and compile musl
this took way to long to figure out fully. I gave up trying to figure it out myself and to save time got chatgpt to fix up the install script,
maybe later I'll fix it up to not leave junk lying around everywhere.

```
#!/bin/bash

set -e  # Stop on first error
set -o pipefail  # Catch errors in piped commands

MUSL_VERSION="1.2.5"
TARGET="arm-linux-musleabi"
INSTALL_DIR="$HOME/NETGEAR_sre/musl-cross"

# Create working directory
mkdir -p $INSTALL_DIR
cd $INSTALL_DIR

# Step 1: Download and Extract Musl
echo "Downloading musl $MUSL_VERSION..."
wget -c https://musl.libc.org/releases/musl-$MUSL_VERSION.tar.gz
tar xvf musl-$MUSL_VERSION.tar.gz
cd musl-$MUSL_VERSION

# Step 2: Build Musl Cross-Compiler (Bootstrap)
echo "Building musl cross-compiler..."
CC=arm-linux-gnueabi-gcc ./configure --target=$TARGET --prefix=$INSTALL_DIR --disable-shared
make -j$(nproc)
make install

# Step 3: Use Musl Cross-Compiler to Build Final Musl
echo "Building final musl with musl-cross..."
cd $INSTALL_DIR
wget -c https://musl.libc.org/releases/musl-$MUSL_VERSION.tar.gz
tar xvf musl-$MUSL_VERSION.tar.gz
cd musl-$MUSL_VERSION

CC=$INSTALL_DIR/bin/$TARGET-gcc ./configure --target=$TARGET --prefix=$INSTALL_DIR
make -j$(nproc)
make install

echo "Musl cross-compilation Installed in: $INSTALL_DIR"
```

```
cd musl-1.2.5/
./configure --prefix=$(realpath ~/NETGEAR_sre)/muslbin --target=armv5l-linux-musleabi --host=x86_64-linux-gnu --CC=gcc
make && make install
cd ~/NETGEAR_sre
```

### compile and copy stub LD\_PRELOAD shared object
```
./musl/bin/musl-gcc ./fw_hacks.c -fPIC -shared -o fw_hacks.so
sudo cp fw_hacks.so squashfs-root/
```

now it does work somewhat but to make things maximally compatible, I've downloaded the appropriate version of busybox, compiled with the cross compiler, enabling debug symbols
n.b. there are a couple of compiler errors due to a couple instances of the same syscall getting called in the wrong manner, which can be fixed by just rewriting the syscall, and sanitizers don't work out of box, but that might be an interesting idea for further investigation?

what I then did is copy the whole squashfs to squashfs\_root\_bb/ (preserving links) and copy over the binaries (busybox and the musl lib)
```
sudo cp -P -r squashfs-root/ squashfs_root_bb
sudo cp -P -r busybox_install/lib squashfs_root_bb/lib
sudo cp busybox_install/bin/busybox squashfs_root_bb/bin/busybox
sudo rm -rf squashfs_root_bb/tmp
sudo mkdir squashfs_root_bb/tmp
sudo mkdir squashfs_root_bb/tmp/run
echo -n "Base" | sudo tee squashfs_root_bb/tmp/orbi_type >/dev/null
```

and changed the fw\_hacks stub as follows:

```
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
```

at some point I encountered an error about entropy with lighttpd, in order to fix that you just need to mount system radndom and urandom

```
sudo touch squashfs-root/dev/urandom; sudo mount -o bind /dev/urandom squashfs-root/dev/urandom
sudo touch squashfs-root/dev/random; sudo mount -o bind /dev/random squashfs-root/dev/random
```

lighthttpd would crash on first load because of a cgi script, it took a while but I eventually figured out by adding the above config to etc/lighttpd/conf.d/cgi.conf

```
server.modules += ( "mod_cgi" )

# Set environment variables to debug CGI execution
setenv.add-environment = (
    "GATEWAY_INTERFACE" => "CGI/1.1",
    "LD_LIBRARY_PATH" => "/lib:/usr/lib",
    "CGI_DEBUG_LOG" => "/var/log/lighttpd/cgi-debug.log"
)

# Log all Lighttpd debug information
server.debug-log-file = "/var/log/lighttpd/debug.log"
debug.log-request-handling = "enable"
debug.log-request-header = "enable"
debug.log-response-header = "enable"
debug.log-condition-handling = "enable"
```
additionally I created a few scripts to run the server:

run\_args.sh
```
sudo systemctl start systemd-binfmt
[[ $QS -eq 1 ]] && CHROOTENV+=" QEMU_STRACE="
sudo chroot squashfs_root_bb /usr/bin/env -i HOME=/ PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin "SHELL=/bin/sh" $CHROOTENV LD_PRELOAD=/fw_hacks.so $@
```

run\_lighttpd\_bb.sh
```
# exec ./run_args.sh /etc/init.d/lighttpd start "2>&1"
exec ./run_args.sh /usr/sbin/lighttpd -D -f /etc/lighttpd/lighttpd.conf "2>&1"
```

upon running run\_lighttpd\_bb.sh, I can see the following error(s):
```
fw_hacks: OPEN(/etc/ppp/ppp0-status,...) called by /usr/sbin/net-cgi
fw_hacks: SANITIZE_PATH: /etc/ppp/ppp0-status -> /etc/ppp/ppp0-status
fw_hacks: OPEN(/tmp/port_status,...) called by /usr/sbin/net-cgi
fw_hacks: SANITIZE_PATH: /tmp/port_status -> /tmp/port_status
Assertion failed: errno != ESNULLP (dni_safeclib.c: dni_fscanf_s: 210)
```

so it wants to have a ppp0 interface. Rather then fighting with interfaces and the QEMU networking stack, I'll just stub out those files.
```
./run_args.sh tee /etc/ppp/ppp0-status <<< "1"
./run_args.sh tee /tmp/port_status <<< "1"
```

but the problem persists?
After a bit of investigation / reverse engineering I stumbled on this: https://tbrindus.ca/correct-ld-preload-hooking-libc/
after a quick patch to my `LD_PRELOAD` lib:

```
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
static int (*real___stat_time64)(const char *, struct stat *) = NULL;
static int (*real___libc_start_main)() = NULL;
static FILE* (*real_fopen)() = NULL;

static int is_injected = 0;

static char *progname = NULL;

#define progname_safe (progname ? progname : "unknown program!")
/

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
    return real_fopen(filename, modes);
}

#if 0
int execve(const char *pathname, char *const argv[], char *const envp[]) {
    P("intercepted EXECVE: %s\n", pathname ? pathname : "<NULL>");

    const char *ld_preload = "LD_PRELOAD=/fw_hacks.so";
#if DEBUG_PRINTENV
    dbgprintenv(envp);
#endif

#if DEBUG_PRINTARG
    dbgprintargv(argv);
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
#endif

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
```

I can see that it's still trying to load stuff from /proc - `fw_hacks`:
```
intercepted FOPEN(/proc/net/if_inet6, r) called by /usr/sbin/net-cgi
```
let's stub that out too:

sanitize\_path
```
void sanitize_path(char* new_path, const char* pathname) {
    // keep the new_path <= the pathname in size if possible
    if (pathname == NULL) {
        return;
    }
    if ('/' == *pathname) {
        while ('/' == pathname[1]) { pathname++; }
    }
    if (strncmp(pathname, "/proc/mtd", 9) == 0 && (pathname[9] == '\0' || pathname[9] == '/')) {
        sprintf(new_path, "/mtd%s", pathname + 9);
    } else if (strncmp(pathname, "/proc/device-tree", 17) == 0) {
        sprintf(new_path, "/device-tree%s", pathname + 5);
    } else {
	    sprintf(new_path, "%s", pathname);
    }
    P("SANITIZE_PATH: %s -> %s\n", pathname, new_path);
}
```

and fopen
```
FILE * fopen(const char *filename, const char *modes) {
    P("intercepted FOPEN(%s, %s) called by %s\n", SS(filename), SS(modes), progname_safe);

    if (!filename) return real_fopen(filename, modes);
    char *new_path = calloc(strlen(filename), sizeof(char));
    sanitize_path(new_path, filename);

    FILE *res = real_fopen(filename, modes);
    free(new_path);

    return res;
}
```

now fopen and open are essentailly doing the same job, but just in case I'll leave both functional.

```
./run_args.sh touch /net/if_inet6
```

let's try just mounting /proc

script: `mounts_for_fw_pack.sh`
```
sudo mkdir -p squashfs_root_bb/proc
sudo mount --bind /dev/urandom /home/mike/NETGEAR_sre/squashfs_root_bb/dev/urandom
sudo mount --bind /dev/random /home/mike/NETGEAR_sre/squashfs_root_bb/dev/random
sudo mount --bind /proc squashfs_root_bb/proc
```

script: first_startup.sh  
```
sudo env SHELL=/bin/sh PATH=/usr/bin:/usr/sbin:/sbin:/bin chroot squashfs_root_bb /bin/mknod -m 666 /dev/null c 1 3
echo -n "Base" | sudo tee squashfs_root_bb/tmp/orbi_type >/dev/null
test ! -e squashfs_root_bb/lib/libc.so.bak && sudo mv squashfs_root_bb/lib/libc.so squashfs_root_bb/lib/libc.so.bak
sudo cp arm-unknown-linux-musleabi/arm-unknown-linux-musleabi/sysroot/usr/lib/libc.so squashfs_root_bb/lib/libc.so
echo -e "nameserver 8.8.8.8\nnameserver 8.8.4.4"
```
