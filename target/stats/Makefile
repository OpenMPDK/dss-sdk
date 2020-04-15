CFLAGS := -fPIC -O3 -g -Wall -Werror
LIB := -lconfig -lpthread
DEBUG := -g
INC := .
CC := gcc
MAJOR := 0
MINOR := 1
NAME := kv_stats
VERSION := $(MAJOR).$(MINOR)

lib: libkv_stats.so.$(VERSION)

test: kv_stats_test
	LD_LIBRARY_PATH=. 

kv_stats_lib.o: 
	$(CC) -c $(CFLAGS) $(LIB) -I$(INC) kv_stats_lib.c

kv_stats_dump: 
	$(CC) kv_stats_dump.c $(LIB) $(DEBUG) -o $@ -I$(INC) -L. -l$(NAME)

kv_stats_test: 
	$(CC) kv_stats_test.c $(LIB) $(DEBUG) -o $@ -I$(INC) -L. -l$(NAME)

lib$(NAME).so: libkv_stats.so.$(VERSION)
	ldconfig -v -n .
	ln -s lib$(NAME).so.$(MAJOR) lib$(NAME).so

libkv_stats.so.$(VERSION): kv_stats_lib.o
	$(CC) -shared -Wl,-soname,libkv_stats.so.$(MAJOR) $^ -o $@

all: lib lib$(NAME).so $(NAME)_test 

clean:
	$(RM) $(NAME)_test $(NAME)_dump *.o *.so*
