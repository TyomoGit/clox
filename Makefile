a.out: scanner.o object.o memory.o vm.o debug.o main.o chunk.o compiler.o value.o 
	gcc scanner.o object.o memory.o vm.o debug.o main.o chunk.o compiler.o value.o -o a.out

scanner.o: scanner.c common.h scanner.h 
	gcc -c scanner.c -o scanner.o

object.o: object.c memory.h object.h value.h vm.h 
	gcc -c object.c -o object.o

memory.o: memory.c memory.h vm.h 
	gcc -c memory.c -o memory.o

vm.o: vm.c debug.h object.h common.h memory.h vm.h compiler.h 
	gcc -c vm.c -o vm.o

debug.o: debug.c value.h debug.h 
	gcc -c debug.c -o debug.o

main.o: main.c vm.h compiler.h common.h debug.h 
	gcc -c main.c -o main.o

chunk.o: chunk.c chunk.h memory.h 
	gcc -c chunk.c -o chunk.o

compiler.o: compiler.c debug.h scanner.h common.h compiler.h 
	gcc -c compiler.c -o compiler.o

value.o: value.c object.h value.h memory.h 
	gcc -c value.c -o value.o

run: a.out
	./a.out

clean:
	rm -f *.o *.out
