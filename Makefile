PJBASE=/home/dartalexx/voicemail/pjproject

include $(PJBASE)/build.mak

CC      = $(PJ_CC)
LDFLAGS = $(PJ_LDFLAGS)
LDLIBS  = $(PJ_LDLIBS)
CFLAGS  = $(PJ_CFLAGS)
CPPFLAGS= ${CFLAGS}

all: server

server: uas_working.c
	$(CC) -g -o $@ $< $(CPPFLAGS) $(LDFLAGS) $(LDLIBS)

test: uas_off.c
	$(CC) -g -o $@ $^ $(CPPFLAGS) $(LDFLAGS) -lgomp  $(LDLIBS)

client: client1.c
	$(CC) -g -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(LDLIBS)
clean:
	rm -f uas*.o server test client

rebuild: clean all
