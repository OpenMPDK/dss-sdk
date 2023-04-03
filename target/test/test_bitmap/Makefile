CPPFLAGS = -Wall -g -std=c++11
LIBS = -lcppunit
INCLUDES = -I/usr/include/cppunit

test_bitmap: test_bitmap_impl.o bitmap_impl.o
		g++ -o test_bitmap test_bitmap_impl.o bitmap_impl.o ${CPPFLAGS} ${LIBS}

test_bitmap_impl.o: test_bitmap_impl.cc
		g++ ${CPPFLAGS} -c ${INCLUDES} test_bitmap_impl.cc

bitmap_impl.o: bitmap_impl.cc
		g++  ${CPPFLAGS} -c bitmap_impl.cc


clean:
		rm test_bitmap test_bitmap_impl.o bitmap_impl.o
