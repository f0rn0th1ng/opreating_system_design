#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define VERSION 23
#define BUFSIZE 8096
#define ERROR   42
#define LOG     44
#define FORBIDDEN 403
#define NOTFOUND    404

#ifndef SIGCLD
#   define SIGCLD SIGCHLD
#endif

struct {
    char *ext;
    char *filetype;
} extensions [] = {
    {"gif", "image/gif" },
    {"jpg", "image/jpg" },
    {"jpeg","image/jpeg"},
    {"png", "image/png" },
    {"ico", "image/ico" },
    {"gz", "image/gz" },
    {"tar", "image/tar" },
    {"htm", "text/html" },
    {"html","text/html" },
    {0,0}
};

void logger(int type, char *s1, char *s2, int socket_fd) {
    int fd;
    char logbuffer[BUFSIZE*2];
    time_t now;
    struct tm *tm_info;
    time(&now);
    tm_info = localtime(&now);

    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", tm_info);

    switch (type) {
        case ERROR:
            sprintf(logbuffer,"ERROR: %s:%s Errno=%d exiting pid=%d", s1, s2, errno, getpid());
            break;
        case FORBIDDEN:
            write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n", 271);
            sprintf(logbuffer,"FORBIDDEN: %s:%s", s1, s2);
            break;
        case NOTFOUND:
            write(socket_fd, "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n", 224);
            sprintf(logbuffer,"NOT FOUND: %s:%s", s1, s2);
            break;
        case LOG:
            sprintf(logbuffer, "%s INFO: %s:%s:%d", timestamp, s1, s2, socket_fd);
            break;
    }

    if ((fd = open("webserver.log", O_CREAT | O_WRONLY | O_APPEND, 0644)) >= 0) {
        write(fd, logbuffer, strlen(logbuffer));
        write(fd, "\n", 1);
        close(fd);
    }
}

void web(int fd, int hit) {
    int j, file_fd, buflen;
    long i, ret, len;
    char * fstr;
    static char buffer[BUFSIZE+1];

    ret = read(fd, buffer, BUFSIZE);
    if (ret == 0 || ret == -1) {
        logger(FORBIDDEN, "failed to read browser request", "", fd);
    }

    if (ret > 0 && ret < BUFSIZE) {
        buffer[ret] = 0;
    } else {
        buffer[0] = 0;
    }

    for (i = 0; i < ret; i++) {
        if (buffer[i] == '\r' || buffer[i] == '\n') {
            buffer[i] = '*';
        }
    }

    logger(LOG, "request", buffer, hit);

    if (strncmp(buffer, "GET ", 4) && strncmp(buffer, "get ", 4)) {
        logger(FORBIDDEN, "Only simple GET operation supported", buffer, fd);
    }

    for (i = 4; i < BUFSIZE; i++) {
        if (buffer[i] == ' ') {
            buffer[i] = 0;
            break;
        }
    }

    for (j = 0; j < i-1; j++) {
        if (buffer[j] == '.' && buffer[j+1] == '.') {
            logger(FORBIDDEN, "Parent directory (..) path names not supported", buffer, fd);
        }
    }

    if (!strncmp(&buffer[0], "GET /\0", 6) || !strncmp(&buffer[0], "get /\0", 6)) {
        strcpy(buffer, "GET /index.html");
    }

    buflen = strlen(buffer);
    fstr = (char *)0;

    for (i = 0; extensions[i].ext != 0; i++) {
        len = strlen(extensions[i].ext);
        if (!strncmp(&buffer[buflen-len], extensions[i].ext, len)) {
            fstr = extensions[i].filetype;
            break;
        }
    }

    if (fstr == 0) {
        logger(FORBIDDEN, "file extension type not supported", buffer, fd);
    }

    if ((file_fd = open(&buffer[5], O_RDONLY)) == -1) {
        logger(NOTFOUND, "failed to open file", &buffer[5], fd);
    }

    logger(LOG, "SEND", &buffer[5], hit);

    len = (long)lseek(file_fd, (off_t)0, SEEK_END);
    lseek(file_fd, (off_t)0, SEEK_SET);
    sprintf(buffer, "HTTP/1.1 200 OK\nServer: nweb/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, len, fstr);
    logger(LOG, "Header", buffer, hit);

    write(fd, buffer, strlen(buffer));

    while ((ret = read(file_fd, buffer, BUFSIZE)) > 0) {
        write(fd, buffer, ret);
    }

    sleep(1);
    close(fd);
}

int main(int argc, char **argv) {
    int i, port, listenfd, socketfd, hit;
    socklen_t length;
    static struct sockaddr_in cli_addr;
    static struct sockaddr_in serv_addr;

    if (argc < 3 || argc > 3 || !strcmp(argv[1], "-?")) {
        printf("hint: nweb Port-Number Top-Directory\t\tversion %d\n\n"
               "\tnweb is a small and very safe mini web server\n"
               "\tnweb only servers out file/web pages with extensions named below\n"
               "\tand only from the named directory or its sub-directories.\n"
               "\tThere is no fancy features = safe and secure.\n\n"
               "\tExample:webserver 8181 /home/nwebdir &\n\n"
               "\tOnly Supports:", VERSION);

        for (i = 0; extensions[i].ext != 0; i++) {
            printf(" %s", extensions[i].ext);
        }

        printf("\n\tNot Supported: URLs including \"..\", Java, Javascript, CGI\n"
               "\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin\n"
               "\tNo warranty given or implied\n\tNigel Griffiths nag@uk.ibm.com\n");
        exit(0);
    }

    if (!strncmp(argv[2], "/", 2) || !strncmp(argv[2], "/etc", 5) ||
        !strncmp(argv[2], "/bin", 5) || !strncmp(argv[2], "/lib", 5) ||
        !strncmp(argv[2], "/tmp", 5) || !strncmp(argv[2], "/usr", 5) ||
        !strncmp(argv[2], "/dev", 5) || !strncmp(argv[2], "/sbin", 6)) {
        printf("ERROR: Bad top directory %s, see nweb -?\n", argv[2]);
        exit(3);
    }

    if (chdir(argv[2]) == -1) {
        printf("ERROR: Can't Change to directory %s\n", argv[2]);
        exit(4);
    }

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        logger(ERROR, "system call", "socket", 0);
    }

    port = atoi(argv[1]);
    if (port < 0 || port > 60000) {
        logger(ERROR, "Invalid port number (try 1->60000)", argv[1], 0);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        logger(ERROR, "system call", "bind", 0);
    }

    if (listen(listenfd, 64) < 0) {
        logger(ERROR, "system call", "listen", 0);
    }

    for (hit = 1;; hit++) {
        length = sizeof(cli_addr);
        if ((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0) {
            logger(ERROR, "system call", "accept", 0);
        }

        web(socketfd, hit);
    }
}
