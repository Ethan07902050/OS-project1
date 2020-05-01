all: main.c process.c scheduling.c queue.c
	gcc -o main -pthread main.c process.c scheduling.c queue.c

clean:
	rm main