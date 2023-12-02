run: main.out
	./main.out
main.out: main.o chunk.o memory.o debug.o value.o vm.o compiler.o scanner.o
	gcc -o main.out main.o chunk.o memory.o debug.o value.o vm.o compiler.o scanner.o

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
compiler.o: compiler.c
	gcc -c compiler.c
scanner.o: scanner.c
	gcc -c scanner.c

clean:
	rm -f *.o *.out