#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define PORT 8080
#define BUFFER_SIZE 1024

void send_404(int client_socket);

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
    char *response = "HTTP/1.1 404 Not Found\r\n"
                     "Content-Type: text/html\r\n"
                     "\r\n"
                     "<h1>404 - Not Found</h1>";
    send(client_socket, response, strlen(response), 0);
}

int main() {
    signal(SIGCHLD, SIG_IGN);
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
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

    if (listen(server_socket, 5) < 0) {
        perror("Listen Failed.");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

while (1) {
    client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
    if (client_socket < 0) {
        perror("Accept Failed.");
        continue;
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        close(client_socket);
        continue;
    }
    
    if (pid == 0) {
        // CHILD PROCESS, handles this one client
        close(server_socket);  // child doesn't need the listening socket
        
        printf("Client connected (pid %d)\n", getpid());
        sleep(5);  // keep this for now, to prove concurrency works
        
        // Read request
        char buffer[BUFFER_SIZE];
        ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received < 0) {
            perror("recv failed");
            close(client_socket);
            exit(1);
        }
        buffer[bytes_received] = '\0';
        printf("Received:\n%s\n", buffer);
        
        // Parse request line
        char method[16], path[256], version[16];
        if (sscanf(buffer, "%15s %255s %15s", method, path, version) != 3) {
            send_404(client_socket);
            close(client_socket);
            exit(1);
        }
        
        // Route
        if (strcmp(path, "/") == 0) {
            send_html(client_socket, "index.html");
        } else if (strcmp(path, "/about") == 0) {
            send_html(client_socket, "about.html");
        } else if (strcmp(path, "/contact") == 0) {
            send_html(client_socket, "contact.html");
        } else {
            send_404(client_socket);
        }
        
        close(client_socket);
        printf("Client Disconnected (pid %d)\n", getpid());
        exit(0);  // child exits when done
    } else {
        // parent process - leads back to "accept"
        close(client_socket);  // parent doesn't need the client socket
    }
}

    close(server_socket);
    return 0;
}   