OBJECTS=Task.o EventConfig.o EventBase.o Event.o EvBuffer.o Listener.o EvDNSBase.o
HTTP_OBJECTS=Protocols/HTTP/Request.o Protocols/HTTP/Connection.o Protocols/HTTP/Server.o Protocols/HTTP/WebsocketServer.o
MYSQL_OBJECTS=Protocols/MySQL/Types.o Protocols/MySQL/ProtocolBuffer.o Protocols/MySQL/Client.o
CXX=g++ -fPIC
CXXFLAGS=-I/opt/homebrew/opt/openssl@1.1/include -I/opt/homebrew/include -I/usr/local/opt/openssl@1.1/include -I/usr/local/include -I/opt/local/include -std=c++20 -g -DHAVE_INTTYPES_H -DHAVE_NETINET_IN_H -Wall -Werror
LDFLAGS=-L/opt/homebrew/opt/openssl@1.1/lib -L/opt/homebrew/lib -L/usr/local/opt/openssl@1.1/lib -L/usr/local/lib -L/opt/local/lib -lphosg -levent -lssl -lcrypto -levent_openssl -g -std=c++20 -lstdc++

PACKAGE_LIBRARIES=libevent-async.a libhttp-async.a libmysql-async.a
PACKAGE_EXECUTABLES=ControlFlowTests EchoServerExample Protocols/HTTP/ServerExample Protocols/HTTP/ClientExample

ifeq ($(shell uname -s),Darwin)
	INSTALL_DIR=/opt/local
	CXXFLAGS += -I$(INSTALL_DIR)/include -DMACOSX -mmacosx-version-min=10.15
else
	INSTALL_DIR=/usr/local
	CXXFLAGS += -I$(INSTALL_DIR)/include -DLINUX
endif

all: $(PACKAGE_LIBRARIES) $(PACKAGE_EXECUTABLES)

ControlFlowTests: ControlFlowTests.o $(OBJECTS)
	g++ -o $@ $(LDFLAGS) $^

EchoServerExample: EchoServerExample.o $(OBJECTS)
	g++ -o $@ $(LDFLAGS) $^

Protocols/HTTP/ServerExample: Protocols/HTTP/ServerExample.o $(OBJECTS) $(HTTP_OBJECTS)
	g++ -o $@ $(LDFLAGS) $^

Protocols/HTTP/ClientExample: Protocols/HTTP/ClientExample.o $(OBJECTS) $(HTTP_OBJECTS)
	g++ -o $@ $(LDFLAGS) $^

install: $(PACKAGE_LIBRARIES)
	mkdir -p $(INSTALL_DIR)/include/event-async
	cp $(PACKAGE_LIBRARIES) $(INSTALL_DIR)/lib/
	cp -r *.hh $(INSTALL_DIR)/include/event-async/

libevent-async.a: $(OBJECTS)
	rm -f $@
	ar rcs $@ $(OBJECTS)

libhttp-async.a: $(HTTP_OBJECTS)
	rm -f $@
	ar rcs $@ $(HTTP_OBJECTS)

libmysql-async.a: $(MYSQL_OBJECTS)
	rm -f $@
	ar rcs $@ $(MYSQL_OBJECTS)

clean:
	find . -name "*.o" -delete
	rm -rf *.dSYM *.o gmon.out $(PACKAGE_LIBRARIES) $(PACKAGE_EXECUTABLES)

.PHONY: clean
