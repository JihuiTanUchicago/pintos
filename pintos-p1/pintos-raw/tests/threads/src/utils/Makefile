CC = gcc
CFLAGS = -Wall -W
LDFLAGS = -lm

all: setitimer-helper squish-pty squish-unix

%.o: %.c
	$(CC) -c $< -o $@

%: %.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

clean:
	rm -f *.o setitimer-helper squish-pty squish-unix

