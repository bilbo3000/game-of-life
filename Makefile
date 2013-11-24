all : game_of_life

game_of_life : game_of_life.o
	mpicc game_of_life.o -o game_of_life

game_of_life.o: 
	mpicc -c game_of_life.c -o game_of_life.o

clean : 
	rm -rf ./game_of_life
	rm -rf ./*.o
