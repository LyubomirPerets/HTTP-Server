#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 1024

static unsigned long http_requests_total = 0;
static unsigned long http_404_total = 0;

void send_404(int client_socket);

void send_metrics(int client_socket) {
    char response[BUFFER_SIZE];
    int response_length = snprintf(
        response,
        sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain; version=0.0.4\r\n"
        "\r\n"
        "# HELP http_server_requests_total Total HTTP requests handled.\n"
        "# TYPE http_server_requests_total counter\n"
        "http_server_requests_total %lu\n"
        "# HELP http_server_404_total Total HTTP 404 responses sent.\n"
        "# TYPE http_server_404_total counter\n"
        "http_server_404_total %lu\n",
        http_requests_total,
        http_404_total
    );

    send(client_socket, response, response_length, 0);
}

void send_html(int client_socket, const char * file_path) {
    FILE * html_file = fopen(file_path, "r");
    if (html_file == NULL) {
        perror("Error opening HTML file.");
        send_404(client_socket);
        return;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    char * http_header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
    send(client_socket, http_header, strlen(http_header), 0);

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, html_file)) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }
    fclose(html_file);
}

void send_404(int client_socket) {
    http_404_total++;

    char *response = "HTTP/1.1 404 Not Found\r\n"
                     "Content-Type: text/html\r\n"
                     "\r\n"
                     "<h1>404 - Not Found</h1>";
    send(client_socket, response, strlen(response), 0);
}

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

    if (bytes_received <= 0) {
        // 0 - closed, -1 = error
        close(client_socket);
        return;
    }

    buffer[bytes_received] = '\0';
    http_requests_total++;

    char method[16], path[256], version[16];
    if (sscanf(buffer, "%15s %255s %15s", method, path, version) != 3) {
        send_404(client_socket);
        close(client_socket);
        return;
    }
    printf("Received request on fd %d: %s %s\n", client_socket, method, path);
    
    if (strcmp(path, "/") == 0) {
        send_html(client_socket, "index.html");
    } else if (strcmp(path, "/about") == 0) {
        send_html(client_socket, "about.html");
    } else if (strcmp(path, "/contact") == 0) {
        send_html(client_socket, "contact.html");
    } else if (strcmp(path, "/metrics") == 0) {
        send_metrics(client_socket);
    } else {
        send_404(client_socket);
    }
    
    close(client_socket);
    printf("Client on fd %d disconnected\n", client_socket);
}

int main() {
    signal(SIGPIPE, SIG_IGN);
    int server_socket;
    struct sockaddr_in server_addr;
    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed.");
        exit(EXIT_FAILURE);
    }
    
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed.");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind Failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_socket, 128) < 0) {
        perror("Listen Failed.");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    set_nonblocking(server_socket);
    
    // Create epoll instance
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1 failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    // Register the listening socket with epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;          // we want to know when it's readable (new connections)
    ev.data.fd = server_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &ev) < 0) {
        perror("epoll_ctl: server_socket");
        exit(EXIT_FAILURE);
    }
    
    printf("Server listening on port %d (epoll mode)...\n", PORT);
    
    #define MAX_EVENTS 64
    struct epoll_event events[MAX_EVENTS];
    
    // The main event loop
    while (1) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n < 0) {
            perror("epoll_wait failed");
            break;
        }
        
        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            
            if (fd == server_socket) {
                // New incoming connection
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_socket = accept(server_socket, 
                                            (struct sockaddr *)&client_addr, 
                                            &client_len);
                if (client_socket < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("accept failed");
                    }
                    continue;
                }
                set_nonblocking(client_socket);
                
                struct epoll_event client_ev;
                client_ev.events = EPOLLIN;
                client_ev.data.fd = client_socket;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &client_ev) < 0) {
                    perror("epoll_ctl: client_socket");
                    close(client_socket);
                    continue;
                }
                printf("Accepted new connection on fd %d\n", client_socket);
            } else {
                // Existing client has data ready
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);  // unregister
                handle_client(fd);                              // handle + close
            }
        }
    }
    
    close(server_socket);
    close(epoll_fd);
    return 0;
}   
