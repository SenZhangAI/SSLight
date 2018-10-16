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

int parse_proxy_param(char *proxy_spec,
                      char **proxy_host,
                      int *proxy_port,
                      char **proxy_user,
                      char **proxy_password)
{
    char *login_sep, *colon_sep, *trailer_sep;
    if (!strncmp("http://", proxy_spec, 7)) {
        proxy_spec += 7;
    }

    login_sep = strchr(proxy_spec, '@');
    if (login_sep) {
        colon_sep = strchr(proxy_spec, ':');
        if (!colon_sep || (colon_sep > login_sep)) {
            fprintf(stderr, "Expected password in '%s'\n, proxy_spec");
            return 0;
        }
        *colon_sep = '\0';
        *proxy_user = proxy_spec;
        *login_sep = '\0';
        *proxy_password = colon_sep + 1;
        proxy_spec = login_sep + 1;
    }
    tailer_sep = strchr(proxy_spec, '/');
    if (tailer_sep) {
        *tailer_sep = '\0';
    }
    colon_sep = strchr(proxy_spec, ':');
    if (colon_sep) {
        *colon_sep = '\0';
        *proxy_host = proxy_spec;
        *proxy_port = atoi(colon_sep + 1);
        if (*proxy_port == 0) {
            return 0;
        }
    } else {
        *proxy_port = HTTP_PORT;
        *proxy_host = proxy_spec;
    }
    return 1;
}

#define MAX_GET_COMMAND 255
int http_get(int connection,
             const char *path,
             const char *host,
             const char *proxy_host,
             const char *proxy_user,
             const char *proxy_password)
{
    static char get_command[MAX_GET_COMMAND];
    if (proxy_host) {
        sprintf(get_command, "GET http://%s/%s HTTP/1.1\r\n", host, path);
    } else {
        sprintf(get_command, "GET /%s HTTP/1.1\r\n", path);
    }

    sprintf(get_command, "GET /%s HTTP/1.1\r\n", path);
    if (send(connection, get_command, strlen(get_command), 0) == -1) {
        return -1;
    }

    sprintf(get_command, "Host: %s\r\n", host);
    if (send(connection, get_command, strlen(get_command), 0) == -1) {
        return -1;
    }

    if (proxy_user) {
        int credentials_len = strlen(proxy_user) + strlen(proxy_password) + 1;
        char *proxy_credentials = malloc(credentials_len);
        char *auth_string = malloc(((credentials_len * 4) / 3) + 1);
        sprintf(proxy_credentials, "%s:%s", proxy_user, proxy_password);
        base64_encode(proxy_credentials, credentials_len, auth_string);

        if (send(connection, get_command, strlen(get_command), 0) == -1) {
            free(proxy_credentials);
            free(auth_string);
            return -1;
        }

        free(proxy_credentials);
        free(auth_string);
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
    char *proxy_host, *proxy_user, *proxy_password;
    int proxy_port;
    char *host, *path;
    struct hostent *host_name;
    struct sockaddr_in host_addr;
    int idx;
#ifdef WIN32
    WSADATA wsaData;
#endif
    if (argc < 2) {
        fprintf(stderr, "Usage: %s: [-p http://[username:password@]proxy-host:proxy-port] <URL>\n", argv[0]);
        return 1;
    }
    proxy_host = proxy_user = proxy_password = host = path = NULL;
    int d = 1;
    if (!strcmp("-p", argv[idx])) {
        if (!parse_proxy_param(argv[++idx], &proxy_host, &proxy_port,
                               &proxy_user, &proxy_password)) {
            fprintf(stderr, "Error - malformed proxy parameter '%s'.\n", argv[2]);
            return 2;
        }
        idx++;
    }

    if (parse_url(argv[idx], &host, &path) == -1) {
        fprintf(stderr, "Error - malformed URL '%s'.\n", argv[1]);
        return 1;
    }
    if (proxy_host) {
        printf("\033[32mConnecting to host '%s'\n\033[0m", proxy_host);
        host_name = gethostbyname(proxy_host);
    } else {
        printf("\033[32mConnecting to host '%s'\n\033[0m", host);
        host_name = gethostbyname(host);
    }

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

    if (!host_name) {
        perror("Error in name resolution.");
        return 3;
    }

    host_addr.sin_family = AF_INET;
    host_addr.sin_port = htons(proxy_host ? proxy_port : HTTP_PORT);
    memcpy(&host_addr.sin_addr, host_name->h_addr_list[0],
           sizeof(struct in_addr));

    if (connect(client_conn, (struct sockaddr *) &host_addr, sizeof(host_addr)) == -1) {
        perror("Unable to connect to host.");
        return 4;
    }

    printf("\033[32mRetrieving document: '%s'\n\033[0m", path);

    http_get(client_conn, path, host, proxy_host, proxy_user, proxy_password);
    display_result(client_conn);
    printf("\033[32mShutting down.\n\033[0m");

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

