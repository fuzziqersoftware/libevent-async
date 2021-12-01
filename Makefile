CORE_OBJECTS=Task.o Config.o Base.o Event.o Buffer.o DNSBase.o
HTTP_OBJECTS=Protocols/HTTP/Request.o Protocols/HTTP/Connection.o Protocols/HTTP/Server.o
MYSQL_OBJECTS=Protocols/MySQL/Types.o Protocols/MySQL/ProtocolBuffer.o Protocols/MySQL/Client.o Protocols/MySQL/BinlogProcessor.o
MEMCACHE_OBJECTS=Protocols/Memcache/Types.o Protocols/Memcache/Client.o
ALL_OBJECTS=$(CORE_OBJECTS) $(HTTP_OBJECTS) $(MEMCACHE_OBJECTS) $(MYSQL_OBJECTS)
CXX=g++ -fPIC
CXXFLAGS=-I/opt/homebrew/opt/openssl@1.1/include -I/opt/homebrew/include -I/usr/local/opt/openssl@1.1/include -I/usr/local/include -I/opt/local/include -std=c++20 -g -DHAVE_INTTYPES_H -DHAVE_NETINET_IN_H -Wall -Werror
LDFLAGS=-L/opt/homebrew/opt/openssl@1.1/lib -L/opt/homebrew/lib -L/usr/local/opt/openssl@1.1/lib -L/usr/local/lib -L/opt/local/lib -lphosg -levent -levent_pthreads -lssl -lcrypto -levent_openssl -g -std=c++20 -lstdc++

PACKAGE_LIBRARIES=\
	libevent-async.a \
	libhttp-async.a \
	libmysql-async.a \
	libmemcache-async.a
PACKAGE_EXECUTABLES=\
	Examples/ControlFlowTests \
	Examples/EchoServer \
	Examples/RangeReverseDNS \
	Examples/HTTPServer \
	Examples/HTTPClient \
	Examples/MemcacheFunctionalTest \
	Examples/MySQLBinlogReader \
	Examples/MySQLBinlogStats

ifeq ($(shell uname -s),Darwin)
	INSTALL_DIR=/opt/local
	CXXFLAGS += -I$(INSTALL_DIR)/include -DMACOSX -mmacosx-version-min=10.15
else
	INSTALL_DIR=/usr/local
	CXXFLAGS += -I$(INSTALL_DIR)/include -DLINUX
endif

ifeq ($(shell uname -m),arm64)
	CXXFLAGS += -arch arm64
	LDFLAGS += -arch arm64
endif

all: depend $(PACKAGE_LIBRARIES) $(PACKAGE_EXECUTABLES)

Examples/%: Examples/%.o $(ALL_OBJECTS)
	g++ -o $@ $(LDFLAGS) $^

depend: $(ALL_OBJECTS:.o=.d)

clean:
	find . -name "*.o" -delete
	rm -rf *.dSYM *.o gmon.out $(PACKAGE_LIBRARIES) $(PACKAGE_EXECUTABLES)

clean-depend:
	find . -name "*.d" -delete

%.d: %.cc
	$(CC) -MM $(CXXFLAGS) $< | sed 's,\($(*F)\)\.o[ :]*,$*.o $@ : ,g' > $@

-include $(ALL_OBJECTS:.o=.d)

install: $(PACKAGE_LIBRARIES)
	mkdir -p $(INSTALL_DIR)/include/event-async
	mkdir -p $(INSTALL_DIR)/include/event-async/Protocols/HTTP
	mkdir -p $(INSTALL_DIR)/include/event-async/Protocols/MySQL
	mkdir -p $(INSTALL_DIR)/include/event-async/Protocols/Memcache
	cp $(PACKAGE_LIBRARIES) $(INSTALL_DIR)/lib/
	cp *.hh $(INSTALL_DIR)/include/event-async/
	cp Protocols/HTTP/*.hh $(INSTALL_DIR)/include/event-async/Protocols/HTTP/
	cp Protocols/MySQL/*.hh $(INSTALL_DIR)/include/event-async/Protocols/MySQL/
	cp Protocols/Memcache/*.hh $(INSTALL_DIR)/include/event-async/Protocols/Memcache/

libevent-async.a: $(CORE_OBJECTS)
	rm -f $@
	ar rcs $@ $(CORE_OBJECTS)

libhttp-async.a: $(HTTP_OBJECTS)
	rm -f $@
	ar rcs $@ $(HTTP_OBJECTS)

libmysql-async.a: $(MYSQL_OBJECTS)
	rm -f $@
	ar rcs $@ $(MYSQL_OBJECTS)

libmemcache-async.a: $(MEMCACHE_OBJECTS)
	rm -f $@
	ar rcs $@ $(MEMCACHE_OBJECTS)

.PHONY: clean
