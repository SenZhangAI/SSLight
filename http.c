#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

#define HTTP_PORT 80

int parse_url(char *uri, char **host, char **path)
{
    char *pos;
    pos = strstr(uri, "//");
    if (!pos) {
        return -1;
    }

    *host = pos + 2;
    pos = strchr(*host, '/');
    if (!pos) {
        *path = NULL;
    } else {
        *pos = '\0';
        *path = pos + 1;
    }

    return 0;
}

#define MAX_GET_COMMAND 255
int http_get(int connection, const char *path, const char *host)
{
    static char get_command[MAX_GET_COMMAND];

    sprintf(get_command, "GET /%s HTTP/1.1\r\n", path);
    if (send(connection, get_command, strlen(get_command), 0) == -1) {
        return -1;
    }

    sprintf(get_command, "Host: %s\r\n", host);
    if (send(connection, get_command, strlen(get_command), 0) == -1) {
        return -1;
    }
    sprintf(get_command, "Connection: close\r\n\r\n");
    if (send(connection, get_command, strlen(get_command), 0) == -1) {
        return -1;
    }
    return 0;
}

#define BUFFER_SIZE 255
void display_result(int connection)
{
    int received = 0;
    static char recv_buf[BUFFER_SIZE+1];
    while ((received = recv(connection, recv_buf, BUFFER_SIZE, 0)) > 0) {
        recv_buf[received] = '\0';
        printf("%s", recv_buf);
    }

    printf("\n");
}

int main(int argc, char *argv[])
{
    int client_conn;
    char *host, *path;
    struct hostent *host_name;
    struct sockaddr_in host_addr;
#ifdef WIN32
    WSADATA wsaData;
#endif
    if (argc < 2) {
        fprintf(stderr, "Usage: %s: <URL>\n", argv[0]);
        return 1;
    }

    if (parse_url(argv[1], &host, &path) == -1) {
        fprintf(stderr, "Error - malformed URL '%s'.\n", argv[1]);
        return 1;
    }

    printf("Connecting to host '%s'\n", host);

#ifdef WIN32
    if(WSAStartup(MAKEWORD(2, 2), &wsaData) != NO_ERROR) {
        fprintf(stderr, "Error, unable to initialize winsock.\n");
        return 2;
    }
#endif

    client_conn = socket(PF_INET, SOCK_STREAM, 0);

    if (!client_conn) {
        perror("Unable to create local socket.");
        return 2;
    }

    host_name = gethostbyname(host);

    if (!host_name) {
        perror("Error in name resolution.");
        return 3;
    }

    host_addr.sin_family = AF_INET;
    host_addr.sin_port = htons(HTTP_PORT);
    memcpy(&host_addr.sin_addr, host_name->h_addr_list[0],
           sizeof(struct in_addr));

    if (connect(client_conn, (struct sockaddr *) &host_addr, sizeof(host_addr)) == -1) {
        perror("Unable to connect to host.");
        return 4;
    }

    printf("Retrieving document: '%s'\n", path);

    http_get(client_conn, path, host);
    display_result(client_conn);
    printf("Shutting down.\n");

#ifdef WIN32
    if (closesocket(client_conn) == -1)
#else
    if (close(client_conn) == -1)
#endif
    {
        perror("Error closing client connection");
        return 5;
    }
#ifdef WIN32
    WSACleanup();
#endif
    return 0;
}

