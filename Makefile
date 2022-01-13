
CC=gcc

ARCH := $(shell uname -m)
ifeq ($(ARCH),armv5tel)
	OPT=-Dnosemopen
else
	OPT=
endif

semaphore:	semaphore.c
	$(CC) $(OPT) -Wall -o semaphore -lpthread -lrt semaphore.c

check:
	@./semtest.sh

test: check

install: semaphore
	install -m 755 semaphore /usr/bin

clean:
	rm -f semaphore
# End
