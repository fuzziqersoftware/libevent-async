OBJECTS=AsyncTask.o \
	EventConfig.o EventBase.o Event.o EvBuffer.o Listener.o EvDNSBase.o \
	HTTPRequest.o HTTPConnection.o HTTPServer.o HTTPWebsocketServer.o
CXX=g++ -fPIC
CXXFLAGS=-I/opt/homebrew/opt/openssl@1.1/include -I/opt/homebrew/include -I/usr/local/opt/openssl@1.1/include -I/usr/local/include -I/opt/local/include -std=c++20 -g -DHAVE_INTTYPES_H -DHAVE_NETINET_IN_H -Wall -Werror
LDFLAGS=-L/opt/homebrew/opt/openssl@1.1/lib -L/opt/homebrew/lib -L/usr/local/opt/openssl@1.1/lib -L/usr/local/lib -L/opt/local/lib -lphosg -levent -lssl -lcrypto -levent_openssl -g -std=c++20 -lstdc++

ifeq ($(shell uname -s),Darwin)
	INSTALL_DIR=/opt/local
	CXXFLAGS += -I$(INSTALL_DIR)/include -DMACOSX -mmacosx-version-min=10.15
else
	INSTALL_DIR=/usr/local
	CXXFLAGS += -I$(INSTALL_DIR)/include -DLINUX
endif

all: libevent-async.a ControlFlowTests EchoServerExample HTTPServerExample HTTPClientExample

ControlFlowTests: ControlFlowTests.o $(OBJECTS)
	g++ -o ControlFlowTests $(LDFLAGS) $^

EchoServerExample: EchoServerExample.o $(OBJECTS)
	g++ -o EchoServerExample $(LDFLAGS) $^

HTTPServerExample: HTTPServerExample.o $(OBJECTS)
	g++ -o HTTPServerExample $(LDFLAGS) $^

HTTPClientExample: HTTPClientExample.o $(OBJECTS)
	g++ -o HTTPClientExample $(LDFLAGS) $^

install: libevent-async.a
	mkdir -p $(INSTALL_DIR)/include/event-async
	cp libevent-async.a $(INSTALL_DIR)/lib/
	cp -r *.hh $(INSTALL_DIR)/include/event-async/

libevent-async.a: $(OBJECTS)
	rm -rf libevent-async.a
	ar rcs libevent-async.a $(OBJECTS)

clean:
	rm -rf *.dSYM *.o gmon.out libevent-async.a ControlFlowTests EchoServerExample HTTPServerExample HTTPClientExample

.PHONY: clean
