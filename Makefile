CC = g++
CCFLAGS = -std=c++0x -I .
LDFLAGS = -lboost_regex

bgpcompare:
	$(CC) BgpCompare.cpp $(CCFLAGS) $(LDFLAGS) -o bgpcompare

	
