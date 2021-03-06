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

#define MSG_SIZE 300

struct gameData{
	int valid; 
	int msg; // <sender Id> - this is a message, (-1) - send to all - (0) this is not a msg
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
	int moveCount; /* amount of moves that were made*/
	char msgTxt[100];
};

struct clientMsg{
	int heap;
	int amount;
	int msg; // 1 - this is a message, 0 - this is a move
	int recp; // player id to send the message to (0 - p-1)
	int moveCount; // amount of move that were made
	char msgTxt[100];
};

// client globals
int playerId, playing, isMisere, moveCount, myTurn;
struct gameData game;

int connectToServer(int sock, const char* address, char* port);
void printGameState(struct gameData game);
void printWinner(struct gameData game);
// struct gameData receiveDataFromServer(int sock);
void printValid(struct gameData game);
struct clientMsg getMoveFromInput(int sock, char* cmd);

// common
int sendAll(int s, char *buf, int *len);
int receiveAll(int s, char *buf, int *len, int first);
void checkForZeroValue(int num, int sock);
void checkForNegativeValue(int num, char* func, int sock);
int parseClientMsg(char buf[MSG_SIZE], struct clientMsg *data);
void createClientMsgBuff(struct clientMsg data, char* buf);
void createGameDataBuff(struct gameData data, char* buf);
int parseGameData(char buf[MSG_SIZE], struct gameData* data);
void updateStaticParams();
 void handleMsg(char *buf);
 void handleFirstMsg(char *buf);

void updateStaticParams(){
	playing = game.playing;
	moveCount = game.moveCount;
	myTurn = game.isMyTurn;
}

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
		//printf( "D: Error opening the socket: %s\n", strerror( errno ) );
   	   	return errno;
	}
	//printf("D: Succesfully got a socket number: %d\n", sock);

	// Connect to server
	sock = connectToServer(sock, address, port);
	//printf("D: socket number after connectToServer: %d\n", sock);
	/*char buf[MSG_SIZE];*/
	/*struct clientMsg m;*/
	// Get initial data 
	//printf("D: trying to get initial data\n");
	char readBuf[MSG_SIZE];
	int msgSize = MSG_SIZE;
	receiveAll(sock, readBuf, &msgSize,1);
	//printf("D: got initial data\n");
	//got the initial data from the server
	if (game.valid == 0)
	{
		 printf("Client rejected: too many clients are already connected\n");
		 return 0;
	}

	updateStaticParams();
	playerId = game.myPlayerId;
	isMisere = game.isMisere;

	if (isMisere == 1)
	{
		printf("This is a Misere game\n");
	}else{
		printf("This is a Regular game\n");
	}

	printf("Number of players is %d\n", game.numOfPlayers);
	printf("You are client %d\n", playerId);
	if (playing == 1)
	{
		 printf("You are playing\n");
	}else{
		 printf("You are only viewing\n");
	}

	printGameState(game);

	int addReadyForSend = 0;

	fd_set fdSetRead, fdSetWrite;
	struct clientMsg cm;
	
	if (myTurn == 1)
	{	
		printf("Your turn:\n");
	}

	while(game.win == -1){
		//printf("D: Starting loop\n");

		// clear set and add listner

		// TODO: need to add timeout

		int maxClientFd = sock;

		FD_ZERO(&fdSetRead);
		FD_ZERO(&fdSetWrite);

		// not only a viewer - adding cmd input
		FD_SET(STDIN, &fdSetRead);
		FD_SET(sock, &fdSetRead);

		if (addReadyForSend == 1)
		{
			//printf("D: adding client to write mode\n");
			FD_SET(sock, &fdSetWrite);
		}

		//printf("D: select...\n");
		int fdReady = select(maxClientFd+1, &fdSetRead, &fdSetWrite, NULL, NULL);
		//printf("D: fdready:%d\n", fdReady);
		// Print ready clients:

		if(fdReady == 0){
			//printf("D: no fd ready\n");
			continue;
		}

		if(FD_ISSET(sock , &fdSetWrite) && addReadyForSend == 1){
			// ready to send
			//printf("D: ready to send\n");
			char cmb[MSG_SIZE];
			createClientMsgBuff(cm, cmb);
			int msg = MSG_SIZE;
			sendAll(sock, cmb, &msg);
			addReadyForSend = 0;
		}

		
		
		if(FD_ISSET(STDIN , &fdSetRead)){
			// there is input from cmd
			int rSize = MSG_SIZE;
			fgets(readBuf,rSize,stdin);
			//printf("D: Read from STDIN:%s\n", readBuf);

			cm = getMoveFromInput(sock, readBuf);

			if (cm.msg == 1)
			{
				// this is message and client is not a viewer
				//printf("D: adding ready for send for client (sending msg)\n");
				addReadyForSend = 1;
			}

			if (cm.msg == 0 && playing == 1)
			{
				if (myTurn != 1)
				{
					// not my turn!
					printf("Move rejected: this is not your turn\n");
				}else{
					//printf("D: adding ready for send for client (sending move)\n");
					addReadyForSend = 1;
				}
			}
		}

		if(FD_ISSET(sock , &fdSetRead)){
			char rBuf[MSG_SIZE];
			int rSize = MSG_SIZE;
			receiveAll(sock, rBuf, &rSize,0);
		}
		//printf("D: ending loop\n");
	}

	//printf("D: Exited the loop!\n");

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

    	//printf("D: trying to connect\n");
    	if (connect(sock, p->ai_addr, p->ai_addrlen) == -1) {
      	  	//close(sock);
       	 	perror("connect");
        	continue;
    	}

    	//printf("D: Connected\n");

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

struct clientMsg getMoveFromInput(int sock, char* cmd){
	int heap, reduce;
	char heapC;
	char msg[MSG_SIZE];
	int recep;
	struct clientMsg m;
	m.moveCount = game.moveCount;
	
	// Exit if user put Q
	if (cmd[0] == 'Q')
	{
		close(sock);
		exit(0);
	}

	if (sscanf(cmd,"MSG %d %[^\n]", &recep, msg) == 2)
	{
		m.msg = 1;
		m.recp = recep;
		strcpy(m.msgTxt, msg);
		int index = strlen(msg) < 150 ? strlen(msg) : 150;
		m.msgTxt[index] = '\0';
		//printf("got msg from stdin:%s with len:%lu\n", msg, strlen(msg));
		return m;
	}

	sscanf(cmd,"%c %d", &heapC, &reduce);
	heap = (int)heapC - (int)'A';
	if (heap < 0 || heap > 3)
	{
		 printf("Illegal input!!!\n");
		 close(sock);
		 exit(1);
	}


	m.heap = heap;
	m.amount = reduce;
	m.msg = 0;

	return m;
}

// struct gameData receiveDataFromServer(int sock)
// {
// 	char buf[MSG_SIZE];
// 	struct gameData game;
// 	int rec = receiveAll(sock, buf, &MSG_SIZE);

// 	// if (rec == -1)
// 	// {
// 	// 	fprintf(stderr, "failed to receive initial data\n");
// 	// 	close(sock);
//  //    	exit(2);
// 	// }

// 	// printf("D: received data from server\n");
// 	// parseGameData(buf, &game);
// 	// printf("D: parsed data from server\n");
// 	// /*printf("Data Received from server: %s\n",buf);*/

// 	return game;
// }

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
	if (playing == 0)
	{
		printf("Game over!\n");

		return;
	}

	if ((isMisere == 1 && game.win != playerId) || (isMisere == 0 && game.win == playerId))
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

// int sendAll(int s, char *buf, int *len) {
// 	int total = 0; /* how many bytes we've sent */
// 	int bytesleft = *len; /* how many we have left to send */
//    	int n;

// 	n = send(s, buf, MSG_SIZE, 0);	
// 	total = n;
// 	// while(total < *len) {
// 	// 		//printf("D: sending data...\n");
// 	// 		n = send(s, buf+total, bytesleft, 0);
// 	// 		//printf("D: sent %d bytes\n", n);
// 	// 		checkForZeroValue(n,s);
// 	// 		if (n == -1) { break; }
// 	// 		total += n;
// 	// 		bytesleft -= n;
// 	//   	}
// 	printf("Buf len:%lu, sent:%d\n",strlen(buf),total);
// 	printf("D: data sent: %s\n",buf);
// 	*len = total; /* return number actually sent here */
	  
// 	return 0;  	
// 	//return n == -1 ? -1:0; /*-1 on failure, 0 on success */
// }

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
	  	
	//printf("Buf len:%lu, sent:%d\n",strlen(buf),total);
	//printf("D: data sent: %s\n",buf);

	return n == -1 ? -1:0; /*-1 on failure, 0 on success */
}


 // int receiveAll(int s, char *buf, int *len) {
 // 	int total = 0; /* how many bytes we've received */
 // 	size_t bytesleft = *len; /* how many we have left to receive */
 //    int n;
	
	// struct gameData gd;
	// printf("D: trying to receive\n");
	// while(total < *len) {
	// 		n = recv(s, buf+total, bytesleft, 0);
	// 		checkForZeroValue(n,s);
	// 		if (n == -1) { 
	// 			printf("D: recv failed\n");
	// 			break; 
	// 		}
	// 		total += n;
	// 		bytesleft -= n;
	// 		printf("D: Total:%d, Len:%d ---- parseGameData():%d, buf:%s\n", total,*len,parseGameData(buf, &gd), buf);
	// 		if (parseGameData(buf, &gd) == 14)
	// 		{
	// 			const char *ptr = strchr(buf, '}');
	// 			if(ptr) {
	// 				// remove the first msg
 //   					int index = ptr - buf;
 //   					strcpy(buf, ClientsQueue[index].writeBuf+index);
	// 			}
	// 			printf("read full server msg:%\n");

	// 		}
	//   	}
	//   	printf("D: received all\n");
	// *len = total; /* return number actually sent here */
	//  return n == -1 ? -1:0; /*-1 on failure, 0 on success */
 // }

 int receiveAll(int s, char *buf, int *len, int first) {
 	int total = 0; /* how many bytes we've received */
 	size_t bytesleft = *len; /* how many we have left to receive */
    int n;
	int endOfMsg = 0;
	//struct gameData gd;
	int index = 0;
	int beforeFirst = 1;
	while(endOfMsg == 0) {
			char rBuff[500];
			if (beforeFirst == 0)
			{
				n = recv(s, rBuff, 500, 0);
				//printf("D: recv came back(rBuff):%s\n",rBuff );
				strcat(buf,rBuff);
			}else{
				n = recv(s, buf, bytesleft, 0);
				//printf("D: recv came back:%s\n",buf );
				beforeFirst = 0;
			}
			//checkForZeroValue(n,s);
			if (n == -1) { break; }
			total += n;
			bytesleft -= n;
		
			const char *ptr = strchr(buf, '}');
			while(ptr) {
				// remove the first msg
   				index = ptr - buf + 1;
   				// Handle msg!!!
   				if (first == 1)
   				{
   					 endOfMsg = 1;
   					 handleFirstMsg(buf);
   					 ptr = NULL;
   					 continue;
   				}
   				char currBuf[MSG_SIZE];
   				strcpy(currBuf,buf);
   				currBuf[index] = '\0';
   				//printf("buf:%s\n",currBuf);
   				handleMsg(currBuf);
   				//printf("1finish to handle msg\n");
   				//printf("BUF:%s, index:%d, buf[index]:%c\n",buf,index,buf[index]);
   				char buf2[MSG_SIZE];
   				strcpy(buf2, buf+index);
   				//printf("copied buf2:%s\n",buf2);
   				strcpy(buf, buf2);
   				//printf("2finish to handle msg\n");
   				//printf("D: read full msg\n");
   				if(buf[0] != '{'){
   					//printf("D: set end of msg to 1\n");
   					endOfMsg = 1;
   				}
				//printf("3finish to handle msg\n");
   				const char* startPtr = strchr(buf, '{');
   				if (!startPtr)
   				{
   					buf[0] = '\0';
   					return 0;
   				}
   				ptr = strchr(buf, '}');
   				//printf("4finish to handle msg\n");
			}
			if (first == 1)
			{
				 break;
			}
	  	}
		*len = total; /* return number actually sent here */
	  	//printf("D: finish receiving all\n");
	 return n == -1 ? -1:0; /*-1 on failure, 0 on success */
 }

 void handleFirstMsg(char *buf){
 	//printf("D: handling initial msg %s\n",buf);
 	parseGameData(buf, &game);
 }

 void handleMsg(char *buf)
 {
 	//printf("D: handling msg %s\n",buf);
	int oldMoveCount = game.moveCount;
	int oldPlaying = game.playing;
	int oldMyTurn = game.isMyTurn;

	struct gameData currGame;
	assert(13 <= parseGameData(buf, &currGame));
	if (currGame.msg != 0)
	{
		char txt[MSG_SIZE];
		strncpy(txt, currGame.msgTxt, strlen(currGame.msgTxt));
		txt[strlen(currGame.msgTxt)] = '\0';
		printf("%d: %s\n",currGame.msg, txt);
		return;
	}

	// if (game.playing == 2 && game.valid == 1)
	// {
	// 	printf("D: player:%d played\n",game.myPlayerId );
		
	// 	return;
	// }
	assert(13 <= parseGameData(buf, &game));
	updateStaticParams();

	if (oldPlaying == 0 && currGame.playing == 1 && currGame.valid == 1)
	{
		 // player turn to be a player
		printf("You are now playing!\n");
		playing = 1;
		if (currGame.isMyTurn == 1)
		{
			printf("Your turn:\n");
		}

		return;
	}

	if (oldMoveCount != currGame.moveCount)
	{
		// assert(13 <= parseGameData(buf, &game));
		// updateStaticParams();

		if (oldMyTurn == 1)
		{
			if (game.valid == 1)
			{
				printf("Move accepted\n");
			}else{
				printf("Illegal move\n");
			}
		}
		
		//printf("my turn? :%d\n",currGame.isMyTurn);
		if (myTurn != 1)
		{
			printGameState(game);
		}
		
	}

	// if (oldMyTurn != currGame.isMyTurn)
	// {
	// 	assert(13 <= parseGameData(buf, &game));
	// 	updateStaticParams();
	// }

	if (myTurn == 1 && game.win == -1)
	{	
		printf("Your turn:\n");
	}


 }

 void checkForZeroValue(int num, int sock){
	if(num==0){
		printf("Disconnected from server\n");
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
	if (data.msg == 0)
	{
		data.msgTxt[0] ='a';
		data.msgTxt[1] = '\0';
	}
	sprintf(buf, "{%d$%d$%d$%d$%d$%s}",
	 data.heap,
	 data.amount,
	 data.msg,
	 data.recp,
	 data.moveCount, 
	 data.msgTxt);
}

 int parseClientMsg(char buf[MSG_SIZE], struct clientMsg *data){
	return sscanf(buf, "{%d$%d$%d$%d$%d$%[^}]",
	 &data->heap,
	 &data->amount,
	 &data->msg,
	 &data->recp,
	 &data->moveCount,
	 &data->msgTxt[0]);
}

 int parseGameData(char buf[MSG_SIZE], struct gameData* data){
	int x = sscanf( buf, "{%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%[^}]",
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

	return x;
}

void createGameDataBuff(struct gameData data, char* buf){
	if (data.msg == 0)
	{
		data.msgTxt[0] ='a';
		data.msgTxt[1] = '\0';
	}
	sprintf(buf, "{%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%s}",
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
