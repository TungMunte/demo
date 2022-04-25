CFLAGS = -Wall -g

PORT_SERVER =

ID_CLIENT = 

IP_SERVER = 

PORT = 

all: server subscriber

# Compileaza server.c
server: server.c

# Compileaza client.c
subscriber_start: subscriber.c

.PHONY: clean run_server run_client

# Ruleaza serverul
server_start: server
	./server ${PORT}

# Ruleaza clientul
subscriber_start: subscriber
	./subscriber ${ID_CLIENT} ${IP_SERVER} ${PORT}

clean:
	rm -f server subscriber
