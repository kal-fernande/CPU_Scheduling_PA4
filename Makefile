CC=gcc
CFLAGS=-O2 -Wall -Wextra -pthread
LDFLAGS=-pthread

# Your files: provide your own main.c next to these files
SRC = cmdparser.c ready_queue.c sched_core.c scheduler_wiring.c csvloader.c main.c
BIN=sched

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f $(BIN) *.o
