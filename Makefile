CXX = g++
CXXFLAGS = -Wall -O2
LDFLAGS = -lX11 -lXext -lXtst

SRC = main.cpp
OBJ = $(SRC:.cpp=.o)
TARGET = strix

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
