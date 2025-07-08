# 컴파일러 및 플래그 설정
CC = gcc
CFLAGS = -Wall -Wextra -g -O0

CGIDIR = cgi-bin

# 타겟 지정
all: echo_server tiny_server $(CGIDIR)/adder # proxy_server

# 공통 robust I/O 객체파일
rio_function.o: rio_function.c rio.h
	$(CC) $(CFLAGS) -c -o $@ $<

# 각 서버 빌드
test_rio: test_rio.c rio_function.o
	$(CC) $(CFLAGS) -o $@ $^

echo_server: echo_server.c rio_function.o
	$(CC) $(CFLAGS) -o $@ $^

tiny_server: tiny_server.c rio_function.o
	$(CC) $(CFLAGS) -o $@ $^

# proxy_server: proxy_server.c rio_function.o
# 	$(CC) $(CFLAGS) -o $@ $^

# CGI 프로그램
$(CGIDIR)/adder: $(CGIDIR)/adder.c
	$(CC) $(CFLAGS) -o $@ $<
	chmod +x $@

# 정리
clean:
	rm -f *.o echo_server tiny_server $(CGIDIR)/adder