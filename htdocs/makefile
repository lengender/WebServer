OBJ= post.o web_sql.o

%.o:%.cpp
	g++ -c -w -o $@ $< -I. `mysql_config --cflags --libs`

post.cgi: $(OBJ)
	g++ -w -o $@ $^ -I. `mysql_config --cflags --libs`

clean:
	rm *.o
