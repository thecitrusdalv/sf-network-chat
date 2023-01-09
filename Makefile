SERVER_SRC_PATH = ./server/src/
SERVER_HEADER_PATH = ./server/headers/
SERVER_OBJ_PATH = ./server/obj/
SERVER_OBJ_MODULES = server.o message.o user.o fileSubsystem.o networkSubsystem.o

CLIENT_SRC_PATH = ./client/src/
CLIENT_OBJ_PATH = ./client/obj/
CLIENT_HEADER_PATH = ./client/headers/
CLIENT_OBJ_MODULES = ./client/obj/*.o

CXX = g++
CXXFLAGS = -g -Wall
CXX+FLAGS = $(CXX) $(CXXFLAGS)

all: server client

server: $(SERVER_SRC_PATH)main.cpp $(SERVER_OBJ_MODULES)
	$(CXX+FLAGS) $< $(SERVER_OBJ_PATH)*.o -o chat_server

server.o: $(SERVER_SRC_PATH)server.cpp $(SERVER_HEADER_PATH)server.h
	$(CXX+FLAGS) -c $< -o $(SERVER_OBJ_PATH)$@

message.o: $(SERVER_SRC_PATH)message.cpp $(SERVER_HEADER_PATH)server.h
	$(CXX+FLAGS) -c $< -o $(SERVER_OBJ_PATH)$@

user.o: $(SERVER_SRC_PATH)user.cpp $(SERVER_HEADER_PATH)server.h
	$(CXX+FLAGS) -c $< -o $(SERVER_OBJ_PATH)$@

fileSubsystem.o: $(SERVER_SRC_PATH)fileSubsystem.cpp $(SERVER_HEADER_PATH)fileSubsystem.h
	$(CXX+FLAGS) -c $< -o $(SERVER_OBJ_PATH)$@

networkSubsystem.o: $(SERVER_SRC_PATH)networkSubsystem.cpp $(SERVER_HEADER_PATH)networkSubsystem.h
	$(CXX+FLAGS) -c $< -o $(SERVER_OBJ_PATH)$@


client: $(CLIENT_SRC_PATH)main.cpp client.o $(SERVER_OBJ_PATH)server.o
	$(CXX+FLAGS) $< $(CLIENT_OBJ_PATH)*.o $(SERVER_OBJ_PATH)*.o -o chat_client

client.o: $(CLIENT_SRC_PATH)client.cpp ${CLIENT_HEADER_PATH}argsProcessing.h
	$(CXX+FLAGS) -c $< -o $(CLIENT_OBJ_PATH)$@ 


clean:
	rm -f client/obj/*.o server/obj/*.o chat_server chat_client
