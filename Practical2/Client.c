#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> // for open flags
#include <time.h> // for time measurement
#include <assert.h>
#include <errno.h> 
#include <string.h>
#include <unistd.h>
#define STDIN 0

// select
#include <sys/select.h>
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

int MSG_SIZE=300;

struct gameData{
	int valid; 
	int msg; // <sender Id> - this is a message, (-1) - this is not a msg
	int isMyTurn; // 0 - no, 1 - yes
	int win; // 0 - no one, <player id> - the player id who won
	int numOfPlayers; // p - then number of players the server allows to play
	int myPlayerId; // player id (0 - p-1), if i dont play: -1
	int playing; // 0 - viewing, 1 - playing
	int isMisere;
	int heapA;
	int heapB;
	int heapC;
	int heapD;
	int moveCount; // amount of move that were made
	char msgTxt[MSG_SIZE-100];
};

struct clientMsg{
	int heap;
	int amount;
	int msg; // 1 - this is a message, 0 - this is a move
	int recp; // player id to send the message to (0 - p-1)
	int moveCount; // amount of move that were made
	char msgTxt[MSG_SIZE-100];
};

// client globals
int playerId, playing, isMisere, moveCount;

int connectToServer(int sock, const char* address, char* port);
void printGameState(struct gameData game);
void printWinner(struct gameData game);
struct gameData receiveDataFromServer(int sock);
void printValid(struct gameData game);
struct clientMsg getMoveFromInput(int sock);

// common
int sendAll(int s, char *buf);
int receiveAll(int s, char *buf, int *len);
void checkForZeroValue(int num, int sock);
void checkForNegativeValue(int num, char* func, int sock);
int parseClientMsg(char buf[MSG_SIZE], struct clientMsg *data);
void createClientMsgBuff(struct clientMsg data, char* buf);
void createGameDataBuff(struct gameData data, char* buf);
int parseGameData(char buf[MSG_SIZE], struct gameData* data);

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
	printf("Succesfully got a socket number: %d\n", sock);

	// Connect to server
	sock = connectToServer(sock, address, port);
	
	char buf[MSG_SIZE];
	struct clientMsg m;
	// Get initial data
	struct gameData game = receiveDataFromServer(sock);
	//got the initial data from the server
	if (game.valid == 0)
	{
		 printf("Client rejected: too many clients are already connected\n");
		 return 0;
	}

	playing = game.playing;
	playerId = game.myPlayerId;
	isMisere = game.isMisere;
	moveCount = moveCount;

	if (isMisere == 1)
	{
		printf("This is a Misere game\n");
	}else{
		printf("This is a Regular game\n");
	}

	printf("Number of players is %d\n", game.numOfPlayers);
	if (playing == 1)
	{
		 printf("You are client %d\n", playerId);
		 printf("You are playing\n");
	}else{
		 printf("You are only viewing\n");
	}

	struct fd_set fdSetRead, fdSetWrite;

	//printGameState(game);
	while(game.win == 0){
		// m = getMoveFromInput(sock);
		// sprintf(buf, "%d$%d", m.heap,m.amount);
		// if(sendAll(sock, buf, &MSG_SIZE) == -1){
		// 	close(sock);
		// 	exit(0);
		// }
		// game = receiveDataFromServer(sock);

		// // Check if move was valid
		// printValid(game); 
		// // keep on playing
		// printGameState(game);

		// clear set and add listner

		// TODO: need to add timeout

		int maxClientFd = sock;

		FD_ZERO(&fdSetRead);
		FD_ZERO(&fdSetWrite);
		if (playing == 1)
		{
			// not only a viewer
			FD_SET(STDIN, &fdSetRead);
		}
		
		FD_SET(sock, &fdSetRead);

		int fdReady = select(maxClientFd+1, &fdSetRead, &fdSetWrite, NULL, NULL);

		if(fdReady == 0){
			continue;
		}

		char readBuf[MSG_SIZE];
		int rSize = MSG_SIZE;
		
		if(FD_ISSET(STDIN , &fdSetRead) == 1){
			// there is input from cmd
			receiveAll(STDIN, readBuf, &rSize);
			printf("Read from STDIN:%s\n", readBuf);

			struct clientMsg cm;
			parseClientMsg(readBuf, &cm);

			if (cm.msg == 1)
			{
				// this is message

			}
		}

		if(FD_ISSET(sock , &fdSetRead) == 1){
			receiveAll(sock, readBuf, &rSize);
			printf("Read from sock rcv:%s\n", readBuf);
		}


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

struct clientMsg getMoveFromInput(int sock){
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

	struct clientMsg m;
	m.heap = heap;
	m.amount = reduce;

	return m;
}

struct gameData receiveDataFromServer(int sock)
{
	char buf[MSG_SIZE];
	struct gameData game;
	int rec = receiveAll(sock, buf, &MSG_SIZE);

	if (rec == -1)
	{
		fprintf(stderr, "failed to receive initial data\n");
		close(sock);
    	exit(2);
	}

	parseGameData(buf, &game);

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
	if (game.playing == 0)
	{
		printf("Game over!\n");

		return;
	}

	if (game.win == playerId)
	{
		printf("You win!\n");
	}else
	{
		printf("You lose!\n");
	}	
}

void printGameState(struct gameData game){
	printf("Heap sizes are %d,%d,%d,%d\n",game.heapA, game.heapB, game.heapC, game.heapD);
}

int sendAll(int s, char *buf) {
	int total = 0; /* how many bytes we've sent */
	int bytesleft = strlen(buf); /* how many we have left to send */
   	int n;
   	int len = strlen(buf);
	
	while(total < len) {
			n = send(s, buf+total, bytesleft, 0);
			checkForZeroValue(n,s);
			if (n == -1) { break; }
			total += n;
			bytesleft -= n;
	  	}
	  	
	return n == -1 ? -1:0; /*-1 on failure, 0 on success */
}

 int receiveAll(int s, char *buf, int *len) {
 	int total = 0; /* how many bytes we've received */
 	size_t bytesleft = *len; /* how many we have left to receive */
    int n;
	
	struct gameData gd;

	while(total < *len && parseGameData(buf, &gd) < 14) {
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

void checkForNegativeValue(int num, char* func, int sock){
	if(num<0){
		printf( "Error: %s\n", strerror(errno) );
		close(sock);
		exit(1);
	}
}

void createClientMsgBuff(struct clientMsg data, char* buf){
	sprintf(buf, "%d$%d$%d$%d$%d$%s$",
	 data.heap,
	 data.amount,
	 data.msg,
	 data.recp,
	 data.moveCount, 
	 data.msgTxt);
}

 int parseClientMsg(char buf[MSG_SIZE], struct clientMsg *data){
	return sscanf(buf, "%d$%d$%d$%d$%d$%s$",
	 &data->heap,
	 &data->amount,
	 &data->msg,
	 &data->recp,
	 &data->moveCount,
	 &data->msgTxt[0]);
}

 int parseGameData(char buf[MSG_SIZE], struct gameData* data){
	return sscanf( buf, "%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%s$",
	 &data->valid,
	 &data->isMyTurn,
	 &data->msg,
	 &data->win,
	 &data->numOfPlayers,
	 &data->myPlayerId, 
	 &data->playing, 
	 &data->isMisere, 
	 &data->heapA, 
	 &data->heapB, 
	 &data->heapC, 
	 &data->heapD,
	 &data->moveCount,
	 &data->msgTxt[0]);
}

void createGameDataBuff(struct gameData data, char* buf){
	sprintf(buf, "%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%s$",
	 data.valid,
	 data.isMyTurn,
	 data.msg,
	 data.win,
	 data.numOfPlayers,
	 data.myPlayerId, 
	 data.playing, 
	 data.isMisere, 
	 data.heapA, 
	 data.heapB, 
	 data.heapC, 
	 data.heapD,
	 data.moveCount,
	 data.msgTxt);
}
