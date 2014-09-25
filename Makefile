CC	= gcc

CFLAGS	= -Wmissing-prototypes -Wstrict-prototypes -Wunused -Wall -ggdb -Wa,-W
CPPFLAGS= -I. -DHAVE_RT_NETLINK

LDFLAGS = -lfl -lpthread -lcrypt

OBJS	:= commands.o vty.o lex.yy.o tcp_socket.o route.o interface.o core.o

all: server

server: $(OBJS)
	$(CC) $(CFLAGS) -rdynamic -o $@ $(OBJS) $(LDFLAGS)



OBJS_TEST := init_modules.o modules_test.o 
init_modules_test:	$(OBJS_TEST)
		$(CC) $(CFLAGS) -rdynamic -o $@ $(OBJS_TEST) $(LDFLAGS)

clean:
	find . \( -name '*.o' \) -exec rm \{\} \;
	rm -f init_modules_test server
