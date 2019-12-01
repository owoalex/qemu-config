CC = g++
CFLAGS = -Wall -g -std=c++11
PACKAGE = `pkg-config --cflags --libs gtk+-3.0`
LIBS = `pkg-config --libs gtk+-3.0`
INC=-I./include

default:	main

main:	src/main.cpp
	$(CC) $(PACKAGE) $(CFLAGS) $(INC) -o build/qemu-config src/main.cpp

clean:
	rm -r build/*
