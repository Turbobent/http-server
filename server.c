#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <stdbool.h>
#include <ctype.h>

#define PORT 8080
#define BUFFER_SIZE 8192
#define strcasecmp _stricmp

#pragma comment(lib, "ws2_32.lib")

// Get file extension
const char *get_file_extension(const char *file_name) {
    const char *dot = strrchr(file_name, '.');
    if (!dot || dot == file_name) {
        return "";
    }
    return dot + 1;
}

// Map extension to MIME type
const char *get_mime_type(const char *file_ext) {
    if (strcasecmp(file_ext, "html") == 0 || strcasecmp(file_ext, "htm") == 0) {
        return "text/html";
    } else if (strcasecmp(file_ext, "txt") == 0) {
        return "text/plain";
    } else if (strcasecmp(file_ext, "jpg") == 0 || strcasecmp(file_ext, "jpeg") == 0) {
        return "image/jpeg";
    } else if (strcasecmp(file_ext, "png") == 0) {
        return "image/png";
    } else {
        return "application/octet-stream";
    }
}

// Decode URL encoding (%20 → space)
char *url_decode(const char *src) {
    char *dest = (char *)malloc(strlen(src) + 1);
    char *pout = dest;
    for (; *src; src++) {
        if (*src == '%' && src[1] && src[2]) {
            int code;
            sscanf(src + 1, "%2x", &code);
            *pout++ = (char)code;
            src += 2;
        } else if (*src == '+') {
            *pout++ = ' ';
        } else {
            *pout++ = *src;
        }
    }
    *pout = '\0';
    return dest;
}

// Send HTTP response with file
void build_http_response(SOCKET client_fd, const char *file_name, const char *file_ext) {
    FILE *file = fopen(file_name, "rb");
    if (!file) {
        // 404 response
        const char *body = "<html><body><h1>404 Not Found</h1></body></html>";
        char header[256];
        int header_len = sprintf(header,
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n",
            (int)strlen(body));

        send(client_fd, header, header_len, 0);
        send(client_fd, body, (int)strlen(body), 0);
        return;
    }

    // get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    const char *mime_type = get_mime_type(file_ext);

    char header[512];
    int header_len = sprintf(header,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "\r\n",
        mime_type, file_size);

    send(client_fd, header, header_len, 0);

    // send file in chunks
    char buffer[BUFFER_SIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(client_fd, buffer, (int)bytes, 0);
    }

    fclose(file);
}

// Handle client in thread
DWORD WINAPI handle_client(LPVOID arg) {
    SOCKET client_fd = *((SOCKET *)arg);
    free(arg);

    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';

        // parse HTTP GET request
        char method[8], path[2048];
        if (sscanf(buffer, "%s %s", method, path) == 2 && strcmp(method, "GET") == 0) {
            const char *url_encoded_file_name = path[0] == '/' ? path + 1 : path;
            char *file_name = url_decode(url_encoded_file_name);

            // fallback: serve index.html if no file given
            if (strlen(file_name) == 0) {
                free(file_name);
                file_name = _strdup("index.html");
            }

            const char *file_ext = get_file_extension(file_name);
            build_http_response(client_fd, file_name, file_ext);

            free(file_name);
        }
    }

    closesocket(client_fd);
    return 0;
}

int main() {
    WSADATA wsa;
    SOCKET server_fd;

    // Start Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed to initialize Winsock. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    // Create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Could not create socket: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // Setup server address
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed: %d\n", WSAGetLastError());
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    // Listen
    if (listen(server_fd, 10) == SOCKET_ERROR) {
        printf("Listen failed: %d\n", WSAGetLastError());
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    printf("Server listening on port %d...\n", PORT);

    // Main loop
    while (1) {
        struct sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);
        SOCKET client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == INVALID_SOCKET) {
            printf("Accept failed: %d\n", WSAGetLastError());
            continue;
        }

        SOCKET *client_fd = malloc(sizeof(SOCKET));
        *client_fd = client_socket;
        CreateThread(NULL, 0, handle_client, client_fd, 0, NULL);
    }

    closesocket(server_fd);
    WSACleanup();
    return 0;
}
