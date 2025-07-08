#include <sys/mman.h>       // mmap() PROT_READ, MAP_PRIVATE
#include <sys/wait.h>
#include <netinet/in.h>     // sockaddr_in, htons, htonl 등 네트워크 관련 구조체와 함수
#include <fcntl.h>          // open() 함수 플래그 정의 (O_RDONLY 등)
#include <sys/stat.h>       // stat(), fstat(), struct stat
#include "rio.h"            // Robust I/O 함수 정의


#define PORT 8000
#define MAXLINE 1024
#define MAXBUF 8192
#define BACKLOG 10          // listen()에서 연결 요청을 대기시킬 최대 큐 길이

// ======================= 유틸리티 함수 ==========================

// 클라이언트가 보낸 HTTP 요청의 헤더 부분을 모두 읽어 들이는 함수
void read_reqeusthdrs(rio_t *rp, request_header_info *hdr_info) {
    char buf[MAXLINE];

    while (rio_readlineb(rp, buf, MAXLINE) > 0) {
        if (strcmp(buf, "\r\n") == 0)   // 헤더 끝은 빈 줄("\r\n")
            break;

        if (strncasecmp(buf, "Content-Length:", 15) == 0) {
            printf("[DEBUG] Found Content-Length line: %s", buf);
            sscanf(buf + 15, " %d", &hdr_info->content_length);
        }
        printf("%s", buf);  // 디버깅용 출력
    }
}

void read_post_body(rio_t *rp, int write_fd, int content_length) {
    char buf[MAXLINE];
    int n, left = content_length;

    while (left > 0 && (n = rio_readlineb(rp, buf, left < MAXLINE ? left : MAXLINE)) > 0) {
        write(write_fd, buf, n);
        left -= n;
    }
}

void set_cgi_env(const char *method, const char *cgiargs, int content_length) {
    char buf[16];

    setenv("REQUEST_METHOD", method, 1);
    if (strcasecmp(method, "GET") == 0) {
        setenv("QUERY_STRING", cgiargs, 1);
    } else if (strcasecmp(method, "POST") == 0) {
        sprintf(buf, "%d", content_length);
        setenv("CONTENT_LENGTH", buf, 1);
    }
}

// 에러 발생 시, HTTP 응답 형식으로 클라이언트에 에러 메시지 전송
void client_error(int fd, char *cause, char *errnum,
                  char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];

    // 1. HTTP 응답 본문 구성
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body + strlen(body), "<body bgcolor=\"ffffff\">\r\n");
    sprintf(body + strlen(body), "<h1>%s: %s</h1>\r\n", errnum, shortmsg);
    sprintf(body + strlen(body), "<p>%s: %s</p>\r\n", longmsg, cause);
    sprintf(body + strlen(body), "<hr><em>The Tiny Web Server</em>\r\n");
    sprintf(body + strlen(body), "</body></html>\r\n");

    // 2. HTTP 응답 헤더 구성
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %lu\r\n\r\n", strlen(body));
    rio_writen(fd, buf, strlen(buf));

    // 3. 본문 전송
    rio_writen(fd, body, strlen(body));
}

// 확장자를 기반으로 Content-Type 문자열 결정
void get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".css"))
        strcpy(filetype, "text/css");
    else if (strstr(filename, ".js"))
        strcpy(filetype, "application/javascript");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else
        strcpy(filetype, "text/plain");  // 기본값
}

// 정적 파일 요청 처리 (파일을 열고 내용 전송)
void serve_static(int connfd, char *filename, int filesize) {
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    // MIME 타입 추출 (예: html, png, gif 등)
    get_filetype(filename, filetype);

    // 응답 헤더 작성
    snprintf(buf, sizeof(buf),
            "HTTP/1.0 200 OK\r\n"
            "Server: Tiny Web Server\r\n"
            "Content-length: %d\r\n"
            "Content-type: %s\r\n\r\n",
            filesize, filetype);

    // 헤더 전송
    write(connfd, buf, strlen(buf));

    // 정적 파일 읽기 (메모리 매핑)
    srcfd = open(filename, O_RDONLY, 0);
    srcp = mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    close(srcfd);

    // 파일 내용 전송
    write(connfd, srcp, filesize);

    // 메모리 해제
    munmap(srcp, filesize);
}

// 동적 파일 요청을 처리하는 함수 (CGI 실행)
void serve_dynamic(int connfd, char *filename, char *cgiargs, char *method, request_header_info *hdr_info, rio_t *rp) {
    char buf[MAXLINE];
    char *emptylist[] = { NULL };
    extern char **environ;

    // 응답 헤더
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    write(connfd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    write(connfd, buf, strlen(buf));

    int pid, pipefd[2];

    // POST일 경우 body 데이터를 자식 stdin에 전달하기 위해 파이프 생성
    if (strcasecmp(method, "POST") == 0) {
        if (pipe(pipefd) < 0) {
            perror("pipe failed");
            return;
        }
    }

    if ((pid = fork()) == 0) {
        // 자식 프로세스
        if (strcasecmp(method, "POST") == 0) {
            close(pipefd[1]);  // write end 닫음
            dup2(pipefd[0], STDIN_FILENO); // 읽기쪽을 stdin으로 연결
        }

        dup2(connfd, STDOUT_FILENO); // stdout -> client
        set_cgi_env(method, cgiargs, hdr_info->content_length);

        execve(filename, emptylist, environ);
        perror("execve failed");
        exit(1);
    } else {
        // 부모 프로세스
        if (strcasecmp(method, "POST") == 0) {
            close(pipefd[0]); // 읽기쪽 닫고
            read_post_body(rp, pipefd[1], hdr_info->content_length);
            close(pipefd[1]);  // 다 썼으면 write end 닫음
        }
        waitpid(pid, NULL, 0);  // 자식 종료 기다림
    }
}

// URI -> 실제 파일 경로로 변환
void parse_uri(char *uri, char *filename, char *cgiargs) {
    char *ptr;

    if (!strstr(uri, "/cgi-bin")) {
        // 정적 콘텐츠 요청
        strcpy(cgiargs, "");    // 동적 인자 없음
        snprintf(filename, MAXLINE, "./static%s", uri); // 정적 디렉토리 기준 경로 설정

        // 디렉토리 요청이면 index.html 추가
        if (uri[strlen(uri) - 1] == '/' || strcmp(uri, "/") == 0) {
            strncat(filename, "index.html", MAXLINE - strlen(filename) - 1);
        }
    } else {
        // 동적 콘텐츠 요청
        ptr = strchr(uri, '?'); // '?' 기준으로 인자 분리
        if (ptr) {
            *ptr = '\0';    // '?' -> 문자열 종료 문자로 바꾸어 URI 자름
            strcpy(cgiargs, ptr + 1);   // 그 이후부터는 cgiargs로 저장
        } else {
            strcpy(cgiargs, "");        // 쿼리 인자 없음
        }
        snprintf(filename, MAXLINE, ".%s", uri); // 실행할 CGI 파일 경로
    }
}

// 클라이언트 하나의 요청을 처리하는 핵심 함수
void doit(int connfd) {
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    struct stat sbuf;
    rio_t rio;
    request_header_info hdr_info;

    rio_init(&rio, connfd);

    // 1. 요청 줄을 Robust I/O로 읽기
    rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);

    // 2. GET 외의 메서드 요청은 에러 처리
    if (strcasecmp(method, "GET") != 0 && strcasecmp(method, "POST") != 0) {
        client_error(connfd, method, "501", "Not Implemented", "Tiny only supports GET and POST");
        return;
    }

    // 3. 요청 헤더 읽기 (필요 정보는 hdr_info에 저장)
    read_reqeusthdrs(&rio, &hdr_info);

    // 4. URI 파싱 -> filename, cgiargs 분리
    parse_uri(uri, filename, cgiargs);

    // 5. 파일 존재 여부 확인
    if (stat(filename, &sbuf) < 0) {
        client_error(connfd, filename, "404", "Not Found", "File not found");
        return;
    }

    // 6. 정적 vs 동적 요청 분기
    if (!strstr(uri, "/cgi-bin")) {
        // 정적 콘텐츠: 정규 파일이고 읽기 권한 있어야 함
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            client_error(connfd, filename, "403", "Forbidden", "No permission to read file");
            return;
        }
        serve_static(connfd, filename, sbuf.st_size);
    } else {
        // 동적 콘텐츠: 정규 파일이고 실행 권한 있어야 함
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
            client_error(connfd, filename, "403", "Forbidden", "No permission to run CGI program");
            return;
        }
        serve_dynamic(connfd, filename, cgiargs, method, &hdr_info, &rio);
    }

    // 7. 응답 후 소켓 닫기
    close(connfd);
}

// ======================= 서버 메인 루프 ==========================

int main() {
    int listenfd, connfd;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t addrlen = sizeof(cliaddr);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);     // TCP 소켓 생성

    memset(&servaddr, 0, sizeof(servaddr));         // 주소 초기화
    servaddr.sin_family = AF_INET;                  // Ipv4 사용
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);   // 모든 IP에서 수신 허용
    servaddr.sin_port = htons(PORT);                // 포트 번호 지정

    bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)); // 소켓-주소 바인딩
    listen(listenfd, BACKLOG);                                      // 연결 대기 상태 진입

    printf("Tiny Server listening on port %d...\n", PORT);


    while (1) {
        connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &addrlen);   // 클라이언트 연결 수락
        // 논 블록킹 모드
        // int flags = fcntl(connfd, F_GETFL, 0);        // 기존 플래그 조회
        // fcntl(connfd, F_SETFL, flags | O_NONBLOCK);  // O_NONBLOCK 플래그 추가
        doit(connfd);
    }

    return 0;
}