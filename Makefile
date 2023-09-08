all: server client 

clean:
	rm -rf kcp_server kcp_client kcp_test

server:
	g++ -std=c++17 -o kcp_server server.cpp kcp_server.cpp udpsocket.cpp  -lpthread -Wall -g

client:
	g++ -std=c++17 -o kcp_client kcp_client.cpp udpsocket.cpp client.cpp -lpthread -Wall -g

test:
	g++ -std=c++17 -o kcp_test test.cpp ikcp.c -Wall -g
