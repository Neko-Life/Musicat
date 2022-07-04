# Specify compile
CXX = g++

# Specify compiler flag
CCF = -std=c++17 -Wall -Wextra -g

# Specify include folder
INC = -Iinclude # -I/usr/include/opus # -lxml2 -lz -llzma -licui18n -licuuc -licudata -lm

# Specify libs to use
LIB = -ldpp -pthread -lcurl -lcurlpp -logg # -lopus -lopusfile # -lllhttp

# Specify source file
SRC = src/musicat/*.cpp src/musicat/cmds/*.cpp src/main.cpp # libs/opusfile/src/*.c # src/include/*.cpp

# Specify out file
OUT = exe/Shasha

all: $(SRC)
	$(CXX) $(CCF) $(INC) $(LIB) $(SRC) -o $(OUT)

ex:
	g++ -Wall -Wextra -g -Iinclude -lcurlpp -lcurl exec.cpp src/musicat/yt_search.cpp src/musicat/encode.cpp -o ex

ex2:
	g++ -Wall -Wextra -g -Iinclude exec2.cpp -o ex2

d:
	g++ -Wall -Wextra -Iinclude -lcurlpp -lcurl d.cpp -o d
