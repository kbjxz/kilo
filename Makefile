# ref: https://makefiletutorial.com/

.DELETE_ON_ERROR:
all: kilo

kilo: kilo.cpp
	mkdir -p ./bin
	g++ -Wall -Wextra -pedantic -g --std=c++20 -o ./bin/kilo kilo.cpp
	chmod +x ./bin/kilo

.PHONY: clean
clean:
	rm -f ./bin/kilo
