#include "argless.h"

// Acknowledgement: util-linux, git, and https://stackoverflow.com/a/17955149

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
                           .version = "0.1.6"};

static noreturn void fatalxc(int excode, char const *fmt, ...);
#define fatal(...) fatalxc(EXIT_FAILURE, __VA_ARGS__)
static noreturn void pfatal(char const *fmt, ...);
#define EX_EXEC_FAILED 126 /* Program located, but not usable. */
#define EX_EXEC_ENOENT 127 /* Could not find program to exec.  */
#define fatalexec(x)                                                           \
    fatalxc(errno == ENOENT ? EX_EXEC_ENOENT : EX_EXEC_FAILED,                 \
            "failed to execute %s", x);

static void close_stdout(void);
static void close_all_nonstd_fds(void);
static void daemonize(char const *pidfile);
static void create_fifos(char const *f0, char const *f1, char const *f2);
static void pop_double_dash(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    argl_parse(&argl, argc, argv);
    if (argl_nargs(&argl) == 0) argl_usage(&argl);
    char const *fpin = argl_get(&argl, "stdin");
    char const *fpout = argl_get(&argl, "stdout");
    char const *fperr = argl_get(&argl, "stderr");
    char const *pidfile = argl_get(&argl, "pidfile");
    char const *program = argl_args(&argl)[0];
    char **args = argl_args(&argl);
    pop_double_dash(argl_nargs(&argl), args);

    setlocale(LC_ALL, "");
    atexit(close_stdout);
    close(0);
    close(1);
    close(2);
    close_all_nonstd_fds();

    daemonize(NULL);
    if (setsid() < 0) fatal("setsid failed");
    daemonize(pidfile);

    create_fifos(fpin, fpout, fperr);

    execvp(program, args);
    fatalexec(program);

    return 0;
}

static void save_pidfile(char const *filepath, int pid);

static void daemonize(char const *pidfile)
{
    pid_t pid = fork();

    if (pid < 0) fatal("fork failed");
    if (pid != 0)
    {
        if (pidfile) save_pidfile(pidfile, pid);
        exit(EXIT_SUCCESS);
    }
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

static noreturn void fatalxc(int excode, char const *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs("\n", stderr);
    exit(excode);
}

static noreturn void pfatal(char const *fmt, ...)
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

static void create_fifos(char const *f0, char const *f1, char const *f2)
{
    char const *files[] = {f0, f1, f2};
    FILE *fps[] = {stdin, stdout, stderr};
    char const *modes[] = {"r", "a", "a"};

    for (int i = 0; i < 3; ++i)
    {
        if (mkfifo(files[i], 0666) < 0)
            pfatal("mkfifo failed for %s", files[i]);
    }

    for (int i = 0; i < 3; ++i)
    {
        if (!freopen(files[i], modes[i], fps[i]))
            pfatal("freopen failed for %s", files[i]);
    }
}

static void close_all_nonstd_fds(void)
{
    for (int fd = _SC_OPEN_MAX; fd > 2; fd--)
        close(fd);
}

static int double_dash_index(int argc, char *argv[])
{
    for (int i = 0; i < argc; ++i)
    {
        if (!strcmp(argv[i], "--")) return i;
    }
    return -1;
}

static void pop_double_dash(int argc, char *argv[])
{
    int i = double_dash_index(argc, argv);
    if (i < 0) return;
    memmove(argv + i, argv + i + 1, sizeof(argv[0]) * (argc - i));
}
