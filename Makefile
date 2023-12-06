a.out: scanner.o memory.o vm.o debug.o main.o chunk.o compiler.o value.o 
	gcc scanner.o memory.o vm.o debug.o main.o chunk.o compiler.o value.o -o a.out

scanner.o: scanner.c scanner.h common.h 
	gcc -c scanner.c -o scanner.o

memory.o: memory.c memory.h 
	gcc -c memory.c -o memory.o

vm.o: vm.c common.h debug.h vm.h compiler.h 
	gcc -c vm.c -o vm.o

debug.o: debug.c debug.h value.h 
	gcc -c debug.c -o debug.o

main.o: main.c compiler.h vm.h common.h debug.h 
	gcc -c main.c -o main.o

chunk.o: chunk.c chunk.h memory.h 
	gcc -c chunk.c -o chunk.o

compiler.o: compiler.c scanner.h common.h compiler.h debug.h 
	gcc -c compiler.c -o compiler.o

value.o: value.c memory.h value.h 
	gcc -c value.c -o value.o

run: a.out
	./a.out

clean:
	rm -f *.o *.out
