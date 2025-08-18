#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <stdbool.h>

#define PORT 8080
#define BUFFER_SIZE 104857600
#define strcasecmp _stricmp

#pragma comment(lib, "ws2_32.lib")

const char *get_file_extension(const char *file_name) {
    const char *dot = strrchr(file_name, '.');
    if (!dot || dot == file_name) {
        return "";
    }
    return dot + 1;
}

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

bool case_insensitive_compare(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        if (tolower((unsigned char)*s1) != tolower((unsigned char)*s2)) {
            return false;
        }
        s1++;
        s2++;
    }
    return *s1 == *s2;
}

char *get_file_case_insensitive(const char *file_name) {
    WIN32_FIND_DATA findFileData;
    HANDLE hFind = FindFirstFile("*", &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    do {
        if (_stricmp(findFileData.cFileName, file_name) == 0) {
            FindClose(hFind);
            return _strdup(findFileData.cFileName); // return copy of name
        }
    } while (FindNextFile(hFind, &findFileData) != 0);

    FindClose(hFind);
    return NULL;
}


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

const char *get_file_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    return dot ? dot + 1 : "";
}

void build_http_response(const char *file_name, const char *file_ext, char *response, size_t *response_len) {
    const char *body = "<html><body><h1>Hello, World!</h1></body></html>";
    const char *header_format =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n";

    sprintf(response, header_format, (int)strlen(body));
    strcat(response, body);
    *response_len = strlen(response);
}

DWORD WINAPI handle_client(LPVOID arg) {
    int client_fd = *((int *)arg);
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
            const char *file_ext = get_file_extension(file_name);

            char response[BUFFER_SIZE * 2];
            size_t response_len = 0;
            build_http_response(file_name, file_ext, response, &response_len);
            send(client_fd, response, (int)response_len, 0);

            free(file_name);
        }
    }

    closesocket(client_fd);
    return 0;
}

//AF_INET: use IPv4 (vs IPv6)
//SOCK_STREAM: use TCP (vs UDP)
//INADDR_ANY: the server accepts connections from any network interface

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

        int *client_fd = malloc(sizeof(int));
        *client_fd = client_socket;
        CreateThread(NULL, 0, handle_client, client_fd, 0, NULL);
    }

    closesocket(server_fd);
    WSACleanup();
    return 0;
}
