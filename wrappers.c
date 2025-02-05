#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <err.h>
#include <curses.h>
#include <termios.h>
#include <signal.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/times.h>
#include <sys/ioctl.h>

#pragma pack(push, 1)
struct unixtermios {
    uint16_t    c_iflag;
    uint16_t    c_oflag;
    uint16_t    c_cflag;
    uint16_t    c_lflag;
    char        c_line;
    uint8_t     c_cc[8];
};
#pragma pack(pop)

#define UNIX_VMIN 4
#define UNIX_VTIME 3

static struct termios original;

void __attribute__((constructor)) init_terminal_settings()
{
    // Make a backup of the terminal state to restore to later.
    if (tcgetattr(STDIN_FILENO, &original) != 0) {
        err(EXIT_FAILURE, "Failed to query terminal attributes.");
    }
}

void __attribute__((destructor)) fini_terminal_settings()
{
    // Restore any terminal craziness that 123 left behind.
    if (tcsetattr(STDIN_FILENO, TCSANOW, &original) != 0) {
        warn("Failed to restore terminal attributes, sorry!");
    }
}

int __unix_ioctl(int fd, unsigned long request, struct unixtermios *argp)
{
    struct termios tio = {0};

    fprintf(stderr, "ioctl(%d, %#x);\n", fd, request);

    switch (request) {
        case TCSETS:
            if (tcgetattr(fd, &tio) != 0) {
                err(EXIT_FAILURE, "Failed to translate ioctl() to tcgetattr()");
            }

            // Lotus wants raw mode?
            if ((argp->c_oflag & 0xE7FFu) == 0) {
                cfmakeraw(&tio);

                // Okay, but nobody likes ignbrk
                tio.c_iflag |= BRKINT | IGNBRK;
                tio.c_lflag |= ISIG;
            } else {
                // I think it's trying to disable raw mode?
            }

            // Translate timeouts.
            tio.c_cc[VTIME] = argp->c_cc[UNIX_VTIME];
            tio.c_cc[VMIN] = argp->c_cc[UNIX_VMIN];

            if (tcsetattr(fd, TCSANOW, &tio) != 0) {
                err(EXIT_FAILURE, "Failed to translate ioctl() to tcsetattr()");
            }

            // Check if that worked
            if (tcgetattr(fd, &tio) != 0) {
                err(EXIT_FAILURE, "Failed to translate ioctl() to tcgetattr()");
            }
            return 0;
        case TCGETS:
            if (tcgetattr(fd, &tio) != 0) {
                err(EXIT_FAILURE, "Failed to translate ioctl() to tcgetattr()");
            }
            argp->c_cc[UNIX_VTIME] = tio.c_cc[VTIME];
            argp->c_cc[UNIX_VMIN]  = tio.c_cc[VMIN];
            return 0;
        default:
            fprintf(stderr, "ioctl: unknown request\n");
    }
    return -1;
}

int __unix_fcntl(int fd, int cmd, ...)
{
    fprintf(stderr, "fcntl(%d, %#x);\n", fd, cmd);
    switch (cmd) {
        case F_SETLK:
            return 0;
        default:
            fprintf(stderr, "fcntl: unknown request\n");
    }
    return -1;
}


void * __unix_signal(int signum, void *handler)
{
    fprintf(stderr, "signal(%u, %p);\n", signum, handler);
    return signal(signum, handler);
}

struct unixstat {
    uint32_t    field_0;
    uint32_t    st_mode;
    uint32_t    field_8;
    uint32_t    field_C;
    uint32_t    st_size;
};

int __unix_stat(const char *pathname, struct unixstat *statbuf)
{
    struct stat buf;

    fprintf(stderr, "stat(%s, %p);\n", pathname, statbuf);

    if (stat(pathname, &buf) != 0)
        return -1;

    statbuf->st_size = buf.st_size;
    statbuf->st_mode = buf.st_mode;
    return 0;
}

int __unix_fstat(int fd, struct unixstat *statbuf)
{
    struct stat buf;

    fprintf(stderr, "fstat(%d, %p);\n", fd, statbuf);

    if (fstat(fd, &buf) != 0)
        return -1;

    statbuf->st_size = buf.st_size;
    statbuf->st_mode = buf.st_mode;
    return 0;
}

int __unix_open(const char *pathname, int flags, mode_t mode)
{
    fprintf(stderr, "open(\"%s\", %#x, %o);\n", pathname, flags, mode);

    switch (flags) {
        case 0x001: return open(pathname, O_WRONLY);
        case 0x101: return open(pathname, O_CREAT | O_WRONLY, mode);
        case 0x102: return open(pathname, O_CREAT | O_RDWR, mode);
        case 0x000: return open(pathname, O_RDONLY);
        case 0x109: return open(pathname, O_CREAT | O_WRONLY | O_APPEND, mode);
        case 0x302: return open(pathname, O_CREAT | O_TRUNC | O_RDWR, mode);
        default:
            errx(EXIT_FAILURE, "open() was called with unrecognized flags %#x", flags);
    }
    return -1;
}

int __unix_uname(char *sysname)
{
    struct utsname name;
    fprintf(stderr, "uname(%p);\n", sysname);
    if (uname(&name) != 0) {
        return -1;
    }
    strncpy(sysname, name.sysname, 48);
    return 0;
}

int __unix_times(void *buffer)
{
    struct tms buf;
    fprintf(stderr, "times(%p);\n", buffer);
    // Note: 123 only cares about the return code.
    return times(&buf);
}


#define SI86FPHW 40
#define FP_387 3
int __unix_sysi86(int cmd, uint32_t *result)
{
    fprintf(stderr, "sysi86(%d, %p);\n", cmd, result);
    // This is used to check for x87 support, nothing else is supported.
    if (cmd != SI86FPHW)
        return -1;

    *result = FP_387;
    return 0;
}
