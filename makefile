OBJ = min_heap_timer.o http_conn.o webserver.o

%.o: %.cpp
	g++ -c -o $@  $< -I . -pthread

server: $(OBJ)
	g++ -o $@ $^ -I . -pthread

clean:
	rm *.o
