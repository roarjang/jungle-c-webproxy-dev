#include <unistd.h>             // read, write, ssize_t
#include <sys/types.h>          // size_t, ssize_t
#include <errno.h>              // errno, EINTR
#include <string.h>             // strerror()
#include <stdio.h>              // printf, perror 등

#define RIO_BUFSIZE 8192

typedef struct {
    int rio_fd;                 // 연결된 fd
    int rio_cnt;                // 내부 버퍼에 남은 바이트 수
    char *rio_buf_ptr;          // 내부 버퍼에서 현재 읽는 위치
    char rio_buf[RIO_BUFSIZE];  // 내부 버퍼
} rio_t;

/* Robust Input/Output Function Implement */
ssize_t rio_readn(int fd, void *usr_buf, size_t n) {
    size_t bytes_left = n;      // 아직 읽어야 할 바이트 수
    ssize_t bytes_read;         // 이번 read() 호출의 결과
    char *buf_ptr = usr_buf;    // 현재 쓰기 위치 포인터 (입력 버퍼)
    
    while (bytes_left > 0) {
        // read() 시스템 콜 호출
        bytes_read = read(fd, buf_ptr, bytes_left);

        // 에러 처리: 시그널로 중단된 경우 -> 다시 시도
        if (bytes_read < 0) {
            if (errno == EINTR)
                continue;       // 인터럽트 발생 시 -> 다시 루프
                
            return -1;          // 그 외 에러 -> 실패 반환
        }

        // EOF 도달: 읽을 게 더 없음
        if (bytes_read == 0)
            break;

        // 성공적으로 읽은 만큼 포인터 이동 및 바이트 수 갱신
        bytes_left -= bytes_read;
        buf_ptr += bytes_read;
    }

    // 실제로 읽은 총 바이트 수 반환
    return (n - bytes_left);
}

ssize_t rio_writen(int fd, void *usr_buf, size_t n) {
    size_t bytes_left = n;          // 아직 써야 할 바이트 수
    ssize_t bytes_written;          // 이번 write() 호출의 결과
    const char *buf_ptr = usr_buf;  // 현재 쓰기 위치 포인터

    while (bytes_left > 0) {
        // write() 시스템 콜 호출
        bytes_written = write(fd, buf_ptr, bytes_left);

        // 오류 처리: 인터럽트로 중단된 경우 -> 다시 시도
        if (bytes_written < 0) {
            if (errno == EINTR)
                continue;           // 시그널 중단 -> 루프 계속

            return -1;              // 그 외 에러 -> 실패 반환
        }

        // 드물지만, 0바이트 쓰인 경우 -> 루프 종료
        if (bytes_written == 0)
            break;

        // 성공적으로 쓴 만큼 포인터와 남은 길이 조정
        bytes_left -= bytes_written;
        buf_ptr += bytes_written;
    }
    // 실제로 쓴 총 바이트 수 반환
    return (n - bytes_left);
}

ssize_t rio_read(rio_t *rp, char *usr_buf, size_t n) {
    // 내부 버퍼가 비어 있으면 read()로 새로 채움
    while (rp->rio_cnt <= 0) {
        int rc = read(rp->rio_fd, rp->rio_buf_ptr, RIO_BUFSIZE);

        if (rc < 0) {
            if (errno == EINTR)
                continue;       // 시그널로 중단 -> 다시 시도
            else
                return -1;      // 진짜 에러
        } else if (rc == 0) {
            return 0;           // EOF
        }

        rp->rio_cnt = rc;               // 버퍼에 새로 채운 바이트 수
        rp->rio_buf_ptr = rp->rio_buf;   // 버퍼의 시작 위치로 초기화
    }

    // 내부 버퍼에서 사용자 버퍼로 최대 n바이트 복사
    size_t cnt = n;
    if (rp->rio_cnt < n)
        cnt = rp->rio_cnt;

    memcpy(usr_buf, rp->rio_buf_ptr, cnt);
    rp->rio_buf_ptr += cnt;
    rp->rio_cnt -= cnt;

    return cnt;
}