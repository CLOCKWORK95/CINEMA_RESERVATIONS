all:
		gcc tcpClient.c -o client -g
		gcc -pthread tcpServer.c -o server -g

clean:
		rm client
		rm server
