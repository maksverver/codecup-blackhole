all: random-player

random-player: random-player.cc
	$(CXX) -std=c++11 -o $@ $< -Os

clean:
	rm -f random-player
