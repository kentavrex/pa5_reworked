all: pa5

env:
	export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./lib64

build:
	clang -std=c99 -Wall -pedantic *.c -Llib64 -lruntime -o pa_program
