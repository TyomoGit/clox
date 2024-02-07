FLAGS= -Wall -Werror -Wextra -Wno-unused-parameter
CC= /usr/local/bin/gcc

debugmode: FLAGS+= -O0 -DDEBUG -g
debugmode: a.out

release: FLAGS+= -O3 -flto
release: a.out

a.out: scanner.o table.o object.o memory.o vm.o debug.o main.o chunk.o compiler.o value.o 
	$(CC) $(FLAGS) scanner.o table.o object.o memory.o vm.o debug.o main.o chunk.o compiler.o value.o -o a.out

scanner.o: scanner.c common.h scanner.h 
	$(CC) $(FLAGS) -c scanner.c -o scanner.o

table.o: table.c table.h memory.h common.h object.h value.h 
	$(CC) $(FLAGS) -c table.c -o table.o

object.o: object.c object.h table.h common.h chunk.h memory.h vm.h value.h 
	$(CC) $(FLAGS) -c object.c -o object.o

memory.o: memory.c vm.h object.h table.h common.h memory.h chunk.h value.h 
	$(CC) $(FLAGS) -c memory.c -o memory.o

vm.o: vm.c value.h chunk.h compiler.h memory.h vm.h table.h object.h debug.h common.h 
	$(CC) $(FLAGS) -c vm.c -o vm.o

debug.o: debug.c chunk.h common.h value.h debug.h 
	$(CC) $(FLAGS) -c debug.c -o debug.o

main.o: main.c table.h debug.h common.h value.h compiler.h chunk.h vm.h object.h 
	$(CC) $(FLAGS) -c main.c -o main.o

chunk.o: chunk.c memory.h common.h object.h value.h chunk.h 
	$(CC) $(FLAGS) -c chunk.c -o chunk.o

compiler.o: compiler.c compiler.h object.h value.h common.h vm.h table.h debug.h scanner.h chunk.h 
	$(CC) $(FLAGS) -c compiler.c -o compiler.o

value.o: value.c memory.h common.h object.h value.h 
	$(CC) $(FLAGS) -c value.c -o value.o

run: a.out
	./a.out

clean:
	rm -f *.o *.out
