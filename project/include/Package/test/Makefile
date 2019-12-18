CXX = nvcc
CXXFLAGS = -I. -I../ -std=c++11
DEPS = $(wildcard ../*.cpp)
SRCS = $(wildcard *.cpp)
OBJ = $(patsubst %.cpp,%.o,$(SRCS))
BIN = test
all : $(OBJ)
	$(MAKE) $(BIN)

%.o : %.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CXXFLAGS)

% : %.o $(DEPS)
	$(CXX) -o $@ $^ $(CXXFLAGS)

clean :
	rm -rf *.o $(BIN) .*~