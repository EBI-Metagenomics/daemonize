#include "argless.h"

// Acknowledgement: util-linux

#define _POSIX_C_SOURCE 200112L
#define _DARWIN_C_SOURCE 1
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <unistd.h>

#ifdef NULL
#undef NULL
#endif

#define NULL ((void *)0)

static struct argl_option const options[] = {
    {"stdint", 'i', ARGL_TEXT("STDIN", "stdin"),
     "Named pipe to simulate stdin."},
    {"stdout", 'o', ARGL_TEXT("STDOUT", "stdout"),
     "Named pipe to simulate stdout."},
    {"stderr", 'e', ARGL_TEXT("STDERR", "stderr"),
     "Named pipe to simulate stderr."},
    ARGL_DEFAULT,
    ARGL_END,
};

static struct argl argl = {.options = options,
                           .args_doc = "[options] <program> [arguments ...]",
                           .doc = "Daemonize a program.",
                           .version = "1.0.0"};

static void noreturn fatal(int stauts, char const *msg);
static void close_stdout(void);

#define EX_EXEC_FAILED 126 /* Program located, but not usable. */
#define EX_EXEC_ENOENT 127 /* Could not find program to exec.  */

int main(int argc, char *argv[])
{
    argl_parse(&argl, argc, argv);
    if (argl_nargs(&argl) == 0) argl_usage(&argl);

    setlocale(LC_ALL, "");
    atexit(close_stdout);

    pid_t pid = 0;
    if ((pid = fork()) < 0)
        fatal(EXIT_FAILURE, "fork failed");
    else if (pid != 0)
        return EXIT_SUCCESS;

    if (setsid() < 0) fatal(EXIT_FAILURE, "setsid failed");

    char const *file = argl_args(&argl)[0];
    execvp(file, argl_args(&argl));
    if (errno)
    {
        fprintf(stderr, "failed to execute %s", file);
        exit(errno == ENOENT ? EX_EXEC_ENOENT : EX_EXEC_FAILED);
    }

    return 0;
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

static void noreturn fatal(int excode, char const *msg)
{
    fputs(msg, stderr);
    exit(excode);
}

static void close_stdout(void)
{
    if (flush_standard_stream(stdout) != 0 && !(errno == EPIPE))
    {
        if (errno)
            fputs("write error", stderr);
        else
        {
            fputs("write error", stderr);
            fprintf(stderr, ": ");
            fprintf(stderr, "%m");
        }
        exit(EXIT_FAILURE);
    }

    if (flush_standard_stream(stderr) != 0) exit(EXIT_FAILURE);
}
