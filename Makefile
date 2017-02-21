INSTALL_PATH = /usr/local
BIN_INSTALL_PATH = $(INSTALL_PATH)/bin
LIB_INSTALL_PATH = $(INSTALL_PATH)/lib
INC_INSTALL_PATH = $(INSTALL_PATH)/include


MYSQL_LIB_PATH=/usr/local/mysql/lib
MYSQL_LIBS=-lmysqlclient

INC_PATH = -I $(INC_INSTALL_PATH)
LIB_PATH = -L $(MYSQL_LIB_PATH)
LIBS = -liconv -lfcgi -ltpl -lsphinxclient -lscws -ltokyotyrant -ltokyocabinet -lz -lpthread
LIBS2 = -lgkdp -lgkdal -lgktt -lgkse -lgkmy -lgksw -lgkl $(MYSQL_LIBS)
CURLIB =  -lgkwbe 

CC = gcc
CXX = g++
MTFlAGS = -D_REENTRANT
MACROS = 
NDBG = -DNDEBUG
SOFLAGS = -fPIC
CFLAGS = -Wall $(MACROS) $(MTFlAGS) $(INC_PATH)
CXXFLAGS = -Wall $(MACROS) $(MTFlAGS) $(INC_PATH)
RM = rm -f
AR = ar
ARFLAGS = rcv
MAKE = gmake

LINKERNAME = libfastcgi-framework.so
SONAME = $(LINKERNAME).1
LIBNAME = $(LINKERNAME).1.0.0

LIB_TARGET = $(LIBNAME) 
BIN_TARGET = wbe
TARGET = $(LIB_TARGET) $(BIN_TARGET)

LIB_OBJS = cgic.o fcgix.o htmtmpl.o captcha.o session.o referer.o snapshot.o dbi.o form.o page.o action.o security.o
BIN_OBJS = wbe.o redirect.o
OBJS = $(LIB_OBJS) $(BIN_OBJS)
INCS = cgic.h fcgix.h htmtmpl.h captcha.h session.h referer.h snapshot.h dbi.h form.h page.h action.h security.h

all: $(LIB_TARGET) $(BIN_TARGET) 
.PHONE:all

$(LIBNAME): CFLAGS += $(SOFLAGS)
$(LIBNAME): $(LIB_OBJS)
	$(CC) -shared -Wl,-soname,$(SONAME) -o $@ $^

wbe: wbe.o
	$(CC) -o $@ $^ $(LIB_PATH) $(CURLIB) $(LIBS2) $(LIBS)

redirect: redirect.o cgic.o fcgix.o
	$(CC) -o $@ $^ $(LIB_PATH) -lfcgi -lgkl -ltokyotyrant -ltokyocabinet -liconv -lz -lpthread

debug: CFLAGS += -g
debug: CXXFLAGS += -g
debug: all

lib_install:
	cp -f  --target-directory=$(INC_INSTALL_PATH) $(INCS)
	cp -f $(LIB_TARGET) $(LIB_INSTALL_PATH)
	cp -f $(LIB_TARGET) $(SYSTEM_LIB_PATH)
	ln -fs $(LIB_TARGET)  $(SYSTEM_LIB_PATH)/$(LINKERNAME)

bin_install:
	cp -f $(BIN_TARGET) $(BIN_INSTALL_PATH)
ifdef BLUEFIRE_RUN_PATH
	cp -f $(BIN_TARGET) $(BLUEFIRE_RUN_PATH)
endif

clean:
	$(RM) $(TARGET) $(OBJS)

test_dbi: 
	gcc -Wall -g -o test_dbi -DTEST_DBI dbi.c $(LIBS) $(LIBS2) $(INC_PATH)

test_form: 
	gcc -Wall -g -o test_form -DTEST_FORM form.c $(LIBS) $(LIBS2) $(INC_PATH)

test_page: 
	gcc -Wall -g -o test_page -DTEST_PAGE page.c $(LIBS) $(LIBS2) $(INC_PATH)

test_action: 
	gcc -Wall -g -o test_action -DTEST_ACTION action.c $(LIBS) $(LIBS2) $(INC_PATH)

test_captcha: 
	gcc -Wall -g -o test_captcha -DTEST_CAPTCHA captcha.c 

test_session:
	gcc  -Wall -g -o test_session -DTEST_SESSION session.c -lgkwbe $(LIBS) $(LIBS2) $(INC_PATH)

test_referer:
	gcc  -Wall -g -o test_referer -DTEST_REFERER referer.c $(LIBS) $(LIBS2) $(INC_PATH)

test_htmtmpl:
	gcc  -Wall -g -o test_htmtmpl -DTEST_HTMTMPL htmtmpl.c $(LIBS) $(LIBS2) $(INC_PATH)

test_secure:
	gcc  -Wall -g -o test_secure -DTEST_SECURE security.c $(LIBS) $(LIBS2) $(INC_PATH) $(LIB_PATH)

