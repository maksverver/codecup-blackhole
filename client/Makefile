all: arbiter random-player

CXXFLAGS=-std=c++11 -Wall -Wextra -Os -g -D_GLIBCXX_DEBUG

arbiter: arbiter.cc
	$(CXX) $(CXXFLAGS) -o $@ $<

random-player: random-player.cc
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f arbiter random-player
