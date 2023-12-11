CC = gcc
CFLAGS = -g

all: simple_test test_case stest

simple_test:
	$(CC) $(CFLAGS) -o simple_test simple_test.c

test_case:
	$(CC) $(CFLAGS) -o test_case test_cases.c

stest:
	$(CC) $(CFLAGS) -o stest stest.c

clean:
	rm -rf simple_test test_case stest
