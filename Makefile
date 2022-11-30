PJBASE=/home/dartalexx/voicemail/pjproject
PJBASE_ARM=/home/dartalexx/voicemail/pjproject_arm
ifeq ($(MAKECMDGOALS),server)
include $(PJBASE)/build.mak
endif
ifeq ($(MAKECMDGOALS),)
include $(PJBASE)/build.mak
endif
ifeq ($(MAKECMDGOALS),arm_server)
include $(PJBASE_ARM)/build.mak
endif
CC      = $(PJ_CC)
LDFLAGS = $(PJ_LDFLAGS)
CFLAGS  = $(PJ_CFLAGS)
CPPFLAGS= ${CFLAGS}
LDLIBS = $(PJ_LDLIBS)

ARM_CC  = /opt/gcc-linaro-5.5.0-2017.10-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc
all: server

server: uas_working.c
	$(CC) -g -o $@ $< $(CFLAGS) $(LDFLAGS) $(LDLIBS)

arm_server: uas_working.c
	$(ARM_CC) -o $@ $< $(CFLAGS) $(LDFLAGS) -L/usr/local/lib $(LDLIBS)

test: uas_off.c
	$(CC) -g -o $@ $^ $(CFLAGS) $(LDFLAGS) -lgomp  $(LDLIBS)

clean:
	rm -f uas*.o arm_server server test client
