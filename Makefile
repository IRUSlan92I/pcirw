.PHONY: all clean

CC = qcc  # ntox86-gcc
CXX = g++
LFLAGS = -Wall #-Vgcc_ntox86
LIBS = #-l socket

BIN_PATH = ./bin
SOURCE_PATH = ./src

TARGET = $(BIN_PATH)/pcirw

SRCS = $(SOURCE_PATH)/main.c

all:
	mkdir -p $(BIN_PATH)
	$(CXX) $(LFLAGS) -o $(TARGET) $(SRCS) $(LIBS)

$(BIN_PATH):
	mkdir -p $(BIN_PATH)

clean:
	rm -f $(TARGET)




