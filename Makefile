PROG = cutils
SRCS = $(wildcard *.c) hashers/spooky.c
OBJS = $(SRCS:.c=.o)

IGNORE = -Wno-implicit-fallthrough -Wno-pointer-to-int-cast \
         -Wno-format-nonliteral

DEBUGFLAGS = -Og -ggdb -DDEBUG \
             -fno-omit-frame-pointer \
             -fsanitize=address,leak,undefined

CFLAGS = -std=c18 -pedantic -Wall -Wextra -Werror $(IGNORE) $(DEBUGFLAGS)
INCLUDES = -I/usr/local/include -I/usr/include -I.

LDFLAGS = -L/usr/local/lib -L/usr/lib64 -L/usr/lib -L.
LDLIBS = -lpthread -lcurl -ljson-c -lwebsockets -lsqlite3

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@ -fPIC

lib$(PROG).so: $(OBJS)
	$(CC) $(CFLAGS) -o lib$(PROG).so $(OBJS) $(INCLUDES) $(LDFLAGS) -shared $(LDLIBS) 

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) -o $(PROG) $(OBJS) $(INCLUDES) $(LDFLAGS) $(LDLIBS)

.PHONY: clean
clean:
	rm -rf $(PROG) $(OBJS) *.o *.so *.core vgcore.*
