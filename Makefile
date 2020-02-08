CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -std=c++17 -g -o0
#-fsanitize=address -fsanitize=leak
#-fsanitize=thread
SRC = test.cc
EXEC = test

all: $(EXEC)

$(EXEC): test.cc lockfree_linkedlist.h reclaimer.h
	$(CXX) $(CXXFLAGS) -o $(EXEC) test.cc

.Phony : clean

clean:
	rm -rf $(EXEC)