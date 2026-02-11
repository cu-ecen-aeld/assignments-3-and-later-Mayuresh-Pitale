#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/stat.h>

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

int server_fd = -1;
int client_fd = -1;
bool signal_caught = false;

void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        signal_caught = true;
        if (server_fd != -1) shutdown(server_fd, SHUT_RDWR);
    }
}

// Helper to make the program a daemon
void make_daemon() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) exit(EXIT_FAILURE);

    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    umask(0);
    if (chdir("/") < 0) {
        syslog(LOG_ERR, "Could not chdir to /");
    }

    // Close all open file descriptors EXCEPT the server socket
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x >= 0; x--) {
        if (x != server_fd) { // PROTECT THE SOCKET
            close(x);
        }
    }
    
    // Redirect stdin/out/err to /dev/null
    int devnull = open("/dev/null", O_RDWR);
    if (devnull != -1) {
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        if (devnull > 2) close(devnull);
    }
}

// Helper to send the entire file content to the client
void send_file_content(int client_socket) {
    int file_fd = open(DATA_FILE, O_RDONLY);
    if (file_fd < 0) {
        syslog(LOG_ERR, "Failed to open data file for reading");
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        if (send(client_socket, buffer, bytes_read, 0) == -1) {
            syslog(LOG_ERR, "Failed to send data to client");
            break;
        }
    }
    close(file_fd);
}

int main(int argc, char *argv[]) {
    bool daemon_mode = false;
    struct sockaddr_in address;
    int opt = 1;

    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = true;
    }

    // Always start with a clean slate
    unlink(DATA_FILE);

    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    if (sigaction(SIGINT, &sa, NULL) != 0 || sigaction(SIGTERM, &sa, NULL) != 0) {
        perror("Signal setup failed");
        closelog();
        return -1;
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        closelog();
        return -1;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        close(server_fd);
        closelog();
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        closelog();
        return -1;
    }

    if (daemon_mode) {
        make_daemon();
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        close(server_fd);
        closelog();
        return -1;
    }

    while (!signal_caught) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (signal_caught) break;
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        //RECEIVE LOOP
        size_t current_buffer_size = BUFFER_SIZE;
        char *packet_buffer = malloc(current_buffer_size);
        if (!packet_buffer) {
            close(client_fd);
            continue;
        }

        size_t total_received = 0;
        ssize_t bytes_received;
        bool newline_found = false;

        while ((bytes_received = recv(client_fd, packet_buffer + total_received, current_buffer_size - total_received, 0)) > 0) {
            total_received += bytes_received;

            if (memchr(packet_buffer + total_received - bytes_received, '\n', bytes_received) != NULL) {
                newline_found = true;
                break;
            }

            if (total_received == current_buffer_size) {
                current_buffer_size *= 2;
                char *new_ptr = realloc(packet_buffer, current_buffer_size);
                if (!new_ptr) {
                    syslog(LOG_ERR, "Realloc failed");
                    newline_found = false;
                    break;
                }
                packet_buffer = new_ptr;
            }
        }

        // WRITE LOOP
        if (newline_found) {
            int file_fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (file_fd < 0) {
                syslog(LOG_ERR, "Could not open file for writing");
            } else {
                if (write(file_fd, packet_buffer, total_received) != total_received) {
                    syslog(LOG_ERR, "Partial write or error");
                }
                close(file_fd);
            }
            
            send_file_content(client_fd);
        }

        free(packet_buffer);
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
        close(client_fd);
    }

    if (server_fd != -1) close(server_fd);
    unlink(DATA_FILE);
    closelog();
    
    return 0;
}
