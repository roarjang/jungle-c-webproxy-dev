#include <sys/socket.h>     // socket, bind, listen, accept
#include <netinet/in.h>     // sockaddr_in, htons
#include "rio.h"

#define PORT 8000
#define MAXLINE 1024
#define BACKLOG 10  // 최대 동시 접속 대기 수

// 클라이언트 요청을 한 줄씩 받아서 접두사와 함께 되돌려 주는 함수
void handle_client(int conn_fd) {
    rio_t rp;
    char buf[MAXLINE];
    char response[MAXLINE + 10];    // 접두사 포함 여유 버퍼

    rio_init(&rp, conn_fd);

    while (rio_readlineb(&rp, buf, MAXLINE) > 0) {
        snprintf(response, sizeof(response), "-> %s", buf);
        rio_writen(conn_fd, response, strlen(response));
    }

    printf("[INFO] Client disconnected.\n");
    close(conn_fd);
}

int main() {
    int listen_fd, conn_fd;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t addrlen = sizeof(cliaddr);

    // 소켓 생성
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket error");
        exit(1);
    }

    // 서버 주소 설정
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    // 바인딩
    if (bind(listen_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind error");
        exit(1);
    }

    // 클라이언트 접속 대기
    if (listen(listen_fd, BACKLOG) < 0) {
        perror("listen error");
        exit(1);
    }

    printf(" Echo Server is running on port %d...\n", PORT);

    // 클라이언트 연결 처리 루프
    while (1) {
        conn_fd = accept(listen_fd, (struct sockaddr *)&cliaddr, &addrlen);
        if (conn_fd < 0) {
            perror("accept error");
            continue;
        }

        printf("[INFO] Client connected.\n");
        handle_client(conn_fd);
    }

    close(listen_fd);
    return 0;
}