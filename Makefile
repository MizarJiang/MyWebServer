# server: main.o http_conn.o
# 	g++ -o server main.o http_conn.o -lpthread

# # main.o:main.cpp ./lock/locker.h ./threadpoll/threadPoll.h ./http/http_conn.h
# # 	g++ -c main.cpp -lpthread
# # http_conn.o: ./http/http_conn.h
# # 	g++ -c ./http/http_conn.cpp

# main.o: main.cpp
# 	g++ -c main.cpp
# http_conn.o: ./http/http_conn.cpp
# 	g++ -c ./http/http_conn.cpp

# .PHONY: clean
# clean: 
# 	-rm *.o


CXX = g++
CFLAGS = -std=c++14 -O2 -Wall -g 

TARGET = server
OBJS = ./CGImysql/*.cpp ./http/*.cpp \
       ./log/*.cpp ./main.cpp

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o ../bin/$(TARGET)  -pthread -lmysqlclient

clean:
	rm -rf ../bin/$(OBJS) $(TARGET)

