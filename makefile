CC=gcc
ARCH := $(shell uname -m)

install: semaphore
	install -m 755 semaphore /usr/bin

clean:
	rm -f semaphore

ifeq ($(ARCH),armv5tel)
	OPT=-Dnosemopen
else
	OPT=
endif

semaphore:	semaphore.c
	$(CC) $(OPT) -o semaphore -lpthread -lrt semaphore.c
# End
