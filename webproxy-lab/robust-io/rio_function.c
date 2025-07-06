#include "rio.h"

/* Robust Input/Output Function Implement */
void rio_init(rio_t *rp, int fd) {
    rp->fd = fd;
    rp->bytes_in_buf = 0;
    rp->read_ptr = rp->internal_buf;
}

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

ssize_t rio_writen(int fd, const void *usr_buf, size_t n) {
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

/*
 * rio_read - 내부 버퍼를 사용하여 시스템 콜 호출을 최소화하고,
 *            사용자 요청 바이트 수만큼 안정적으로 읽어오는 robust read 함수
 */
ssize_t rio_read(rio_t *rp, void *usr_buf, size_t n) {
    size_t bytes_left = n;      // 아직 사용자에게 전달할 바이트 수
    char *buf_p = usr_buf;      // 사용자 버퍼 포인터

    while (bytes_left > 0) {
        // 내부 버퍼가 비어 있으면 시스템 콜로 새로 채움
        if (rp->bytes_in_buf <= 0) {
            rp->bytes_in_buf = read(rp->fd, rp->internal_buf, RIO_BUF_SIZE);

            if (rp->bytes_in_buf < 0) {
                if (errno == EINTR)
                    continue;       // 시그널로 인한 중단 -> 재시도
                return -1;          // 그 외 read 에러
            } else if (rp->bytes_in_buf == 0) {
                break;              // EOF
            }

            rp->read_ptr = rp->internal_buf; // 버퍼 포인터 초기화
        }

        // 내부 버퍼에서 복사할 수 있는 바이트 수 계산
        size_t cnt = (bytes_left < (size_t)rp->bytes_in_buf) ? bytes_left : (size_t)rp->bytes_in_buf;

        // 내부 버퍼 -> 사용자 버퍼로 복사
        memcpy(usr_buf, rp->read_ptr, cnt);

        // 상태 갱신
        rp->read_ptr += cnt;
        rp->bytes_in_buf -= cnt;
        buf_p += cnt;
        bytes_left -= cnt;
    }

    // 총 읽은 바이트 수 반환
    return (n - bytes_left);
}

ssize_t rio_readlineb(rio_t *rp, void *usr_buf, size_t max_len) {
    // 최소한 '\0'을 담을 공간은 있어야 함
    if (max_len < 2) {
        if (max_len == 1)
            ((char *)usr_buf)[0] = '\0';
        return 0;
    }

    char *p = usr_buf;  // 사용자 버퍼 포인터
    char c;             // 한 글자씩 임시 저장할 변수
    ssize_t rc;         // rio_read의 반환값

    // 최대 max_len - 1까지만 반복 (마지막은 '\0'용 공간)
    while (p < (char *)usr_buf + max_len - 1) {
        rc = rio_read(rp, &c, 1);  // 내부 버퍼에서 1바이트 읽기

        if (rc == 1) {
            *p++ = c;              // 사용자 버퍼에 복사 후 포인터 증가
            if (c == '\n')         // 줄 끝이면 반복 종료
                break;
        } else if (rc == 0) {
            // EOF: 아무것도 못 읽은 경우
            if (p == (char *)usr_buf)
                return 0;
            break;  // 일부라도 읽었으면 종료
        } else {
            return -1;  // read 에러
        }
    }
    *p = '\0';  // 문자열 종료 처리
    
    return p - (char *)usr_buf;  // 총 읽은 바이트 수 반환 (\0 제외)
}