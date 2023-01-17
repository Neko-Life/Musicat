# Specify compile
CXX = g++

# Specify compiler flag
CCF = -std=c++17 -MMD -Wall -Wextra -g

# Specify include folder
INC = -Iinclude # -I/usr/include/opus # -lxml2 -lz -llzma -licui18n -licuuc -licudata -lm

# Specify libs to use
LIB = -ldpp -pthread -lcurl -lcurlpp -logg -lpq -licui18n -licuuc -licudata -licuio # -lopus -lopusfile # -lllhttp

# Specify source file
SRC = $(wildcard src/musicat/*.cpp) $(wildcard src/musicat/cmds/*.cpp) src/main.cpp # libs/opusfile/src/*.c # src/include/*.cpp

OBJS = $(SRC:.cpp=.o)
DSFILES = $(SRC:.cpp=.d)
include $(DSFILES)

# Specify out file
OUT = exe/Shasha

all: $(OBJS)
	$(CXX) $(CCF) $(INC) $(LIB) $(OBJS) -o $(OUT)

$(DSFILES): $(SRC)
	$(CXX) $(CCF) $(INC) $(LIB) -c $(@:.d=.cpp) -o $(@:.d=.o)

clean:
	rm $(OBJS) $(DSFILES) $(OUT)
