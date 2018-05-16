OPTS = -std=c99 -Wall -Wextra
FLAGS = -c
OBJS = mem_manager.o scanner.o dll.o

lru: $(OBJS)
	gcc $(OPTS) $(OBJS) -o lru

fifo: $(OBJS) without_mods.o
	gcc $(OPTS) without_mods.o scanner.o -o fifo

test: lru
	./lru ../BACKING_STORE.bin ../addresses.txt > example_output.txt
	diff ../correct_lru.txt example_output.txt

mem_manager.o: mem_manager.c scanner.h dll.h
	gcc $(OPTS) $(FLAGS) mem_manager.c

without_mods.o: without_mods.c scanner.h
	gcc $(OPTS) $(FLAGS) without_mods.c

scanner.o: scanner.c scanner.h
	gcc $(OPTS) $(FLAGS) scanner.c

dll.o: dll.c dll.h
	gcc $(OPTS) $(FLAGS) dll.c

clean:
	rm -f $(OBJS) lru fifo without_mods.o example_output.txt
