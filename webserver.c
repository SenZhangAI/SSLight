#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef WIN32
#    include <windows.h>
#    include <winsock2.h>
#else
#    include <netdb.h>
#    include <netinet/in.h>
#    include <sys/socket.h>
#    include <unistd.h>
#endif

#define HTTP_PORT 80

#define DEFAULT_LINE_LEN 255

char *read_line(int connection)
{
    static int   line_len = DEFAULT_LINE_LEN;
    static char *line     = NULL;
    int          size;
    char         c;
    int          pos = 0;

    if (!line) {
        line = malloc(line_len);
    }

    while ((size = recv(connection, &c, 1, 0)) > 0) {
        if ((c == '\n') && (line[pos - 1] == '\r')) {
            line[pos - 1] = '\0';
            break;
        }
        line[pos++] = c;

        if (pos > line_len) {
            line_len *= 2;
            line = realloc(line, line_len);
        }
    }

    return line;
}

static void build_success_response(int connection)
{
    char buf[255];

    sprintf(buf,
            "HTTP/1.1 200 Success\r\nConnection: Close\r\nContent-Type:text/html\r\n\r\n<html><head><title>Test Page</title></head><body>Nothing here</body></html>\r\n");

    // Technically, this should account for short writes.
    if (send(connection, buf, strlen(buf), 0) < strlen(buf)) {
        perror("Trying to respond");
    }
}

static void build_error_response(int connection, int error_code)
{

    char buf[255];
    sprintf(buf, "HTTP/1.1 %d Error Occurred\r\n\r\n", error_code);

    // Technically, this should account for short writes.

    if (send(connection, buf, strlen(buf), 0) < strlen(buf)) {
        perror("Trying to respond");
    }
}

static void process_http_request(int connection)
{
    char *request_line;
    request_line = read_line(connection);
    if (strncmp(request_line, "GET", 3)) {
        //Only supprots "GET" here
        build_error_response(connection, 501);
    } else {
        while (strcmp(read_line(connection), " "))
            ;

        build_success_response(connection);
    }
#ifdef WIN32
    if (closesocket(connection) == -1)
#else
    if (close(connection) == -1)
#endif
    {
        perror("Unable to close connection");
    }
}

int main(int argc, char *argv[])
{
    int                listen_sock;
    int                connect_sock;
    int                on = 1;
    struct sockaddr_in local_addr;
    struct sockaddr_in client_addr;
    int                client_addr_len = sizeof(client_addr);
#ifdef WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != NO_ERROR) {
        perror("Unable to initialize winsock");
        exit(EXIT_FAILURE);
    }
#endif
    if ((listen_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Unable to create listening socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(listen_sock,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &on, sizeof(on)) == -1) {
        perror("Setting socket option error");
        exit(EXIT_FAILURE);
    }

    local_addr.sin_family      = AF_INET;
    local_addr.sin_port        = htons(HTTP_PORT);
    local_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    //local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_sock,
             (struct sockaddr *)&local_addr,
             sizeof(local_addr)) == -1) {
        perror("Unable to bind to local address");
        exit(EXIT_FAILURE);
    }

    if (listen(listen_sock, 5) == -1) {
        perror("Unable to set socket backlog");
        exit(EXIT_FAILURE);
    }

    while ((connect_sock = accept(listen_sock,
                                  (struct sockaddr *)&client_addr,
                                  (socklen_t *)&client_addr_len)) != -1) {
        // TODO: ideally, this would spawn a new thread.
        process_http_request(connect_sock);
    }

    if (connect_sock == -1) {
        perror("Unable to accept socket");
        exit(EXIT_FAILURE);
    }

    return 0;
}
