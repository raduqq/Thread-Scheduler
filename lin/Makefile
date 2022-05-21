# Copyright Radu-Stefan Minea 334CA [2022]

CC = gcc
CFLAGS = -fPIC -Wall -Wextra

.PHONY: build
build: libscheduler.so

libscheduler.so: so_scheduler.o
	$(CC) $(LDFLAGS) -shared -o $@ $^

so_scheduler.o: so_scheduler.c so_scheduler.h
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	-rm -f *.o libscheduler.so