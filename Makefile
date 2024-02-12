FLAGS := -Wall -Werror -Wextra -Wno-unused-parameter
CC := gcc

ifeq ($(MODE), release)
	FLAGS += -O3 -flto
else
	FLAGS+= -O0 -DDEBUG -g
	MODE := debug
endif

a.out: scanner.o table.o object.o memory.o vm.o debug.o main.o chunk.o compiler.o value.o 
	@ echo "build in $(MODE) mode"
	$(CC) $(FLAGS) scanner.o table.o object.o memory.o vm.o debug.o main.o chunk.o compiler.o value.o -o a.out

scanner.o: scanner.c common.h scanner.h 
	$(CC) $(FLAGS) -c scanner.c -o scanner.o

table.o: table.c table.h memory.h chunk.h common.h object.h value.h 
	$(CC) $(FLAGS) -c table.c -o table.o

object.o: object.c common.h memory.h object.h chunk.h table.h vm.h value.h 
	$(CC) $(FLAGS) -c object.c -o object.o

memory.o: memory.c table.h common.h chunk.h memory.h object.h value.h vm.h 
	$(CC) $(FLAGS) -c memory.c -o memory.o

vm.o: vm.c table.h debug.h value.h object.h common.h vm.h chunk.h compiler.h memory.h 
	$(CC) $(FLAGS) -c vm.c -o vm.o

debug.o: debug.c debug.h chunk.h common.h value.h 
	$(CC) $(FLAGS) -c debug.c -o debug.o

main.o: main.c compiler.h table.h common.h vm.h chunk.h object.h value.h debug.h 
	$(CC) $(FLAGS) -c main.c -o main.o

chunk.o: chunk.c value.h memory.h object.h chunk.h common.h 
	$(CC) $(FLAGS) -c chunk.c -o chunk.o

compiler.o: compiler.c vm.h compiler.h debug.h value.h object.h chunk.h scanner.h common.h table.h 
	$(CC) $(FLAGS) -c compiler.c -o compiler.o

value.o: value.c memory.h chunk.h value.h object.h common.h 
	$(CC) $(FLAGS) -c value.c -o value.o

run: a.out
	./a.out

clean:
	rm -f *.o *.out
