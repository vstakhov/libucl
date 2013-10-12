CC ?= gcc
DESTDIR ?= /usr/local
LD ?= gcc
C_COMMON_FLAGS ?= -fPIC -Wall -W -Wno-unused-parameter -Wno-unused-variable -Wno-pointer-sign -I./include -I./uthash -I./src
MAJOR_VERSION = 0
MINOR_VERSION = 1
VERSION = "$(MAJOR_VERSION).$(MINOR_VERSION)"
SONAME = libucl.so.$(MAJOR_VERSION)
OBJDIR ?= ./.obj
SRCDIR ?= ./src
MKDIR ?= mkdir
INSTALL ?= install
RM ?= rm
RMDIR ?= rmdir
LD_SHARED_FLAGS ?= -Wl,-soname,$(SONAME) -shared

all: $(OBJDIR)/$(SONAME)

$(OBJDIR)/$(SONAME): $(OBJDIR)/ucl_util.o $(OBJDIR)/ucl_parser.o $(OBJDIR)/ucl_emitter.o $(OBJDIR)
	$(CC) -o $(OBJDIR)/$(SONAME) $(OBJDIR)/ucl_util.o $(OBJDIR)/ucl_parser.o $(OBJDIR)/ucl_emitter.o $(LD_SHARED_FLAGS) $(LDFLAGS) $(SSL_LIBS) $(FETCH_LIBS)

$(OBJDIR):
	$(MKDIR) $(OBJDIR)

$(OBJDIR)/ucl_util.o: $(SRCDIR)/ucl_util.c $(OBJDIR)
	$(CC) -o $(OBJDIR)/ucl_util.o $(CPPFLAGS) $(CFLAGS) $(C_COMMON_FLAGS) $(SSL_CFLAGS) $(FETCH_FLAGS) -c $(SRCDIR)/ucl_util.c
$(OBJDIR)/ucl_parser.o: $(SRCDIR)/ucl_parser.c $(OBJDIR)
	$(CC) -o $(OBJDIR)/ucl_parser.o $(CPPFLAGS) $(CFLAGS) $(C_COMMON_FLAGS) $(SSL_CFLAGS) $(FETCH_FLAGS) -c $(SRCDIR)/ucl_parser.c
$(OBJDIR)/ucl_emitter.o: $(SRCDIR)/ucl_emitter.c $(OBJDIR)
	$(CC) -o $(OBJDIR)/ucl_emitter.o $(CPPFLAGS) $(CFLAGS) $(C_COMMON_FLAGS) $(SSL_CFLAGS) $(FETCH_FLAGS) -c $(SRCDIR)/ucl_emitter.c

clean:
	$(RM) $(OBJDIR)/*.o $(OBJDIR)/$(SONAME) $(OBJDIR)/chargen
	$(RMDIR) $(OBJDIR)
	
chargen: $(OBJDIR) utils/chargen.c
	$(CC) -o $(OBJDIR)/chargen $(CPPFLAGS) $(CFLAGS) $(C_COMMON_FLAGS) $(SSL_CFLAGS) $(FETCH_FLAGS) $(LDFLAGS) utils/chargen.c

install: $(OBJDIR)/$(SONAME)
	$(INSTALL) -m0755 $(SONAME) $(DESTDIR)/lib/$(SONAME)
	$(INSTALL) -m0644 include/ucl.h $(DESTDIR)/include/ucl.h

.PHONY: clean
