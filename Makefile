a.out: scanner.o table.o object.o memory.o vm.o debug.o main.o chunk.o compiler.o value.o 
	gcc scanner.o table.o object.o memory.o vm.o debug.o main.o chunk.o compiler.o value.o -o a.out

scanner.o: scanner.c common.h scanner.h 
	gcc -c scanner.c -o scanner.o

table.o: table.c table.h memory.h common.h object.h value.h 
	gcc -c table.c -o table.o

object.o: object.c object.h table.h common.h chunk.h memory.h vm.h value.h 
	gcc -c object.c -o object.o

memory.o: memory.c vm.h object.h table.h common.h memory.h chunk.h value.h 
	gcc -c memory.c -o memory.o

vm.o: vm.c value.h chunk.h compiler.h memory.h vm.h table.h object.h debug.h common.h 
	gcc -c vm.c -o vm.o

debug.o: debug.c chunk.h common.h value.h debug.h 
	gcc -c debug.c -o debug.o

main.o: main.c table.h debug.h common.h value.h compiler.h chunk.h vm.h object.h 
	gcc -c main.c -o main.o

chunk.o: chunk.c memory.h common.h object.h value.h chunk.h 
	gcc -c chunk.c -o chunk.o

compiler.o: compiler.c compiler.h object.h value.h common.h vm.h table.h debug.h scanner.h chunk.h 
	gcc -c compiler.c -o compiler.o

value.o: value.c memory.h common.h object.h value.h 
	gcc -c value.c -o value.o

run: a.out
	./a.out

clean:
	rm -f *.o *.out
