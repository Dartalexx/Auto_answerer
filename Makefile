PJBASE=$(CURDIR)/pjproject
PJBASE_ARM=$(CURDIR)/pjproject_arm
ifeq ($(MAKECMDGOALS),server)
include $(PJBASE)/build.mak
endif
ifeq ($(MAKECMDGOALS),)
include $(PJBASE)/build.mak
endif
ifeq ($(MAKECMDGOALS), noopt)
include $(PJBASE)/build.mak
endif
ifeq ($(MAKECMDGOALS),arm_server)
include $(PJBASE_ARM)/build.mak
endif
CC      = $(PJ_CC)
LDFLAGS = $(PJ_LDFLAGS)
CFLAGS  = $(PJ_CFLAGS)
CFLAGS_NOOPT = -DPJ_AUTOCONF=1 \
-O0 -DPJ_IS_BIG_ENDIAN=0 -DPJ_IS_LITTLE_ENDIAN=1 -DPJMEDIA_USE_OLD_FFMPEG=1 \
-I/home/dartalexx/auto_answerer/pjproject/pjlib/include -I/home/dartalexx/auto_answerer/pjproject/pjlib-util/include \
-I/home/dartalexx/auto_answerer/pjproject/pjnath/include -I/home/dartalexx/auto_answerer/pjproject/pjmedia/include \
-I/home/dartalexx/auto_answerer/pjproject/pjsip/include
CPPFLAGS = ${CFLAGS}
LDLIBS = $(PJ_LDLIBS)

ARM_CC  = /opt/gcc-linaro-5.5.0-2017.10-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc
all: server

noopt: uas.c
	gcc -g -o $@ $< $(CFLAGS_NOOPT) $(LDFLAGS) $(LDLIBS)
server: uas.c
	$(CC) -g -o $@ $< $(CFLAGS) $(LDFLAGS) $(LDLIBS)
arm_server: uas.c
	$(ARM_CC) -o $@ $< $(CFLAGS) $(LDFLAGS) -L/usr/local/lib $(LDLIBS)

clean:
	rm -f arm_server noopt server
