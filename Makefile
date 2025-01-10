CC = gcc
CFLAGS = -Wall

SERVER_OBJ = queue.o cJSON.o log.o packet.o exchange.o hash_table.o trie.o user.o server.o
LIBRARY_OBJ = cJSON.o log.o packet.o message_broker.o

ifeq ($(shell pkg-config --exists uuid && echo yes),yes)
    CFLAGS += $(shell pkg-config --cflags uuid)
    LDLIBS += $(shell pkg-config --libs uuid)
    USE_UUID = 1
else
    USE_UUID = 0
endif

all: libmessagebroker.a server clean_obj

libmessagebroker.a: $(LIBRARY_OBJ)
	ar rcs libmessagebroker.a $(LIBRARY_OBJ)

server: server.c $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o server $(SERVER_OBJ) $(LDLIBS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

log.o: log.c log.h
	$(CC) $(CFLAGS) -DLOG_USE_COLOR -c log.c -o log.o

clean_obj:
	rm -f *.o

clean:
	rm -f server libmessagebroker.a *.o

clean_logs:
	rm -f *.log

.PHONY: all clean clean_obj
