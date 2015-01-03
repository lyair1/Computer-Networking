 #include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> // for open flags
#include <time.h> // for time measurement
#include <assert.h>
#include <errno.h> 
#include <string.h>
#include <math.h>
#include <unistd.h>

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
int msg_SIZE = 300;

struct gameData{
	int valid; 
	int msg; // <sender Id> - this is a message, (-1) - send to all - (0) this is not a msg
	int isMyTurn; // 0 - no, 1 - yes
	int win; // 0 - no one, <player id> - the player id who won
	int numOfPlayers; // p - the number of players the server allows to play
	int myPlayerId; // player id (0 - p-1), if i dont play: -1
	int playing; // 0 - viewing, 1 - playing
	int isMisere;
	int heapA;
	int heapB;
	int heapC;
	int heapD;
	int moveCount; // amount of moves that were made
	char msgTxt[100]; // TODO: unknown error with 'MSG_SIZE - 100' while compiling
};

struct clientMsg{
	int heap;
	int amount;
	int msg; // 1 - this is a message, 0 - this is a move
	int recp; // player id to send the message to (0 - p-1)
	int moveCount; // amount of move that were made
	char msgTxt[100]; // TODO: unknown error with 'MSG_SIZE - 100' while compiling
};

struct clientData{
	int fd;			// fd that was returned from select
	int clientNum;	// client number implemented bonus style
	int isPlayer;
	char readBuf[MSG_SIZE];		// contains data read from client
	char writeBuf[MSG_SIZE]; 	// contains data to write to client
	//int amountToWrite; // indicating amount of data contains by writeBuf 
};

struct clientData ClientsQueue[10];	// queue for connected clients
int minFreeClientNum = 1;			// lowest available client number
int clientIndexTurn = 0;			// current turn of client (index according to queue)
int conPlayers = 0;					// amount of connected players
int conViewers = 0;					// amount of connected viewers
struct gameData game;				// global game struct

// game utils
int myBind(int sock, const struct sockaddr_in *myaddr, int size);
int IsBoardClear(struct gameData game);
void RemoveOnePieceFromBiggestHeap(struct gameData * game);
int MaxNum(int a, int b, int c, int d);
int CheckAndMakeClientMove(struct clientMsg clientMove);

// common
int sendAll(int s, char *buf, int *len);
void checkForZeroValue(int num, int sock);
void checkForNegativeValue(int num, char* func, int sock);
int parseClientMsg(char buf[MSG_SIZE], struct clientMsg *data);
void createClientMsgBuff(struct clientMsg data, char* buf);
void createGameDataBuff(struct gameData data, char* buf);
int parseGameData(char buf[MSG_SIZE], struct gameData* data);

void PRINT_Debug(char* msg);
void SendCantConnectToClient(int fd);
void sendInvalidMoveToPlayer(int index);
void updateEveryoneOnMove(int index);
void updateEveryoneOnMoveExceptIndex(int index);
void notifyOnTurn();
void notifyOnTurningToPlayer();

void addClientToQueue(int newFd, int isPlayer);
int delClientFromQueue(int fd);
void handleMsg(struct clientMsg ,int index);
void handleIncomingMsg(struct clientMsg data,int index);
// int sendProperDataAfterMove(struct gameData data);
void handleReadBuf(int index);
int receiveFromClient(int index);
int sendToClient(int index);

// void updateClientsOnChange(int clientNum, int action);

// *
//  send messages to all clients with proper coded data
//  clientNum - the client that was changed
//  action - type of action that was made
//  0 - deleted client
//  1 - added client

// void updateClientsOnChange(int clientNum, int action){

// }

#define DEBUG 1

void PRINT_Debug(char* msg){
	if(DEBUG){
		printf("%s\n",msg);
	}
}

int main(int argc, char** argv){
	int sockListen, errorIndicator;
	int maxClientFd;
	int i, fdready;
	struct sockaddr_in myaddr;
	struct sockaddr addrBind;
	struct in_addr inAddr;
	/*struct socklen_t *addrlen;*/
	/*char buf[MSG_SIZE];*/
	fd_set fdSetRead, fdSetWrite;

	int M, port, p;
	/*struct clientMsg clientMove;*/

	/*Region input Check*/

	if(argc<=3 || argc>=6){
		printf("Illegal arguments\n");
		exit(1);
	}

	sscanf(argv[1],"%d",&p);

	/*printf("argv[1] %s\n", argv[2]);*/
	sscanf(argv[2],"%d",&M);

	if( strcmp(argv[3],"0") ==0 ){
		game.isMisere =0;
	}
	else if(strcmp(argv[3],"1") ==0 ){
		game.isMisere=1;
	}
	else{
		printf("Illegal arguments. Misere should be 0/1\n");
		exit(1);
	}

	if(argc==5){
		sscanf(argv[4],"%d",&port);
	}
	else{
		port =6325;
	}

	game.heapA = M;
	game.heapB = M;
	game.heapC = M;
	game.heapD = M;
	game.valid=1;
	game.win = -1;
	game.numOfPlayers = p;
	game.msg = 0;
	game.moveCount = 0;

	
	/*printf("Set all arguments, start server\n");*/

	// Set listner. accepting only in main loop

	sockListen = socket(AF_INET, SOCK_STREAM, 0);
	checkForNegativeValue(sockListen, "socket", sockListen);
	/*printf("Succesfully got a socket number: %d\n", sockListen);*/
	addrBind.sa_family = AF_INET;
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons(port);
	inAddr.s_addr = htonl( INADDR_ANY );
	myaddr.sin_addr = inAddr;
	errorIndicator=myBind(sockListen, &myaddr, sizeof(addrBind));
	checkForNegativeValue(errorIndicator, "bind", sockListen);
	/*printf("Succesfully binded %d\n", sock);*/

	errorIndicator=listen(sockListen, 9);
	checkForNegativeValue(errorIndicator, "listen", sockListen);
	/*printf("Succesfully started listening: %d\n", sock);*/
	
	while(1){
		// clear set and add listner
		
		maxClientFd = sockListen;
		FD_ZERO(&fdSetRead);
		FD_ZERO(&fdSetWrite);
		FD_SET(sockListen, &fdSetRead);
		printf("listening socket is:%d\n",sockListen);

		// add all clients to fdSetRead
		printf("clients to add. players:%d, viewers:%d\n",conPlayers,conViewers);
		for(i=0 ; i< conViewers + conPlayers ; i++){
			printf("Adding fd:%d to read\n",ClientsQueue[i].fd);
			FD_SET(ClientsQueue[i].fd, &fdSetRead);
			if(strlen(ClientsQueue[i].writeBuf) > 0){
				printf("Adding fd:%d to write\n",ClientsQueue[i].fd);
				FD_SET(ClientsQueue[i].fd, &fdSetWrite);
			}
			else{
				printf("ClientsQueue[i].writeBuf = %s\n",ClientsQueue[i].writeBuf);
			}
			if(ClientsQueue[i].fd > maxClientFd) { maxClientFd = ClientsQueue[i].fd; }
		}

		// TODO: need to add timeout
		printf("Select...\n");
		fdready = select(maxClientFd+1, &fdSetRead, &fdSetWrite, NULL, NULL);
		printf("Exit select...fdReady = %d\n",fdready);

		// printf("%s...\n",strerror(errno));
		if (FD_ISSET (sockListen, &fdSetRead))
      		{
      			 printf("Reading from sock listen\n");
      			 int fdCurr = accept(sockListen, (struct sockaddr*)NULL, NULL );
      			 checkForNegativeValue(fdCurr, "accept", fdCurr);

				 if(fdCurr >= 0){
				 	printf("Got a valid FD after accept, fd:%d\n",fdCurr);
				 	if( (conViewers + conPlayers) == 9){
					// too much connected clients. sending "can't connect" to client
					SendCantConnectToClient(fdCurr);
					}
					else if(conPlayers == p){
						// max amount of players. new client is a viewer
						printf("new client is a viewer: fd:%d\n",fdCurr);
						addClientToQueue(fdCurr, 0);
					}
					else{
						// new client is a player
						printf("new client is a player: fd:%d\n",fdCurr);
						addClientToQueue(fdCurr, 1);
					}
				 }
			}

		/* Service all the sockets with input pending. */
     	for (i = 0; i < conPlayers+conViewers; ++i){
     		printf("checking: i=%d, fd=%d\n", i, ClientsQueue[i].fd);
	      	if (FD_ISSET (ClientsQueue[i].fd, &fdSetRead)){
	      		printf("sock %d is ready for read\n", ClientsQueue[i].fd);
	      		errorIndicator = receiveFromClient(i);
	      		if(errorIndicator < 0){
	 				close(ClientsQueue[i].fd);
					delClientFromQueue(ClientsQueue[i].fd);
	 			}
	 			else if(errorIndicator == 1){
	 				printf("handling received data\n"); 
	 				handleReadBuf(i);
	 			}

	      	}

	      	if (FD_ISSET (ClientsQueue[i].fd, &fdSetWrite)){
	      		printf("sock %d is ready for write\n", ClientsQueue[i].fd);
	      		errorIndicator = sendToClient(i);
	      		if(errorIndicator < 0){
					close(ClientsQueue[i].fd);
					delClientFromQueue(ClientsQueue[i].fd);
	      		}
	      	}
      }
	}
}

int sendToClient(int index){
	if(strlen(ClientsQueue[index].writeBuf) ==0){
		//nothing to send
		return 0;
	}

   	int n;
	
	//printf("sending data...:%s\n",ClientsQueue[index].writeBuf);
	n = send(ClientsQueue[index].fd, ClientsQueue[index].writeBuf, strlen(ClientsQueue[index].writeBuf), 0);
	//printf("sent %d bytes to index: %d\n", n, index);
	if(n <= 0){
		//client disconected
		return -1;
	}
	// writing succeeded. need to move move buffer head
	printf("writeBuf before moving head:%s\n", ClientsQueue[index].writeBuf);
	strcpy(ClientsQueue[index].writeBuf, ClientsQueue[index].writeBuf+n);
	printf("writeBuf after moving head:%s\n", ClientsQueue[index].writeBuf);
	return 1;
}

/***
	action for clientQueue[index]
*/
void handleReadBuf(int index){
	struct clientMsg data;
	int retVal;
	parseClientMsg(ClientsQueue[index].readBuf, &data);

	if(data.msg == 1){
		//printf("Read msg from client\n");
		handleIncomingMsg(data, index);
	}
	else{
		// client sent a move
		if(index != clientIndexTurn){
			// it is not the client turn
			//printf("Client played out of turn");
			sendInvalidMoveToPlayer(index);
			return;
		}

		retVal = CheckAndMakeClientMove(data);
		clientIndexTurn = (clientIndexTurn+1) % (conPlayers); // keep the turn moving only between connected players
		if(retVal ==-1){
			printf("invalid client move\n");
			sendInvalidMoveToPlayer(index);
			updateEveryoneOnMoveExceptIndex(index);
			notifyOnTurn();
		}
		else{
			printf("valid client move\n");
			updateEveryoneOnMove(index);
			if(retVal==0) {
				notifyOnTurn();
			}
		}
	}

	// deleting read data from readBuf

	const char *ptr = strchr(ClientsQueue[index].readBuf, ')');
	int ind = ptr - ClientsQueue[index].readBuf + 1;
	strcpy(ClientsQueue[index].readBuf, ClientsQueue[index].readBuf + ind);
}

void notifyOnTurn(){
	char buf[MSG_SIZE];
	struct gameData newGame;

	newGame.isMyTurn =1;
	newGame.valid=1;
	newGame.playing=1;
	newGame.msg = 0;
	newGame.win = game.win;
	newGame.numOfPlayers = game.numOfPlayers;
	newGame.isMisere = game.isMisere;
	newGame.heapA = game.heapA;
	newGame.heapB = game.heapB;
	newGame.heapC = game.heapC;
	newGame.heapD = game.heapD;
	game.moveCount++;
	newGame.moveCount = game.moveCount;

	createGameDataBuff(newGame, buf);
	printf("buf:%s\n",buf );
	strcat(ClientsQueue[clientIndexTurn].writeBuf, buf);
}

void notifyOnTurningToPlayer(){
	char buf[MSG_SIZE];
	int index;

	index = conPlayers;
	ClientsQueue[index].isPlayer = 1;

	game.isMyTurn = 0;
	game.valid=1;
	game.playing=1;

	createGameDataBuff(game, buf);
	strcat(ClientsQueue[index].writeBuf, buf);
}

void updateEveryoneOnMove(int index){
	int i;
	char buf[MSG_SIZE];

	game.myPlayerId = ClientsQueue[index].clientNum;
	for(i=0; i<conPlayers+conViewers; i++){
		game.playing = ClientsQueue[i].isPlayer;
		createGameDataBuff(game, buf);
		strcat(ClientsQueue[i].writeBuf, buf);
	}
}

void updateEveryoneOnMoveExceptIndex(int index){
	int i;
	char buf[MSG_SIZE];

	game.myPlayerId = ClientsQueue[index].clientNum;
	for(i=0; i<conPlayers+conViewers; i++){
		if(i==index){
			continue;
		}
		game.playing = ClientsQueue[i].isPlayer;
		createGameDataBuff(game, buf);
		strcat(ClientsQueue[i].writeBuf, buf);
	}
}
void sendInvalidMoveToPlayer(int index){
	char buf[MSG_SIZE];

	game.valid = 0;
	game.playing = ClientsQueue[index].isPlayer;

	strcpy(game.msgTxt, "");

	createGameDataBuff(game, buf);

	// restore value
	game.valid =1;

	strcat(ClientsQueue[index].writeBuf, buf);
}
/**
	handles msg written by client clientQueue[index] 
*/
void handleIncomingMsg(struct clientMsg data,int index){
	int i;
	char buf[MSG_SIZE];
	struct gameData newGame;

	newGame.valid =1;
	newGame.msg = ClientsQueue[index].clientNum;
	newGame.playing = ClientsQueue[index].isPlayer;

	strncpy(newGame.msgTxt, data.msgTxt, strlen(data.msgTxt));
	newGame.msgTxt[strlen(data.msgTxt)] = '\0';

	createGameDataBuff(newGame, buf);
	printf("gamebuf is %s\n", buf);

	if(data.recp == -1){
		// send to all
		for(i=0;i<conViewers+conPlayers; i++){
			strcat(ClientsQueue[i].writeBuf, buf);
		}
	}
	else{
		//sent to specific client. we will search for him
		for(i=0;i<conViewers+conPlayers; i++)
			if(ClientsQueue[i].clientNum == data.recp){
				strcat(ClientsQueue[i].writeBuf, buf);
			}
	}
}

/**
	handles receive from client.
	
	1 for read all data from client
	0 for read partial data
	-1 for disconnected client
*/
int receiveFromClient(int index){
	// int total = 0;  how many bytes we've received 
 	// size_t bytesleft = *len;  how many we have left to receive 
    int n;
    char temp[MSG_SIZE];
	
	// buf + strlen(buf) guaranties no override
	n = recv(ClientsQueue[index].fd, ClientsQueue[index].readBuf+strlen(ClientsQueue[index].readBuf), MSG_SIZE , 0);

	if(n <= 0){
		//client disconected
		return -1;
	}
	if(sscanf(ClientsQueue[index].readBuf,"(%s)",temp) ==1){
		printf("index:%d, num:%d, socket:%d, has full msg in his buffer:%s\n",index,ClientsQueue[index].clientNum,ClientsQueue[index].fd,ClientsQueue[index].readBuf);
		return 1;
	}
	return 0;
}

void sendClientConnected(int fd, struct gameData *data){
	struct clientData thisClientData;
	char buf[MSG_SIZE];

	// last one added	
	thisClientData = ClientsQueue[conViewers+conPlayers]; 

	data->valid = 1;
	data->msg = 0;
	data->myPlayerId = thisClientData.clientNum;
	data->playing = thisClientData.isPlayer;

	parseGameData(buf, data);
	int errorIndicator = sendAll(fd, buf, &msg_SIZE);
	checkForNegativeValue(errorIndicator, "send", fd);
	
}

void SendCantConnectToClient(int fd){
	int errorIndicator;
	char buf[MSG_SIZE];
	struct gameData newGame;

	// -1 stands for too many clients connected
	newGame.valid =0;
	createGameDataBuff(newGame, buf);

	errorIndicator = sendAll(fd, buf, &msg_SIZE);
	checkForNegativeValue(errorIndicator, "send", fd);

	close(fd);
}

/*
	returns:	-1 invalid move
				2 valid move, client lost
				1 valid move, client won,
				0 valid move, nobody won
**/
int CheckAndMakeClientMove(struct clientMsg clientMove){
	printf("checking move for heap:%d, count:%d\n",clientMove.heap, clientMove.amount);
	// move count is allways increased if a player played on his turn
	game.moveCount++;
	if(clientMove.heap<0 || clientMove.heap>3){
		return -1;
	}

	switch(clientMove.heap){
		case(0):
			if(game.heapA < clientMove.amount){
				return -1;
			}
			else{
				game.valid=1;
				game.heapA-=clientMove.amount;
			}
			break;

		case(1):
			if(game.heapB < clientMove.amount){
				return -1;
			}
			else{
				game.valid=1;
				game.heapB-=clientMove.amount;
			}
			break;

		case(2):
			if(game.heapC < clientMove.amount){
				return -1;
			}
			else{
				game.valid=1;
				game.heapC-=clientMove.amount;
			}
			break;

		case(3):
			if(game.heapD < clientMove.amount){
				return -1;
			}
			else{
				game.valid=1;
				game.heapD-=clientMove.amount;
			}
			break;

		default:
				return -1;
	}

	if(IsBoardClear(game)){
		if(game.isMisere){
			// all other clients win
			game.win=ClientsQueue[clientIndexTurn].clientNum;
			return 2;
		}
		else{
			// Client win
			game.win=ClientsQueue[clientIndexTurn].clientNum;
			return 1;
		}
	}
	return 0;
}

void checkForNegativeValue(int num, char* func, int sock){
	if(num<0){
		printf( "Error: %s. func:%s", strerror(errno),func );
		close(sock);
	}
}

int myBind(int sock, const struct sockaddr_in *myaddr, int size){
	return bind(sock, (struct sockaddr*)myaddr, sizeof(struct sockaddr_in));
}

int MaxNum(int a, int b, int c, int d){
	int biggest = a;
	if (biggest < b)
	{
		 biggest = b;
	}
	if (biggest < c)
	{
		 biggest = c;
	}
	if (biggest < d)
	{
		 biggest = d;
	}

	return biggest;
}

int IsBoardClear(struct gameData game){
	return ( 	game.heapA==0 &&
				game.heapB==0 &&
				game.heapC==0 &&
				game.heapD==0   );
}


int sendAll(int s, char *buf, int *len) {
	int total = 0; /* how many bytes we've sent */
	int bytesleft = *len; /* how many we have left to send */
   	int n;
	
	while(total < *len) {
			printf("D: sending data...\n");
			n = send(s, buf+total, bytesleft, 0);
			printf("D: sent %d bytes\n", n);
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

/**
 newFd - new client's fd returned from select
 isPlayer - 1 for player, 0 for viewer
*/
void addClientToQueue(int newFd, int isPlayer){
	struct clientData newClient;
	int newClientIndex;
	struct gameData newGame;
	int i;

	// handling queue
	newClientIndex = conPlayers+conViewers; 
	newClient.fd = newFd;
	newClient.clientNum = minFreeClientNum;
	newClient.isPlayer = isPlayer;
	ClientsQueue[newClientIndex] = newClient;

	// handling globals
	if(isPlayer){
		conPlayers++;
	}
	else{
		conViewers++;
	}

	// finding new MinFreeClientNum
	for(minFreeClientNum = 1; minFreeClientNum<100; minFreeClientNum++){
		for(i=0; i<conPlayers+conViewers; i++){
			if(minFreeClientNum == ClientsQueue[i].clientNum){
				// we found a client with the same number. need to continue to next outside iteration
				break;
			}
		}
		if(minFreeClientNum != ClientsQueue[i].clientNum){
			// kind of nasty code, but should work.
			// we are exiting main loop because we have found our number (inner loop finished)
			break;
		}
	}


	// handling writeBuf
	newGame.valid = 1;
	newGame.msg=0;
	newGame.isMyTurn = (conViewers+conPlayers == 1) ? 1 : 0; // this is new client turn only if he is the only one here
	newGame.win =-1;
	newGame.numOfPlayers = game.numOfPlayers;
	newGame.myPlayerId = newClient.clientNum;
	newGame.playing = newClient.isPlayer;
	newGame.isMisere = game.isMisere;
	newGame.heapA = game.heapA;
	newGame.heapB = game.heapB;
	newGame.heapC = game.heapC;
	newGame.heapD = game.heapD;
	newGame.moveCount = game.moveCount;

	// first write to writeBuf. no worries of ruining previous data
	createGameDataBuff(newGame, ClientsQueue[newClientIndex].writeBuf);
}

/** fd - fd of client that was disconnected
	return value - 1 deleted client is a player, 0 for viewer, 2 for need to notify on new turn
*/
int delClientFromQueue(int fd){
	int i,j;
	struct clientData delClient;

	printf("in delClientFromQueue. clientIndexTurn=%d\n",clientIndexTurn);
	/* find and copy deleted client*/
	for(i=0; i< conViewers+conPlayers; i++){
		if(ClientsQueue[i].fd == fd){
			delClient = ClientsQueue[i];
			printf("in for loop. found deleted index=%d\n",i);
			break;
		}
	}
	j=i;
	/* move clients after deleted client to the left*/
	for(; j< conViewers+conPlayers - 1; j++){
		ClientsQueue[j] = ClientsQueue[j+1];
		printf("copying to %d\n",j);
	}
	
	/* preserve global turn*/
	if(i < clientIndexTurn){
		clientIndexTurn--;
		printf("decreasing clientIndexTurn\n");
	}
	else if(i == clientIndexTurn && (ClientsQueue[i].isPlayer) ){
		notifyOnTurn();
		printf("notifyOnTurn\n");
	}

	/* update globals */
	if(delClient.clientNum < minFreeClientNum){
		minFreeClientNum = delClient.clientNum;
	}

	if(delClient.isPlayer){
		conPlayers--;
		if(conViewers>0){
			notifyOnTurningToPlayer();
			conPlayers++;
			conViewers--;
			if(conPlayers-1==clientIndexTurn){
				printf("notifyOnTurn\n");
				notifyOnTurn();
			}
		}
		return 1;
	}
	else{
		conViewers--;
		return 0;
	}
}



/**
	clientMove - struct containing msgTxt and reciver
	index - ClientsQueue index of sender
*/
void handleMsg(struct clientMsg clientMove,int index){
	struct gameData data;
	int i;
	char buf[MSG_SIZE];

	data.valid = 1;
	data.msg = index;
	strcpy(data.msgTxt, clientMove.msgTxt);
	createClientMsgBuff(clientMove, buf);

	if(clientMove.recp == -1){
		// send to all except the sender
		for (i=0; i< conPlayers + conViewers ; i++){
			if(i != index){
				sendAll(ClientsQueue[i].fd, buf, &msg_SIZE);
			}
		}
	}
	else{
		// send only to a specific client number
		for (i=0; i< conPlayers + conViewers ; i++){
			if(ClientsQueue[i].clientNum == clientMove.recp){
				sendAll(ClientsQueue[i].fd, buf, &msg_SIZE);
			}
		}
	}
}


void createClientMsgBuff(struct clientMsg data, char* buf){
	sprintf(buf, "(%d$%d$%d$%d$%d$%s)",
	 data.heap,
	 data.amount,
	 data.msg,
	 data.recp,
	 data.moveCount, 
	 data.msgTxt);
}

 int parseClientMsg(char buf[MSG_SIZE], struct clientMsg *data){
	return sscanf(buf, "(%d$%d$%d$%d$%d$%[^)]",
	 &data->heap,
	 &data->amount,
	 &data->msg,
	 &data->recp,
	 &data->moveCount,
	 &data->msgTxt[0]);
}

 int parseGameData(char buf[MSG_SIZE], struct gameData* data){
	return sscanf( buf, "(%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%[^)]",
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
	sprintf(buf, "(%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%s)",
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
