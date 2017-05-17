CFLAGS = -g -Wall -O2
LDFLAGS =

OBJECTS=../wmediumd/wserver_messages.o ../wmediumd/wserver_messages_network.o

all: client_snr client_errprob

client_snr: client_snr.o $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

client_errprob: client_errprob.o $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f client_snr.o client_errprob.o client_snr client_errprob
