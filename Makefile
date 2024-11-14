CC = gcc
CFLAGS = -Wall

OBJ = socketutil.o queue.o cJSON.o log.o utils.o packet.o exchange.o hash_table.o trie.o

ifeq ($(shell pkg-config --exists uuid && echo yes),yes)
    CFLAGS += $(shell pkg-config --cflags uuid)
    LDLIBS += $(shell pkg-config --libs uuid)
    USE_UUID = 1
else
    USE_UUID = 0
endif

all: producer server consumer clean_obj

producer: producer.c $(OBJ)
	$(CC) $(CFLAGS) -o producer producer.c $(OBJ) $(LDLIBS)

server: server.c $(OBJ)
	$(CC) $(CFLAGS) -o server server.c $(OBJ) $(LDLIBS)

consumer: consumer.c $(OBJ)
	$(CC) $(CFLAGS) -o consumer consumer.c $(OBJ) $(LDLIBS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

log.o: log.c log.h
	$(CC) $(CFLAGS) -DLOG_USE_COLOR -c log.c -o log.o

clean_obj:
	rm -f *.o

clean:
	rm -f producer server consumer *.o

clean_logs:
	rm -f producer_log.txt server_log.txt consumer_log.txt

.PHONY: all clean clean_obj
