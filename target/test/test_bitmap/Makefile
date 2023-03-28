test_bitmap: test_bitmap_impl.o bitmap_impl.o
		g++ -o test_bitmap test_bitmap_impl.o bitmap_impl.o -g -std=c++11

test_bitmap_impl.o: test_bitmap_impl.cc
		g++ -g -c test_bitmap_impl.cc -std=c++11

bitmap_impl.o: bitmap_impl.cc
		g++ -g -c bitmap_impl.cc -std=c++11


clean:
		rm test_bitmap test_bitmap_impl.o bitmap_impl.o
