# 컴파일러 및 플래그 설정
CC = gcc
CFLAGS = -Wall -Wextra -g

# 타겟 실행 파일
TARGET = test_rio

# 오브젝트 파일 목록
OBJS = test_rio.o rio_function.o

# 기본 빌드 명령어
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

# .c -> .o 자동 규칙
%.o: %.c
	$(CC) $(CFLAGS) -c $<

# 정리
clean:
	rm -f *.o $(TARGET)