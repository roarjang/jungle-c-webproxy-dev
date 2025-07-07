# 컴파일러 및 플래그 설정
CC = gcc
CFLAGS = -Wall -Wextra -g

# 공통 소스
RIO_SRC = rio_function.c
RIO_OBJ = rio_function.o

# 실행 대상
TARGETS = test_rio echo_server tiny_server

# 기본 all 타겟
all: $(TARGETS)

# test_rio 빌드
test_rio: test_rio.o $(RIO_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

echo_server: echo_server.o $(RIO_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

tiny_server: tiny_server.o $(RIO_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# .c -> .o 자동 규칙
%.o: %.c
	$(CC) $(CFLAGS) -c $<

# 정리
clean:
	rm -f *.o $(TARGETS)