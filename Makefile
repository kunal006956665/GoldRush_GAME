renew: renew.cpp libmap.a goldchase.h
	g++ renew.cpp -o renew -L. -lmap -lpanel -lncurses

libmap.a: Screen.o Map.o
	ar -r libmap.a Screen.o Map.o

Screen.o: Screen.cpp
	g++ -c Screen.cpp

Map.o: Map.cpp
	g++ -c Map.cpp

clean:
	rm -f Screen.o Map.o libmap.a renew
