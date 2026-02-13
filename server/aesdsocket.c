/*
* AESD Socket Server
*Author: Mayuresh Pitale
*Date: 02/13/2026 
*References: https://gemini.google.com/share/af53bf20979b
*/
#include <stdio.h>      // standard I/O
#include <stdlib.h>     // malloc, free, exit
#include <string.h>     // memset, strcmp, strerror
#include <unistd.h>     // close, read, write, fork, setsid
#include <syslog.h>     // syslog logging
#include <sys/types.h>  // socket types
#include <sys/socket.h> // socket API
#include <netinet/in.h> // sockaddr_in
#include <arpa/inet.h>  // inet_ntop
#include <fcntl.h>      // open, O_* flags
#include <signal.h>     // signal handling
#include <errno.h>      // errno
#include <stdbool.h>    // bool type
#include <sys/stat.h>   // file modes

#define PORT 9000                    // server port
#define DATA_FILE "/var/tmp/aesdsocketdata" // path to data file
#define BUFFER_SIZE 1024             // initial buffer chunk size

int server_fd = -1;   // listening socket descriptor
int client_fd = -1;   // accepted client socket descriptor
bool signal_caught = false; // termination signal flag

// Signal handler: mark termination and shutdown sockets
void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) { // handle termination signals
        syslog(LOG_INFO, "Caught signal, exiting"); // log signal catch
        signal_caught = true; // set termination flag
        if (server_fd != -1) shutdown(server_fd, SHUT_RDWR); // close listener
        if (client_fd != -1) shutdown(client_fd, SHUT_RDWR); // close client
    }
}

// Fork and detach process to run as a daemon
void make_daemon() {
    pid_t pid = fork(); // first fork
    if (pid < 0) exit(EXIT_FAILURE); // fork failed
    if (pid > 0) exit(EXIT_SUCCESS); // parent exits

    if (setsid() < 0) exit(EXIT_FAILURE); // create new session

    signal(SIGCHLD, SIG_IGN); // ignore child signals
    signal(SIGHUP, SIG_IGN);  // ignore hangups

    pid = fork(); // second fork
    if (pid < 0) exit(EXIT_FAILURE); // fork failed
    if (pid > 0) exit(EXIT_SUCCESS); // parent exits

    umask(0); // reset file mode creation mask
    if (chdir("/") < 0) { // change working directory to root
        syslog(LOG_ERR, "Could not chdir to /"); // log failure
    }

    long maxfd = sysconf(_SC_OPEN_MAX); // get max file descriptors
    if (maxfd < 0) maxfd = 1024; // fallback if unknown
    for (int fd = (int)maxfd; fd >= 0; fd--) { // close all fds
        if (fd != server_fd) close(fd); // keep listener protected
    }

    int devnull = open("/dev/null", O_RDWR); // open /dev/null
    if (devnull != -1) {
        dup2(devnull, STDIN_FILENO);  // redirect stdin
        dup2(devnull, STDOUT_FILENO); // redirect stdout
        dup2(devnull, STDERR_FILENO); // redirect stderr
        if (devnull > 2) close(devnull); // close extra fd
    }
}

// Helper to perform full writes (handle partial writes)
ssize_t write_all(int fd, const void *buf, size_t count) {
    size_t total = 0; // bytes written so far
    const char *p = buf;
    while (total < count) { // loop until all bytes written
        ssize_t w = write(fd, p + total, count - total); // write chunk
        if (w < 0) {
            if (errno == EINTR) continue; // retry on interrupt
            return -1; // return error
        }
        total += w; // accumulate bytes written
    }
    return total; // return total written
}

// Helper to perform full sends to socket (handle partial sends)
ssize_t send_all(int sock, const void *buf, size_t len) {
    size_t total = 0; // bytes sent so far
    const char *p = buf;
    while (total < len) { // loop until all bytes sent
        ssize_t s = send(sock, p + total, len - total, MSG_NOSIGNAL); // send chunk
        if (s < 0) {
            if (errno == EINTR) continue; // retry on interrupt
            return -1; // return error
        }
        total += s; // accumulate bytes sent
    }
    return total; // return total sent
}

// Read the entire data file and send its contents to the client
void send_file_content(int client_socket) {
    int file_fd = open(DATA_FILE, O_RDONLY); // open data file for reading
    if (file_fd < 0) {
        syslog(LOG_ERR, "Failed to open data file for reading: %s", strerror(errno)); // log error
        return;
    }

    char buffer[BUFFER_SIZE]; // temporary read buffer
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) { // read loop
        if (send_all(client_socket, buffer, (size_t)bytes_read) < 0) { // send chunk
            syslog(LOG_ERR, "Failed to send data to client: %s", strerror(errno)); // log send error
            break;
        }
    }
    if (bytes_read < 0) {
        syslog(LOG_ERR, "Error reading data file: %s", strerror(errno)); // log read error
    }
    close(file_fd); // close file descriptor
}

int main(int argc, char *argv[]) {
    bool daemon_mode = (argc == 2 && strcmp(argv[1], "-d") == 0); // check -d flag
    struct sockaddr_in address; // server address struct
    int optval = 1; // socket option value

    // 1. Setup Signal Handling FIRST
    struct sigaction sa; // sigaction struct
    memset(&sa, 0, sizeof(sa)); // clear struct
    sa.sa_handler = handle_signal; // assign handler
    sigaction(SIGINT, &sa, NULL); // register SIGINT
    sigaction(SIGTERM, &sa, NULL); // register SIGTERM
    signal(SIGPIPE, SIG_IGN); // ignore SIGPIPE to avoid termination

    // 2. Create and Bind Socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0); // create TCP socket
    if (server_fd < 0) return -1; // exit on failure

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)); // allow address reuse

    memset(&address, 0, sizeof(address)); // clear address
    address.sin_family = AF_INET; // IPv4
    address.sin_addr.s_addr = INADDR_ANY; // bind to all interfaces
    address.sin_port = htons(PORT); // set port in network byte order

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { // bind socket
        close(server_fd); // cleanup on failure
        return -1; // exit
    }

    // 3. Daemonize BEFORE opening logs if requested
    if (daemon_mode) {
        make_daemon(); // detach and run in background
    }

    // 4. Start Logging (Now safe for Daemon mode)
    openlog("aesdsocket", LOG_PID, LOG_USER); // open syslog

    if (listen(server_fd, 10) < 0) { // start listening for connections
        syslog(LOG_ERR, "Listen failed"); // log error
        goto cleanup; // jump to cleanup
    }

    while (!signal_caught) { // main accept loop
        struct sockaddr_in client_addr; // client address
        socklen_t client_len = sizeof(client_addr); // client address length

        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len); // accept connection
        if (client_fd < 0) { // accept error handling
            if (signal_caught) break; // break if terminating
            continue; // otherwise continue accepting
        }

        char client_ip[INET_ADDRSTRLEN]; // buffer for client IP string
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN); // convert IP
        syslog(LOG_INFO, "Accepted connection from %s", client_ip); // log connection

        // Packet processing
        size_t current_buffer_size = BUFFER_SIZE; // initial buffer size
        char *packet_buffer = malloc(current_buffer_size); // dynamic receive buffer
        size_t total_received = 0; // bytes currently in buffer
        ssize_t bytes_received;

        // Loop to receive data until the client closes or we find a newline
        while ((bytes_received = recv(client_fd, packet_buffer + total_received, 
                                      current_buffer_size - total_received, 0)) > 0) {
            
            size_t old_total = total_received; // track where new data starts
            total_received += bytes_received; // update total received

            // Check the NEWLY received chunk for a newline
            char *newline_ptr = memchr(packet_buffer + old_total, '\n', bytes_received); // search for newline
            
            if (newline_ptr != NULL) { // complete packet received
                int file_fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644); // open file for append
                if (file_fd >= 0) {
                    write_all(file_fd, packet_buffer, total_received); // write entire packet
                    close(file_fd); // close file after write
                    send_file_content(client_fd); // send file contents back to client
                }
                total_received = 0; // reset buffer for next packet
            } else if (total_received >= current_buffer_size) { // need more space
                current_buffer_size += BUFFER_SIZE; // grow buffer in chunks
                packet_buffer = realloc(packet_buffer, current_buffer_size); // resize buffer
            }
        }

        free(packet_buffer); // free receive buffer
        syslog(LOG_INFO, "Closed connection from %s", client_ip); // log close
        close(client_fd); // close client socket
        client_fd = -1; // reset client fd
    }

cleanup:
    if (server_fd != -1) close(server_fd); // close server socket if open
    unlink(DATA_FILE); // remove data file on exit
    syslog(LOG_INFO, "Cleaning up and exiting"); // final log
    closelog(); // close syslog
    return 0; // exit
}