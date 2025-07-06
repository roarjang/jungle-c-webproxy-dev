#include <fcntl.h>      // open
#include "rio.h"        // robust I/O 함수들

int main() {
    rio_t rio;
    char buf[1024];
    
    int fd = open("test_input.txt", O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    rio_init(&rio, fd);

    while (rio_readlineb(&rio, buf, sizeof(buf)) > 0) {
        printf("읽은 줄: %s", buf);  // \n 포함되므로 printf 줄바꿈 생략
    }

    close(fd);
    return 0;
}