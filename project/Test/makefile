CXX = g++
CXXFLAGS = -I. -std=c++11 -I../common/ -lcrypto -lmysqlclient -L/usr/lib64/mysql 
DEPS = $(wildcard  ../Utility/*.cpp )
SRCS = $(wildcard *.cpp)
OBJ = $(patsubst %.cpp,%.o,$(SRCS))
BIN = $(patsubst %.cpp,%,$(SRCS))

all : $(OBJ)
	echo $(DEPS)
	$(MAKE) $(BIN)

% : %.o $(DEPS)
	$(CXX) -o $@ $^ $(CXXFLAGS)

%.o : %.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CXXFLAGS)

clean :
	rm -rf *.o $(BIN) .*~