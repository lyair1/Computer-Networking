#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> // for open flags
#include <time.h> // for time measurement
#include <assert.h>
#include <errno.h> 
#include <string.h>
#include <unistd.h>

//Sockets
#include <sys/socket.h>
//Internet addresses
#include <netinet/in.h>
//Working with Internet addresses
#include <arpa/inet.h>
//Domain Name Service (DNS)
#include <netdb.h>
//Working with errno to report errors
#include <errno.h>

struct gameData{
	int valid;
	int win;
	int isMisere;
	int heapA;
	int heapB;
	int heapC;
	int heapD;
};

struct move{
	int heap;
	int amount;
};

int msgSize = 50;

int connectToServer(int sock, const char* address, char* port);
void printGameState(struct gameData game);
void printWinner(struct gameData game);
struct gameData parseDataFromServer(char buf[msgSize]);
struct gameData receiveDataFromServer(int sock);
void printValid(struct gameData game);
struct move getMoveFromInput(int sock);
int sendAll(int s, char *buf, int *len);
int receiveAll(int s, char *buf, int *len);
void checkForZeroValue(int num, int sock);

int main(int argc, char const *argv[])
{ 
	/*printf("Client Started\n");*/
	char port[20];
	char address[50];

	assert(argc >= 1 && argc <= 3);

	if (argc < 2)
	{
		strcpy(address,"localhost");
	}else{
		strcpy(address,argv[1]);
	}

	if (argc < 3)
	{
		strcpy(port,"6325");
	}
	else
	{
		strcpy(port,argv[2]);
	}

	/*printf("trying to get socket\n");*/
	// Get socket
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1)
	{
		printf( "Error opening the socket: %s\n", strerror( errno ) );
   	   	return errno;
	}
	/*printf("Succesfully got a socket number: %d\n", sock);*/

	// Connect to server
	sock = connectToServer(sock, address, port);
	
	char buf[msgSize];
	struct move m;
	// Get initial data
	struct gameData game = receiveDataFromServer(sock);
	if (game.isMisere == 1)
	{
		printf("This is a Misere game\n");
	}else{
		printf("This is a Regular game\n");
	}

	printGameState(game);
	while(game.win == -1){
		m = getMoveFromInput(sock);
		sprintf(buf, "%d$%d", m.heap,m.amount);
		if(sendAll(sock, buf, &msgSize) == -1){
			close(sock);
			exit(0);
		}
		game = receiveDataFromServer(sock);

		// Check if move was valid
		printValid(game);
		// keep on playing
		printGameState(game);
	}

	printWinner(game);

	return 0;
}

int connectToServer(int sock, const char* address, char* port){
	struct addrinfo hints, *servinfo, *p;
	int rv;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; // use AF_INET6 to force IPv6
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(address, port, &hints, &servinfo)) != 0) {
	//if ((rv = getaddrinfo("nova.cs.tau.ac.il", "6325", &hints, &servinfo)) != 0) {
    	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    	close(sock);
    	exit(1);
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
   	 	if ((sock = socket(p->ai_family, p->ai_socktype,
        	    p->ai_protocol)) == -1) {
       	 	perror("socket");
        	continue;
    	}

    	/*printf("trying to connect\n");*/
    	if (connect(sock, p->ai_addr, p->ai_addrlen) == -1) {
      	  	//close(sock);
       	 	perror("connect");
        	continue;
    	}

    	/*printf("Connected\n");*/

    	break; // if we get here, we must have connected successfully
	}

	if (p == NULL) {
    	// looped off the end of the list with no connection
    	fprintf(stderr, "failed to connect\n");
    	close(sock);
    	exit(2);
	}

	freeaddrinfo(servinfo); // all done with this structure

	return sock;
}

struct move getMoveFromInput(int sock){
	int heap, reduce;
	char heapC;
	char cmd[10];

	printf("Your turn:\n");

	fgets(cmd,10,stdin);
	// Exit if user put Q
	if (strcmp(cmd,"Q") == 0)
	{
		close(sock);
		exit(0);
	}

	sscanf(cmd,"%c %d", &heapC, &reduce);
	heap = (int)heapC - (int)'A';
	if (heap < 0 || heap > 3)
	{
		 printf("Illegal input!!!\n");
		 close(sock);
		 exit(1);
	}

	struct move m;
	m.heap = heap;
	m.amount = reduce;

	return m;
}

struct gameData receiveDataFromServer(int sock)
{
	char buf[msgSize];
	struct gameData game;
	int rec = receiveAll(sock, buf, &msgSize);

	if (rec == -1)
	{
		fprintf(stderr, "failed to receive initial data\n");
		close(sock);
    	exit(2);
	}

	game = parseDataFromServer(buf);

	/*printf("Data Received from server: %s\n",buf);*/

	return game;
}

void printValid(struct gameData game)
{
	if (game.valid == 1)
	{
		printf("Move accepted\n");
	}else{
		printf("Illegal move\n");
	}
}

void printWinner(struct gameData game)
{
	if (game.win == 1)
	{
		printf("You win!\n");
	}else if (game.win == 2)
	{
		printf("I win!\n");
	}	
}

void printGameState(struct gameData game){
	printf("Heap sizes are %d,%d,%d,%d\n",game.heapA, game.heapB, game.heapC, game.heapD);
}

struct gameData parseDataFromServer(char buf[msgSize]){
	struct gameData data;
	sscanf( buf, "%d$%d$%d$%d$%d$%d$%d", &data.valid, &data.win, &data.isMisere, &data.heapA, &data.heapB, &data.heapC, &data.heapD);

	return data;
}


int sendAll(int s, char *buf, int *len) {
	int total = 0; /* how many bytes we've sent */
	int bytesleft = *len; /* how many we have left to send */
   	int n;
	
	while(total < *len) {
			n = send(s, buf+total, bytesleft, 0);
			checkForZeroValue(n,s);
			if (n == -1) { break; }
			total += n;
			bytesleft -= n;
	  	}
	*len = total; /* return number actually sent here */
	  	
	return n == -1 ? -1:0; /*-1 on failure, 0 on success */
}

 int receiveAll(int s, char *buf, int *len) {
 	int total = 0; /* how many bytes we've received */
 	size_t bytesleft = *len; /* how many we have left to receive */
    int n;
	
	while(total < *len) {
			n = recv(s, buf+total, bytesleft, 0);
			checkForZeroValue(n,s);
			if (n == -1) { break; }
			total += n;
			bytesleft -= n;
	  	}
	*len = total; /* return number actually sent here */
	  	
	 return n == -1 ? -1:0; /*-1 on failure, 0 on success */
 }

 void checkForZeroValue(int num, int sock){
	if(num==0){
		printf( "Disconnected from server\n");
		close(sock);
		exit(1);
	}
}
