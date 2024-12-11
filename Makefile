all: pa5

clean:
	rm -rf *.log *.o pa5

ipc.o: ipc.c context.h ipc.h pipes.h
	clang -c -std=c99 -pedantic -Werror -Wall ipc.c -o ipc.o

pa5.o: pa5.c common.h context.h ipc.h pa2345.h pipes.h
	clang -c -std=c99 -pedantic -Werror -Wall pa5.c -o pa5.o

pa5: ipc.o pa5.o pipes.o
	clang ipc.o pa5.o pipes.o -o pa5 -Llib64 -lruntime

pipes.o: pipes.c ipc.h pipes.h
	clang -c -std=c99 -pedantic -Werror -Wall pipes.c -o pipes.o
