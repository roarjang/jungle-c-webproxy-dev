#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    int x = 0, y = 0;
    char *method = getenv("REQUEST_METHOD");

    // 반드시 헤더 먼저 출력
    printf("Content-Type: text/html\r\n\r\n");

    if (method && strcmp(method, "GET") == 0) {
        char *qs = getenv("QUERY_STRING");
        if (qs) {
            sscanf(qs, "x=%d&y=%d", &x, &y);
        }
    } else if (method && strcmp(method, "POST") == 0) {
        char *len_str = getenv("CONTENT_LENGTH");
        int len = len_str ? atoi(len_str) : 0;
        if (len > 0) {
            char buf[1024];
            fread(buf, 1, len, stdin);
            sscanf(buf, "x=%d&y=%d", &x, &y);
        }
    }

    printf("<html><body>\n");
    printf("<h1>%d + %d = %d</h1>\n", x, y, x + y);
    printf("</body></html>\n");

    return 0;
}