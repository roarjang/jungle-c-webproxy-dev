#ifndef RIO_H
#define RIO_H

#include <unistd.h>     // read, write, ssize_t
#include <sys/types.h>  // size_t, ssize_t
#include <errno.h>      // errno, EINTR
#include <string.h>     // memcpy, memset
#include <stdio.h>      // printf, fprintf, perror (optional)
#include <stdlib.h>

#define RIO_BUF_SIZE 8192

typedef struct {
    int fd;                             // 파일 디스크립터
    int bytes_in_buf;                   // 내부 버퍼에 남은 바이트 수
    char *read_ptr;                     // 읽을 위치 포인터
    char internal_buf[RIO_BUF_SIZE];    // 내부 버퍼         
} rio_t;

typedef struct {
    int content_length;
} request_header_info;

// 초기화 함수
void rio_init(rio_t *rp, int fd);

// 내부 버퍼 기반 robust read 함수 (한 번에 n바이트 이하)
ssize_t rio_read(rio_t *rp, void *usr_buf, size_t n);

// n바이트를 robust하게 읽는 함수 (short read 방지)
ssize_t rio_readn(int fd, void *usr_buf, size_t n);

// n바이트를 robust하게 쓰는 함수 (short write 방지)
ssize_t rio_writen(int fd, const void *usr_buf, size_t n);

// 한 줄 단위 robust read 함수
ssize_t rio_readlineb(rio_t *rp, void *usr_buf, size_t max_len);

#endif // RIO_H