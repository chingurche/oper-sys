#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

static int write_all(int fd, const char *buf, size_t len)
{
    size_t off = 0;
    while (off < len) {
        ssize_t w = write(fd, buf + off, len - off);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        off += (size_t)w;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        const char msg[] = "usage: ./child filename\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    int file = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (file == -1) {
        const char msg[] = "error: failed to open requested file\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    char buf[4096];
    ssize_t bytes;

    while ((bytes = read(STDIN_FILENO, buf, sizeof buf)) != 0) {
        if (bytes < 0) {
            if (errno == EINTR) continue;
            const char msg[] = "error: failed to read\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }

        int has_nl = 0;
        ssize_t content = bytes;
        if (buf[bytes - 1] == '\n') {
            has_nl = 1;
            content = bytes - 1;
        }

        for (ssize_t i = 0; i < content / 2; ++i) {
            char tmp = buf[i];
            buf[i] = buf[content - 1 - i];
            buf[content - 1 - i] = tmp;
        }

        if (write_all(file, buf, (size_t)content) == -1) {
            const char msg[] = "error: failed to write\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }

        if (has_nl) {
            if (write_all(file, "\n", 1) == -1) {
                const char msg[] = "error: failed to write\n";
                write(STDERR_FILENO, msg, sizeof(msg) - 1);
                exit(EXIT_FAILURE);
            }
        }
    }


    close(file);
    return EXIT_SUCCESS;
}