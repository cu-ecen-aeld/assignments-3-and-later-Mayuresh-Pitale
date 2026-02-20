/*
* AESD Socket Server
*Author: Mayuresh Pitale
*Date: 02/13/2026 
*References: https://gemini.google.com/share/af53bf20979b
*/
/*
* AESD Socket Server
* Author: Mayuresh Pitale
* Date: 02/13/2026 
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
#include <pthread.h>    // POSIX threads
#include <sys/queue.h>  // SLIST macros
#include <time.h>       // POSIX timers

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

// --- Globals ---
int server_fd = -1;
volatile sig_atomic_t signal_caught = 0;
pthread_mutex_t file_mutex;

// --- Function Prototypes ---
ssize_t write_all(int fd, const void *buf, size_t count);
ssize_t send_all(int sock, const void *buf, size_t len);

// --- Linked List Node Definition ---
typedef struct thread_data_s {
    pthread_t thread_id;
    int client_fd;
    bool thread_complete;
    SLIST_ENTRY(thread_data_s) entries;
} thread_data_t;

// Define the head of the list
SLIST_HEAD(thread_list_head, thread_data_s) head;

// --- Helper Functions ---

// Helper to perform full writes
ssize_t write_all(int fd, const void *buf, size_t count) {
    size_t total = 0;
    const char *p = buf;
    while (total < count) {
        ssize_t w = write(fd, p + total, count - total);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total += w;
    }
    return total;
}

// Helper to perform full sends
ssize_t send_all(int sock, const void *buf, size_t len) {
    size_t total = 0;
    const char *p = buf;
    while (total < len) {
        ssize_t s = send(sock, p + total, len - total, MSG_NOSIGNAL);
        if (s < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total += s;
    }
    return total;
}

// --- Thread Functions ---

// Timer thread function
void timer_thread(union sigval sigval) {
    char time_str[100];
    char outstr[200];
    time_t t;
    struct tm *tmp;

    time(&t);
    tmp = localtime(&t);
    if (tmp == NULL) return;

    strftime(time_str, sizeof(time_str), "%a, %d %b %Y %T %z", tmp); // RFC 2822 format
    snprintf(outstr, sizeof(outstr), "timestamp:%s\n", time_str);

    // Lock mutex before writing to the shared file
    if (pthread_mutex_lock(&file_mutex) == 0) {
        int fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd >= 0) {
            write_all(fd, outstr, strlen(outstr));
            close(fd);
        }
        pthread_mutex_unlock(&file_mutex);
    }
}

// Client connection thread handler
void* thread_handler(void* thread_param) {
    thread_data_t* data = (thread_data_t*)thread_param;
    char* packet_buffer = NULL;
    size_t current_buffer_size = BUFFER_SIZE;
    size_t total_received = 0;
    ssize_t bytes_received;

    packet_buffer = (char*)malloc(current_buffer_size);
    if (!packet_buffer) goto cleanup_thread;

    // Receive Loop
    while ((bytes_received = recv(data->client_fd, packet_buffer + total_received, 
                                  current_buffer_size - total_received, 0)) > 0) {
        
        total_received += bytes_received;
        
        // Check for newline
        if (memchr(packet_buffer + total_received - bytes_received, '\n', bytes_received) != NULL) {
            
            // CRITICAL SECTION: Lock mutex for File I/O
            pthread_mutex_lock(&file_mutex);
            
            // 1. Write packet to file
            int file_fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (file_fd >= 0) {
                write_all(file_fd, packet_buffer, total_received);
                close(file_fd);
            }

            // 2. Read full file and send back
            file_fd = open(DATA_FILE, O_RDONLY);
            if (file_fd >= 0) {
                char read_buf[BUFFER_SIZE];
                ssize_t read_bytes;
                while ((read_bytes = read(file_fd, read_buf, sizeof(read_buf))) > 0) {
                    send_all(data->client_fd, read_buf, read_bytes);
                }
                close(file_fd);
            }
            
            pthread_mutex_unlock(&file_mutex);
            // END CRITICAL SECTION

            total_received = 0; // Reset for next packet
        } 
        else if (total_received == current_buffer_size) {
            current_buffer_size *= 2;
            char *tmp = realloc(packet_buffer, current_buffer_size);
            if (!tmp) break; 
            packet_buffer = tmp;
        }
    }

cleanup_thread:
    if (packet_buffer) free(packet_buffer);
    close(data->client_fd);
    data->thread_complete = true;
    return NULL;
}

// --- System Functions ---

void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        signal_caught = 1;
        if (server_fd != -1) shutdown(server_fd, SHUT_RDWR);
    }
}

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
    if (chdir("/") < 0) syslog(LOG_ERR, "Could not chdir to /");

    long maxfd = sysconf(_SC_OPEN_MAX);
    if (maxfd < 0) maxfd = 1024;
    for (int fd = (int)maxfd; fd >= 0; fd--) {
        if (fd != server_fd) close(fd);
    }

    int devnull = open("/dev/null", O_RDWR);
    if (devnull != -1) {
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        if (devnull > 2) close(devnull);
    }
}

// --- Main ---

int main(int argc, char *argv[]) {
    bool daemon_mode = (argc == 2 && strcmp(argv[1], "-d") == 0);
    struct sockaddr_in address;
    int optval = 1;

    // Init mutex and list
    pthread_mutex_init(&file_mutex, NULL);
    SLIST_INIT(&head);

    // Setup signals
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN); // Ignore broken pipes

    // Create and bind socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return -1;

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        close(server_fd);
        return -1;
    }

    if (daemon_mode) make_daemon();

    openlog("aesdsocket", LOG_PID, LOG_USER);

    if (listen(server_fd, 10) < 0) {
        syslog(LOG_ERR, "Listen failed");
        goto cleanup;
    }

    // Setup POSIX timer
    timer_t timer_id;
    bool timer_created = false;
    struct sigevent sev;
    struct itimerspec its;

    memset(&sev, 0, sizeof(struct sigevent));
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = timer_thread;
    
    if (timer_create(CLOCK_REALTIME, &sev, &timer_id) == 0) {
        timer_created = true;
        its.it_value.tv_sec = 10;
        its.it_value.tv_nsec = 0;
        its.it_interval.tv_sec = 10;
        its.it_interval.tv_nsec = 0;
        timer_settime(timer_id, 0, &its, NULL);
    }

    // Main accept loop
    while (!signal_caught) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int new_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (new_fd < 0) {
            if (signal_caught) break;
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        thread_data_t *new_node = (thread_data_t *)malloc(sizeof(thread_data_t));
        if (new_node == NULL) {
            syslog(LOG_ERR, "Malloc failed");
            close(new_fd);
            continue;
        }

        new_node->client_fd = new_fd;
        new_node->thread_complete = false;

        if (pthread_create(&new_node->thread_id, NULL, thread_handler, new_node) != 0) {
            syslog(LOG_ERR, "Thread creation failed");
            free(new_node);
            close(new_fd);
            continue;
        }

        SLIST_INSERT_HEAD(&head, new_node, entries);

        // Reap finished threads manually (safe traversal without macro)
        thread_data_t *cursor = SLIST_FIRST(&head);
        while (cursor != NULL) {
            thread_data_t *temp = SLIST_NEXT(cursor, entries); // Save next before modifying
            if (cursor->thread_complete) {
                pthread_join(cursor->thread_id, NULL);
                SLIST_REMOVE(&head, cursor, thread_data_s, entries);
                free(cursor);
            }
            cursor = temp;
        }
    }

cleanup:
    // Join all remaining threads gracefully
    while (!SLIST_EMPTY(&head)) {
        thread_data_t *elem = SLIST_FIRST(&head);
        shutdown(elem->client_fd, SHUT_RDWR); // Force blocking recv() to exit
        pthread_join(elem->thread_id, NULL);
        SLIST_REMOVE_HEAD(&head, entries);
        free(elem);
    }

    if (timer_created) timer_delete(timer_id);
    pthread_mutex_destroy(&file_mutex);
    if (server_fd != -1) close(server_fd);
    unlink(DATA_FILE);
    closelog();
    
    return 0;
}