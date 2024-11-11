CC = gcc
CFLAGS = -Wall

OBJ = socketutil.o queue.o cJSON.o log.o utils.o packet.o exchange.o hash_table.o trie.o

all: producer server clean_obj

producer: producer.c $(OBJ)
	$(CC) $(CFLAGS) -o producer producer.c $(OBJ)

server: server.c $(OBJ)
	$(CC) $(CFLAGS) -o server server.c $(OBJ)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

log.o: log.c log.h
	$(CC) $(CFLAGS) -DLOG_USE_COLOR -c log.c -o log.o

clean_obj:
	rm -f *.o

clean:
	rm -f producer server *.o

clean_logs:
	rm -f producer_log.txt server_log.txt

.PHONY: all clean clean_obj