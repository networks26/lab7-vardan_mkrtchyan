# Network programming lab: two ways to serve TCP clients

## Part 1: A server that forks

In this lab we will use `fork()` for an old Unix programming technique: **one child process for every incoming TCP connection**.

Most of `server.c` is already written. **Replace only the six `???` expressions.** You do not need to write your own signal handlers; read the supplied ones and see what they do. Use `man` program with function names. For example: `man signal`.

Your server will:

- Listen on **127.0.0.1**, TCP port **23000** IS HARDCODED! CHANGE IT!.
- Call `accept()` to wait for a connection.
- Call `fork()`: the child reads a message and replies; the parent waits for another client.
- Catch Ctrl+C (`SIGINT`), print `SIGINT caught`, close the listening socket, and exit cleanly.
- Handle `SIGCHLD` so exited children are collected rather than left as zombies.

This server deliberately uses **blocking sockets**. It does not use `poll()` or `select()`. We will look at a different approach in Part 2.

> `127.0.0.1` means the client must run on the **same machine** as the server. If you are connected to a student server over SSH, open multiple SSH sessions to that server. If many students work on the same computer, change `PORT` in **both** C files to a port not used by another student, then rebuild.

### Task: complete `server.c`

Find six `???` expressions and replace each with one C expression.

| TODO | What is missing? | Read |
|---|---|---|
| 1 | Register the already defined handler for `SIGINT` | `man 2 signal` |
| 2 | Create an IPv4 TCP socket | `man 2 socket` |
| 3 | Bind the socket to the prepared `address` | `man 2 bind` |
| 4 | Begin listening with `BACKLOG` | `man 2 listen` |
| 5 | Accept a client (we do not need the client's address) | `man 2 accept` |
| 6 | Create a child process | `man 2 fork` |

(Here 2 is the section of manual collection)

Clues: `AF_INET`, `SOCK_STREAM`, `server_fd`, `address`, `BACKLOG`. For `bind()`, you need a pointer to `address` and its size.

The existing `SIGINT` handler sets a flag, closes the listening descriptor and writes a message using `write()` (not `printf()`). The main loop then exits normally. The existing `SIGCHLD` handler calls `waitpid()` with `WNOHANG` to collect children that have finished.

### Compile and try one client

After completing the TODOs:

```sh
make
./server
```

In another terminal on that same machine:

```sh
nc 127.0.0.1 23000 (or whatever port you have used)
```

Type `hello`, press Enter. Example response:

```text
child 12346 received: hello
```

The PID will be different on your machine. The child then closes that connection.

### Test two clients and look at the processes

**Do not skip this part: `ps` or `pstree` is required.**

1. Terminal A: run `./server`. Note its **parent PID**, printed in the first line, for example `12345`.
2. Terminal B: run `nc 127.0.0.1 23000`, but **do not type anything yet**. That child is blocked in `read()`.
3. Terminal C: run another `nc 127.0.0.1 23000`, also without typing. There should now be **two children**.
4. Terminal D: inspect the process tree **while both clients are still connected**. Replace `12345` with your actual server PID:

```sh
pstree -p 12345
```

Or, using `ps`:

```sh
ps --forest -o pid,ppid,stat,cmd -p 12345 --ppid 12345
```

Example of what you may see:

```text
server(12345)-+-server(12346)
              `-server(12347)
```

These are **example PIDs**. You should see one parent server and two child servers. If `pstree` is unavailable, use `ps`; `pstree` is normally supplied by the `psmisc` package.

5. In terminal C, type `second` and press Enter. You should get a response even though B is still waiting.
6. In terminal D, run `pstree` or `ps` again. How many children are left?
7. In terminal B, type `first` and press Enter. Try the command again. What happened to the children?
8. In terminal A, press **Ctrl+C**. Check for:

```text
SIGINT caught
parent: listening socket closed, server exiting
```

It must return to the shell. Do not use `kill -9` to stop the server.

## Part 2: A server with `poll()` and no `fork()`

This part is **already implemented for you** in `poll_server.c`. **Do not edit this source file.** Your job is to build it, run it, and compare its behavior with Part 1.

Stop `./server` before trying this example: **both programs use the same port**.

```sh
make poll_server
./poll_server
```

Open terminals B and C again, each running:

```sh
nc 127.0.0.1 23000
```

Leave both terminals waiting, without typing. In terminal D, inspect the server process by PID using the **same** `pstree` or `ps` command as before.

- Do you see any child processes under `poll_server`?
- Type `second` in terminal C while B is still idle. Does C receive a reply?
- Type `first` in terminal B. Does B receive a reply?
- Inspect the processes again. Does the number of server processes change?
- Press Ctrl+C in the `poll_server` terminal to close its sockets and exit.

The replies now contain `server PID received: ...` instead of `child PID received: ...`. **Both clients are served by the same PID.**

### What is different?

In `server.c`, a blocking `accept()` waits for each new connection, then `fork()` creates a **separate process** to handle it.

In `poll_server.c`, a single process keeps an **array of sockets**. `poll()` waits until the listening socket or one of the clients is ready. The program then handles whichever socket needs attention. No `fork()` is used.

Find these lines in the provided `poll_server.c`:

```c
struct pollfd fds[MAX_CLIENTS + 1];
```

```c
poll(fds, MAX_CLIENTS + 1, -1)
```

The array contains one listening socket plus up to `MAX_CLIENTS` connected client sockets. `poll()` tells the program which ones are ready to read; it does not create processes.

This demonstration handles only one short message per connection. Real network programs also need to deal with partial messages and writes, slow clients, and other issues. Those are outside this exercise.

## Short questions (`answers.txt`)

1. Why does the forking server's **child** close the listening socket, while the **parent** closes the accepted client socket?
2. What does `SIGCHLD` mean? Why does the handler call `waitpid()` with `WNOHANG`?
3. What does `EINTR` mean? What should happen if Ctrl+C arrives while the server is waiting in `accept()`?
4. What did `pstree` or `ps` show when **two clients** were connected to `server`? What did it show for `poll_server`?
5. How can `poll_server` serve two connected clients without calling `fork()`?

## Submission

Submit:

- Your completed `server.c` and the provided `Makefile`.
- Your `answers.txt` with short answers to the five questions.
- The output of **`pstree -p PID` or the `ps` command above**, captured while **both clients are still connected to the forking server**.
- The equivalent process output for **`poll_server`**, and the Ctrl+C shutdown output.

The `poll_server.c` file is supplied unchanged; you do not need to write it yourself.

## Optional bonus

Modify `server.c` so that each child keeps receiving and replying until the client disconnects. The supplied server intentionally calls `read()` only once; remember that TCP is a byte stream, not a sequence of messages.
