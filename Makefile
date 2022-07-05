# Specify compile
CXX = g++

# Specify compiler flag
CCF = -std=c++17 -MMD -Wall -Wextra -g

# Specify include folder
INC = -Iinclude # -I/usr/include/opus # -lxml2 -lz -llzma -licui18n -licuuc -licudata -lm

# Specify libs to use
LIB = -ldpp -pthread -lcurl -lcurlpp -logg # -lopus -lopusfile # -lllhttp

# Specify source file
SRC = \
src/musicat/yt-playlist.cpp \
src/musicat/slash.cpp \
src/musicat/player.cpp \
src/musicat/yt-search.cpp \
src/musicat/yt-track-info.cpp \
src/musicat/cli.cpp \
src/musicat/musicat.cpp \
src/musicat/cmds/move.cpp \
src/musicat/cmds/loop.cpp \
src/musicat/cmds/invite.cpp \
src/musicat/cmds/hello.cpp \
src/musicat/cmds/autoplay.cpp \
src/musicat/cmds/remove.cpp \
src/musicat/cmds/play.cpp \
src/musicat/cmds/skip.cpp \
src/musicat/cmds/queue.cpp \
src/musicat/cmds/pause.cpp \
src/musicat/encode.cpp \
src/musicat/run.cpp \
src/main.cpp # libs/opusfile/src/*.c # src/include/*.cpp

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
	rm $(OBJS) $(DSFILES)

ex:
	g++ -Wall -Wextra -g -Iinclude -lcurlpp -lcurl exec.cpp src/musicat/yt_search.cpp src/musicat/encode.cpp -o ex

ex2:
	g++ -Wall -Wextra -g -Iinclude exec2.cpp -o ex2

d:
	g++ -Wall -Wextra -Iinclude -lcurlpp -lcurl d.cpp -o d
