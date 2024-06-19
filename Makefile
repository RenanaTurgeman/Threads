CC = gcc
CFLAGS = -std=c11 -pthread
TARGET = primeCounter

all: $(TARGET)

$(TARGET): primeCounter.o lfq.o
	$(CC) $(CFLAGS) -o $(TARGET) primeCounter.o lfq.o

primeCounter.o: primeCounter.c lfq.h
	$(CC) $(CFLAGS) -c primeCounter.c

lfq.o: lfq.c lfq.h
	$(CC) $(CFLAGS) -c lfq.c

clean:
	rm -f $(TARGET) *.o
