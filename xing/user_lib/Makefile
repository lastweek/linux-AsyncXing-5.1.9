CFLAGS := -static

SRCS := $(wildcard *.c posix/*.c)
OBJS := $(SRCS:.c=.o)

all: $(OBJS)

clean:
	rm -f *.o

%.o: %.c
	gcc -g -O2 -o $@ $< -lm -pthread -fomit-frame-pointer
