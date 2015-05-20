
all: main


main: main.cpp
	g++ main.cpp -o photopuzzle -lSDL2 -lSDL2_ttf -lSDL2_image -ggdb -Wall
