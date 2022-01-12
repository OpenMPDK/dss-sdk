#
#   The Clear BSD License
#
#   Copyright (c) 2022 Samsung Electronics Co., Ltd.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted (subject to the limitations in the
#   disclaimer below) provided that the following conditions are met:
#
#   	* Redistributions of source code must retain the above copyright
#   	  notice, this list of conditions and the following disclaimer.
#   	* Redistributions in binary form must reproduce the above copyright
#   	  notice, this list of conditions and the following disclaimer in
#   	  the documentation and/or other materials provided with the distribution.
#   	* Neither the name of Samsung Electronics Co., Ltd. nor the names of its
#   	  contributors may be used to endorse or promote products derived from
#   	  this software without specific prior written permission.
#
#   NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED
#   BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
#   CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
#   BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
#   FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
#   COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
#   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
#   NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
#   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
#   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
#   THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
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
