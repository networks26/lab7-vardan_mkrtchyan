#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 23000
#define BACKLOG 5
#define MAX_CLIENTS 16
#define BUFFER_SIZE 512

/* This is a complete example. There are no TODOs in this file. */

static volatile sig_atomic_t stopping = 0;
static volatile sig_atomic_t listening_fd = -1;

static void on_sigint(int signum)
{
    int saved_errno = errno;
    const char message[] = "\nSIGINT caught\n";
    int fd;

    (void)signum;
    stopping = 1;
    fd = (int)listening_fd;
    listening_fd = -1;
    if (fd >= 0) {
        (void)close(fd);
    }
    (void)write(STDERR_FILENO, message, sizeof(message) - 1);
    errno = saved_errno;
}

int main(void)
{
    struct sockaddr_in address;
    struct pollfd fds[MAX_CLIENTS + 1];
    char buffer[BUFFER_SIZE];
    int server_fd, client_fd, reuse = 1, i, slot;
    int result = 0;
    ssize_t n;

    if (signal(SIGINT, on_sigint) == SIG_ERR) {
        perror("signal");
        return 1;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
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

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }
    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    /* Slot 0 is the listening socket. Other slots are for clients. */
    for (i = 0; i <= MAX_CLIENTS; i++) {
        fds[i].fd = -1;             /* -1 means this slot is free. */
        fds[i].events = POLLIN;     /* Tell us when data is ready to read. */
        fds[i].revents = 0;
    }
    fds[0].fd = server_fd;
    listening_fd = server_fd;

    printf("server %ld listening on 127.0.0.1:%d (poll, no fork)\n",
           (long)getpid(), PORT);
    fflush(stdout);

    while (!stopping) {
        /* Wait until some socket can be read without blocking. */
        if (poll(fds, MAX_CLIENTS + 1, -1) < 0) {
            if (stopping) {
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            result = 1;
            break;
        }
        if (stopping) {
            break;
        }

        /* A new connection is waiting on the listening socket. */
        if (fds[0].revents & POLLIN) {
            client_fd = accept(server_fd, NULL, NULL);
            if (client_fd < 0) {
                if (stopping) {
                    break;
                }
                if (errno != EINTR) {
                    perror("accept");
                }
            } else {
                slot = -1;
                for (i = 1; i <= MAX_CLIENTS; i++) {
                    if (fds[i].fd == -1) {
                        slot = i;
                        break;
                    }
                }
                if (slot == -1) {
                    /* This demonstration has a fixed client limit. */
                    close(client_fd);
                } else {
                    fds[slot].fd = client_fd;
                    fds[slot].events = POLLIN;
                    printf("server: accepted client in slot %d\n", slot);
                    fflush(stdout);
                }
            }
        }

        /* Read whichever clients have sent data. No child processes. */
        for (i = 1; i <= MAX_CLIENTS; i++) {
            if (fds[i].fd == -1) {
                continue;
            }
            if (!(fds[i].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL))) {
                continue;
            }

            n = read(fds[i].fd, buffer, sizeof(buffer) - 1);
            if (n > 0) {
                buffer[n] = '\0';
                (void)dprintf(fds[i].fd, "server %ld received: %s",
                              (long)getpid(), buffer);
            } else if (n < 0) {
                perror("read");
            }
            /* One message per client, just like the forking server. */
            close(fds[i].fd);
            fds[i].fd = -1;
        }
    }

    /* The handler closes the listening socket; close client sockets here. */
    if (listening_fd >= 0) {
        listening_fd = -1;
        close(server_fd);
    }
    for (i = 1; i <= MAX_CLIENTS; i++) {
        if (fds[i].fd >= 0) {
            close(fds[i].fd);
        }
    }
    printf("server: sockets closed, exiting\n");
    return result;
}
