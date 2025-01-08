CC = gcc
CFLAGS = -Wall

OBJ = socketutil.o queue.o cJSON.o log.o utils.o packet.o exchange.o hash_table.o trie.o user.o

ifeq ($(shell pkg-config --exists uuid && echo yes),yes)
    CFLAGS += $(shell pkg-config --cflags uuid)
    LDLIBS += $(shell pkg-config --libs uuid)
    USE_UUID = 1
else
    USE_UUID = 0
endif

all: client server clean_obj

client: client.c $(OBJ)
	$(CC) $(CFLAGS) -o client client.c $(OBJ) $(LDLIBS)

server: server.c $(OBJ)
	$(CC) $(CFLAGS) -o server server.c $(OBJ) $(LDLIBS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

log.o: log.c log.h
	$(CC) $(CFLAGS) -DLOG_USE_COLOR -c log.c -o log.o

clean_obj:
	rm -f *.o

clean:
	rm -f client server *.o

clean_logs:
	rm -f *.log

.PHONY: all clean clean_obj
