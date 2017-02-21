a.out: http_parse.o
	g++ -o a.out http_parse.o

http_parse.o:http_parse.cpp
	g++ -c http_parse.cpp
clean:
	rv *.o
