all: confy

confy: confy.cpp
	g++ --std=c++17 -g -o confy confy.cpp
