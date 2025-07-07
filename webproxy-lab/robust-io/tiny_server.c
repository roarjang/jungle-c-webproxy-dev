#include <netinet/in.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "rio.h"

#define PORT 8000
#define MAXLINE 1024
#define BACKLOG 10

// ======================= 유틸리티 함수 ==========================

// 클라이언트에 HTTP 에러 메시지 전송
void send_error(int fd, int status, char *msg, char *longmsg) {
    char buf[MAXLINE], body[MAXLINE];

    // 응답 body 생성
    snprintf(body, sizeof(body),
            "<html><title>Error</title><body>\r\n"
            "%d: %s<br>\r\n%s\r\n</body></html>\r\n",
            status, msg, longmsg);

    // 응답 헤더 생성
    snprintf(buf, sizeof(buf),
            "HTTP/1.0 %d %s\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %lu\r\n\r\n",
            status, msg, strlen(body));

    write(fd, buf, strlen(buf));
    write(fd, body, strlen(body));
}

// 정적 파일 응답 (파일 열고 send)
void serve_static(int fd, char *filename) {
    int srcfd = open(filename, O_RDONLY, 0);
    if (srcfd < 0) {
        send_error(fd, 403, "Forbidden", "File cannot be opened");
        return;
    }

    struct stat sbuf;
    if (fstat(srcfd, &sbuf) < 0) {
        send_error(fd, 500, "Internal Server Error", "fstat failed");
        return;
    }

    // 간단한 Content-Type 결정
    char *type = strstr(filename, ".html") ? "text/html" : "text/plain";

    char header[MAXLINE];
    snprintf(header, sizeof(header),
            "HTTP/1.0 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %ld\r\n"
            "Connection: close\r\n\r\n",
            type, (long)sbuf.st_size);
        
    write(fd, header, strlen(header));

    // 파일 내용 전송
    char buf[MAXLINE];
    ssize_t n;
    while ((n = read(srcfd, buf, MAXLINE)) > 0) {
        write(fd, buf, n);
    }

    close(srcfd);
}

// URI -> 파일 경로 변환
void parse_uri(char *uri, char *filename) {
    // 기본 웹 루트 디렉터리
    char webroot[] = "./static";
    snprintf(filename, MAXLINE, "%s%s", webroot, uri);

    // URI가 "/"이면 index.html로 대체
    if (uri[strlen(uri) - 1] == '/') {
        strncat(filename, "index.html", MAXLINE - strlen(filename) - 1);
    }
}

// 요청 한 줄 읽고 정적 파일 제공
void handle_client(int connfd) {
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE];

    // 요청 줄 읽기
    read(connfd, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);

    // GET 외에는 에러 응답
    if (strcasecmp(method, "GET") != 0) {
        send_error(connfd, 501, "Not Implemented", "Tiny does not support this method");
        return;
    }

    parse_uri(uri, filename);
    serve_static(connfd, filename);
    close(connfd);
}

// ======================= 서버 메인 루프 ==========================

int main() {
    int listenfd, connfd;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t addrlen = sizeof(cliaddr);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    listen(listenfd, BACKLOG);

    printf("Tiny Server listening on port %d...\n", PORT);

    while (1) {
        connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &addrlen);
        handle_client(connfd);
    }

    return 0;
}