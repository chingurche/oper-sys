#include <sys/wait.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

static const char NAME_PROGRAM_NAME[] = "./child";

static pid_t init_child(const char *arg,
                        int my_pipe[2],
                        int other_pipe[2])
{
    pid_t pid = fork();
    if (pid < 0) {
        const char msg[] = "error: fork failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }
    if (pid > 0) return pid;

    dup2(my_pipe[0], STDIN_FILENO);
    close(my_pipe[0]);
    close(my_pipe[1]);
    close(other_pipe[0]);
    close(other_pipe[1]);

    char *const args[] = { (char *)NAME_PROGRAM_NAME, (char *)arg, NULL };
    execv(NAME_PROGRAM_NAME, args);

    const char msg[] = "error: execv failed\n";
	write(STDERR_FILENO, msg, sizeof(msg) - 1);
    _exit(EXIT_FAILURE);
}

static int rand_percent(void)
{
    int limit = RAND_MAX - (RAND_MAX % 100);
    int r;
    do { r = rand(); } while (r >= limit);
    return r % 100;
}

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


int main(int argc, char **argv) {
    if (argc != 3) {
        const char msg[] = "Correct program usage: ./parent [path1] [path2]\n";
		write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    int pipe1[2];
	if (pipe(pipe1) == -1) {
		const char msg[] = "error: failed to create pipe\n";
		write(STDERR_FILENO, msg, sizeof(msg) - 1);
		exit(EXIT_FAILURE);
	}

	int pipe2[2];
	if (pipe(pipe2) == -1) {
		const char msg[] = "error: failed to create pipe\n";
		write(STDERR_FILENO, msg, sizeof(msg) - 1);
		exit(EXIT_FAILURE);
	}

    pid_t child1 = init_child(argv[1], pipe1, pipe2);
    pid_t child2 = init_child(argv[2], pipe2, pipe1);

    close(pipe1[0]);
    close(pipe2[0]);

    char line[8192];
    size_t len = 0;
    int at_line_start = 1;

    char chunk[4096];
    for (;;) {
        ssize_t n = read(STDIN_FILENO, chunk, sizeof chunk);
        if (n < 0) {
            if (errno == EINTR) continue;
            const char msg[] = "error: failed to read stdin\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }
        if (n == 0) break;

        for (ssize_t i = 0; i < n; ++i) {
            char c = chunk[i];

            if (c == '\n') {
                int fd = (rand_percent() < 80) ? pipe1[1] : pipe2[1];
                if (write_all(fd, line, len) == -1 ||
                    write_all(fd, "\n", 1) == -1) {
                    const char msg[] = "error: failed to write to pipe\n";
                    write(STDERR_FILENO, msg, sizeof(msg) - 1);
                    exit(EXIT_FAILURE);
                }
                len = 0;
                at_line_start = 1;
            } else {
                if (len < sizeof(line) - 1) {
                    line[len++] = c;
                } else {
                    const char msg[] = "error: line too long\n";
                    write(STDERR_FILENO, msg, sizeof(msg) - 1);
                    exit(EXIT_FAILURE);
                }
                at_line_start = 0;
            }
        }
    }

    if (len > 0 || !at_line_start) {
        int fd = (rand_percent() < 80) ? pipe1[1] : pipe2[1];
        if (write_all(fd, line, len) == -1 ||
            write_all(fd, "\n", 1) == -1) {
            const char msg[] = "error: failed to write to pipe\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }
    }

    close(pipe1[1]);
    close(pipe2[1]);
    
    waitpid(child1, NULL, 0);
    waitpid(child2, NULL, 0);

    exit(EXIT_SUCCESS);
}