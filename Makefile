run: main.out
	./main.out
main.out: main.o chunk.o memory.o debug.o value.o vm.o
	gcc -o main.out main.o chunk.o memory.o debug.o value.o vm.o

main.o: main.c
	gcc -c main.c
chunk.o: chunk.c
	gcc -c chunk.c
memory.o: memory.c
	gcc -c memory.c
	gcc -c memory.c
debug.o: debug.c
	gcc -c debug.c
value.o: value.c
	gcc -c value.c
vm.o: vm.c
	gcc -c vm.c

clean:
	rm -f *.o *.out