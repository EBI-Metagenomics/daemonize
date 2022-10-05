#include "argless.h"

// Acknowledgement: util-linux

#define _POSIX_C_SOURCE 200112L
#define _DARWIN_C_SOURCE 1
#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef NULL
#undef NULL
#endif

#define NULL ((void *)0)

static struct argl_option const options[] = {
    {"stdin", 'i', ARGL_TEXT("STDIN", "stdin"),
     "Named pipe to simulate stdin."},
    {"stdout", 'o', ARGL_TEXT("STDOUT", "stdout"),
     "Named pipe to simulate stdout."},
    {"stderr", 'e', ARGL_TEXT("STDERR", "stderr"),
     "Named pipe to simulate stderr."},
    {"pidfile", 'p', ARGL_TEXT("PIDFILE", "pid"), "File to write PID into."},
    ARGL_DEFAULT,
    ARGL_END,
};

static struct argl argl = {.options = options,
                           .args_doc = "[options] <program> [arguments ...]",
                           .doc = "Daemonize a program.",
                           .version = "0.1.1"};

static void noreturn fatalxc(int excode, char const *fmt, ...);
#define fatal(...) fatalxc(EXIT_FAILURE, __VA_ARGS__)
static void noreturn pfatal(char const *fmt, ...);

static void close_stdout(void);
static void create_pipes(char const *restrict file0, char const *restrict file1,
                         char const *restrict file2);
static void save_pidfile(char const *filepath, int pid);
static void close_nonstd_fds(void);

#define EX_EXEC_FAILED 126 /* Program located, but not usable. */
#define EX_EXEC_ENOENT 127 /* Could not find program to exec.  */
#define fatalexec(x)                                                           \
    fatalxc(errno == ENOENT ? EX_EXEC_ENOENT : EX_EXEC_FAILED,                 \
            "failed to execute %s", x);

int main(int argc, char *argv[])
{
    argl_parse(&argl, argc, argv);
    if (argl_nargs(&argl) == 0) argl_usage(&argl);

    setlocale(LC_ALL, "");
    atexit(close_stdout);

    pid_t pid = 0;

    if ((pid = fork()) < 0)
        fatal("fork failed");
    else if (pid != 0)
    {
        save_pidfile(argl_get(&argl, "pidfile"), pid);
        return EXIT_SUCCESS;
    }

    close_nonstd_fds();
    if (setsid() < 0) fatal("setsid failed");

    create_pipes(argl_get(&argl, "stdin"), argl_get(&argl, "stdout"),
                 argl_get(&argl, "stderr"));

    char const *file = argl_args(&argl)[0];
    execvp(file, argl_args(&argl));
    fatalexec(file);
    return 0;
}

static void save_pidfile(char const *filepath, int pid)
{
    FILE *fp = fopen(filepath, "wb");
    if (!fp) pfatal("failed to open %s", filepath);

    if (fprintf(fp, "%d", pid) < 0) pfatal("failed to write %s", filepath);

    if (fclose(fp)) pfatal("failed to close %s", filepath);
}

static inline int flush_standard_stream(FILE *stream)
{
    int fd = 0;

    errno = 0;

    if (ferror(stream) != 0 || fflush(stream) != 0) goto error;

    /*
     * Calling fflush is not sufficient on some filesystems
     * like e.g. NFS, which may defer the actual flush until
     * close. Calling fsync would help solve this, but would
     * probably result in a performance hit. Thus, we work
     * around this issue by calling close on a dup'd file
     * descriptor from the stream.
     */
    if ((fd = fileno(stream)) < 0 || (fd = dup(fd)) < 0 || close(fd) != 0)
        goto error;

    return 0;

error:
    return (errno == EBADF) ? 0 : EOF;
}

static void noreturn fatalxc(int excode, char const *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs("\n", stderr);
    exit(excode);
}

static void noreturn pfatal(char const *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    if (errno) fprintf(stderr, ": %s", strerror(errno));
    fputs("\n", stderr);
    exit(EXIT_FAILURE);
}

static void close_stdout(void)
{
    if (flush_standard_stream(stdout) != 0 && !(errno == EPIPE))
        pfatal("write error");

    if (flush_standard_stream(stderr) != 0) exit(EXIT_FAILURE);
}

static void create_pipes(char const *restrict file0, char const *restrict file1,
                         char const *restrict file2)
{
    char const *files[] = {file0, file1, file2};
    FILE *fps[] = {stdin, stdout, stderr};
    char const *modes[] = {"r", "a", "a"};

    for (int i = 0; i < 3; ++i)
    {
        if (mkfifo(files[i], 0666) < 0)
            pfatal("mkfifo failed for %s", files[i]);

        if (!freopen(files[i], modes[i], fps[i]))
            pfatal("freopen failed for %s", files[i]);
    }
}

/* Source: OpenSIPS */
static void close_nonstd_fds(void)
{
    /* 32 is the maximum number of inherited open file descriptors */
    for (int r = 3; r < 32; r++)
        close(r);
}
