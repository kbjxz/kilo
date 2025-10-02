# ref: https://makefiletutorial.com/

.DELETE_ON_ERROR:
all: kilo

kilo: kilo.cpp
	g++ -Wall -Wextra -pedantic -g --std=c++20 -o kilo
	chmod +x ./kilo

.PHONY: clean
clean:
	rm -f ./kilo
