all: confy

confy: confy.cpp ast_def.hpp ast_impl.hpp parser_utils.hpp
	g++ --std=c++17 -g -o confy confy.cpp
