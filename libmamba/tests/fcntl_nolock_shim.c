#define _GNU_SOURCE
#include <stdarg.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

typedef int void_fcntl(int, int);
typedef int int_fcntl(int, int, int);
typedef int uint64_fcntl(int, int, u_int64_t*);
typedef int fownerex_fcntl(int, int, struct f_owner_ex*);

int fcntl(int fd, int cmd, ...)
{
    void* real_fcntl = dlsym(RTLD_NEXT, "fcntl");
    // Inspired by https://stackoverflow.com/a/58472959
    int result;
    va_list va;
    va_start(va, cmd);

    switch (cmd) {
        // void cases
        case F_GETFD:
        case F_GETFL:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
#ifdef F_GETPIPE_SZ
        case F_GETPIPE_SZ:
#endif
#ifdef F_GET_SEALS
        case F_GET_SEALS:
#endif
            va_end(va);
            return ((void_fcntl*)real_fcntl)(fd, cmd);
        // int cases
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETFL:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
#ifdef  F_SETPIPE_SZ
        case F_SETPIPE_SZ:
#endif
#ifdef F_ADD_SEALS
        case F_ADD_SEALS:
#endif
            result = ((int_fcntl*)real_fcntl)(fd, cmd, va_arg(va, int));
            va_end(va);
            return result;
        // u_int64_t*
#ifdef F_GET_RW_HINT
        case F_GET_RW_HINT:
        case F_SET_RW_HINT:
        case F_GET_FILE_RW_HINT:
        case F_SET_FILE_RW_HINT:
            result = ((uint64_fcntl*)real_fcntl)(fd, cmd, va_arg(va, u_int64_t*));
            va_end(va);
            return result;
#endif
        // f_owner_ex* cases
        case F_GETOWN_EX:
        case F_SETOWN_EX:
            result = ((fownerex_fcntl*)real_fcntl)(fd, cmd, va_arg(va, struct f_owner_ex*));
            va_end(va);
            return result;
        // flock* cases
        case F_SETLK:
        case F_SETLKW:
        case F_GETLK:
            va_end(va);
            errno = ENOSYS;
            return -1;
        default:
            return -1;
    }
}
