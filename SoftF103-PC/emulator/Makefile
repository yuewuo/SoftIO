INCLUDES=-I../ -I../../

all: softf103-pseudo

softf103-pseudo: softf103-pseudo.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $< -lcurses

clean:
	rm softf103-pseudo
