CXX = g++
CFLAGS = -std=c++11 -O2 -Wall -g 

TARGET = server
OBJS = ./CGImysql/*.cpp ./http/*.cpp \
       ./log/*.cpp ./main.cpp

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o $(TARGET)  -pthread -lmysqlclient

clean:
	rm -rf ../bin/$(OBJS) $(TARGET)

