#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PORT 23000
#define BACKLOG 5
#define BUFFER_SIZE 512

static volatile sig_atomic_t stopping = 0;
static volatile sig_atomic_t listening_fd = -1;

/* Ctrl+C sends SIGINT. A signal handler should do very little. */
static void on_sigint(int signum)
{
    int saved_errno = errno;
    const char message[] = "\nSIGINT caught\n";

    (void)signum;
    stopping = 1;
    /* Closing the listening socket also handles SIGINT just before accept(). */
    int fd = (int)listening_fd;
    listening_fd = -1;
    if (fd >= 0) {
        (void)close(fd);
    }
    (void)write(STDERR_FILENO, message, sizeof(message) - 1);
    errno = saved_errno;
}

/* Collect children that have exited, so they do not remain zombies. */
static void on_sigchld(int signum)
{
    int saved_errno = errno;

    (void)signum;
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
    errno = saved_errno;
}

/* A child handles one client. This reads up to BUFFER_SIZE - 1 bytes. */
static void serve_client(int client_fd)
{
    char buffer[BUFFER_SIZE];
    ssize_t n;

    n = read(client_fd, buffer, sizeof(buffer) - 1);
    if (n < 0) {
        perror("read");
        return;
    }
    if (n == 0) {
        return;   /* Client closed without sending anything. */
    }

    buffer[n] = '\0';
    (void)dprintf(client_fd, "child %ld received: %s", (long)getpid(), buffer);
}

int main(void)
{
    struct sockaddr_in address;
    int server_fd;
    int client_fd;
    int reuse = 1;
    int result = 0;
    pid_t pid;

    /* TODO 1: Register the function that handles Ctrl+C (SIGINT). */
    if (signal(SIGINT, ???) == SIG_ERR) {
        perror("signal SIGINT");
        return 1;
    }

    /* Reap dead children so they do not become zombies. */
    if (signal(SIGCHLD, on_sigchld) == SIG_ERR) {
        perror("signal SIGCHLD");
        return 1;
    }

    /* TODO 2: Create an IPv4 TCP socket (see man 2 socket). */
    server_fd = ???;
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    /* TODO 3: Give the socket its local address (see man 2 bind). */
    if (??? < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    /* TODO 4: Start listening (see man 2 listen). */
    if (??? < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("parent %ld listening on 127.0.0.1:%d\n",
           (long)getpid(), PORT);
    fflush(stdout);

    listening_fd = server_fd;

    while (!stopping) {
        /* TODO 5: Accept one connection (see man 2 accept). */
        client_fd = ???;
        /* SIGINT might have arrived while (or just before) accepting. */
        if (stopping) {
            if (client_fd >= 0) {
                close(client_fd);
            }
            break;
        }
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;   /* Interrupted by a signal. */
            }
            perror("accept");
            result = 1;
            break;
        }

        /* TODO 6: Make a child process (see man 2 fork). */
        pid = ???;
        if (pid < 0) {
            perror("fork");
            close(client_fd);
            continue;
        }

        if (pid == 0) {
            /* Child: Ctrl+C should terminate this child normally. */
            signal(SIGINT, SIG_DFL);

            close(server_fd);           /* Child does not accept clients. */
            serve_client(client_fd);
            close(client_fd);
            _exit(0);
        }

        /* Parent: the child handles the client; parent accepts more. */
        printf("parent: started child %ld\n", (long)pid);
        fflush(stdout);
        close(client_fd);
    }

    /* SIGINT already closed it; in other cases close it here. */
    if (listening_fd >= 0) {
        listening_fd = -1;
        close(server_fd);
    }
    printf("parent: listening socket closed, server exiting\n");
    return result;
}
