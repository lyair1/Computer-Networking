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
	int numOfPlayers; // p - then number of players the server allows to play
	int myPlayerId; // player id (0 - p-1), if i dont play: -1
	int playing; // 0 - viewing, 1 - playing
	int isMisere;
	int heapA;
	int heapB;
	int heapC;
	int heapD;
	int moveCount; // amount of moves that were made
	char msgTxt[MSG_SIZE - 100]; // TODO: unknown error with 'MSG_SIZE - 100' while compiling
};

struct clientMsg{
	int heap;
	int amount;
	int msg; // 1 - this is a message, 0 - this is a move
	int recp; // player id to send the message to (0 - p-1)
	int moveCount; // amount of move that were made
	char msgTxt[MSG_SIZE - 100]; // TODO: unknown error with 'MSG_SIZE - 100' while compiling
};

struct clientData{
	int fd;			// fd that was returned from select
	int clientNum;	// client number implemented bonus style
	int isPlayer;
};

struct clientData ClientsQueue[10];	// queue for connected clients
int minFreeClientNum = 1;			// lowest available client number
int clientIndexTurn = 0;			// current turn of client (index according to queue)
int conPlayers = 0;					// amount of connected players
int conViewers = 0;					// amount of connected viewers

int myBind(int sock, const struct sockaddr_in *myaddr, int size);
int IsBoardClear(struct gameData game);
void RemoveOnePieceFromBiggestHeap(struct gameData * game);
int MaxNum(int a, int b, int c, int d);
void CheckAndMakeClientMove(struct gameData * game, struct clientMsg clientMove);

// common
int sendAll(int s, char *buf, int *len);
int receiveAll(int s, char *buf, int *len);
void checkForZeroValue(int num, int sock);
void checkForNegativeValue(int num, char* func, int sock);
int parseClientMsg(char buf[MSG_SIZE], struct clientMsg *data);
void createClientMsgBuff(struct clientMsg data, char* buf);
void createGameDataBuff(struct gameData data, char* buf);
int parseGameData(char buf[MSG_SIZE], struct gameData* data);

void PRINT_Debug(char* msg);
void SendCantConnectToClient(int fd);

void addClientToQueue(int newFd, int isPlayer);
int delClientFromQueue(int fd);
void handleMsg(struct clientMsg ,int index);
int sendProperDataAfterMove(struct gameData data);
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
	int fdCurr, fdReady, maxClientFd;
	int i;
	struct sockaddr_in myaddr;
	struct sockaddr addrBind;
	struct in_addr inAddr;
	/*struct socklen_t *addrlen;*/
	char buf[MSG_SIZE];
	fd_set fdSetRead, fdSetWrite;

	int M, port, p;
	struct gameData game;
	struct clientMsg clientMove;

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
		for(i=0 ; i< conViewers + conPlayers ; i++){
			printf("clients to add\n");
			FD_SET(ClientsQueue[i].fd, &fdSetRead);
			FD_SET(ClientsQueue[i].fd, &fdSetWrite);
			if(ClientsQueue[i].fd > maxClientFd) { maxClientFd = ClientsQueue[i].fd; }
		}

		// TODO: need to add timeout
		printf("Select...\n");
		fdReady = select(maxClientFd+1, &fdSetRead, &fdSetWrite, NULL, NULL);
		printf("Exit select...\n");

		/* Service all the sockets with input pending. */
      for (i = 0; i < FD_SETSIZE; ++i){
      	if (FD_ISSET (i, &fdSetRead)){
      		printf("sock %d is ready for read\n", i);

      		if (sockListen == i)
      		{
      			 printf("Reading from sock listen\n");
      			 int currFd = accept(sockListen, (struct sockaddr*)NULL, NULL );

      			 checkForNegativeValue(fdCurr, "accept", fdCurr);
				 if(fdCurr >= 0){
				 	printf("Got a valid FD after accept, fd:%d\n",fdCurr);
				 	if( (conViewers + conPlayers) == 9){
					// too much connected clients. sending "can't connect" to client
					SendCantConnectToClient(fdCurr);
					}
					else if(conPlayers == p){
						// max amount of players. new client is a viewer
						addClientToQueue(fdCurr, 0);
						// TODO: tell all clients
					}
					else{
						// new client is a player
						printf("new client is a player\n");
						addClientToQueue(fdCurr, 1);
						char currBuf[MSG_SIZE];
						createGameDataBuff(game, currBuf);
						printf("trying to send buf:%s\n",currBuf);
						assert(sendAll(fdCurr, currBuf, &msg_SIZE) == -1);
						printf("sent all\n");	
						sleep(2);
						exit(0);
						// TODO: tell all clients
					}
				 }
			}

      		
      	}

      	if (FD_ISSET (i, &fdSetWrite)){
      		printf("sock %d is write for read\n", i);
      	}
      }
        



		// if (fdReady < 0)
		// {
		// 	printf("fdready < 0\n");
		// }
		// if(fdReady == 0){
		// 	printf("fdready == 0\n");
		// 	exit(0);
		// 	continue;
		// }

		// // handling messages first
		// for(i=0 ; i< conViewers + conPlayers ; i++){
		// 	printf("in loop\n");
		// 	if(FD_ISSET(ClientsQueue[i].fd , &fdSetRead)){
		// 		printf("ready for read msg\n");
		// 		exit(0);
		// 		if((i != clientIndexTurn) && (ClientsQueue[i].fd != sockListen)){
		// 			// Client is ready to send data.
		// 			// it is not the client turn & it's not the listner
		// 			// hence, only message is legal
		// 			errorIndicator = receiveAll(ClientsQueue[i].fd, buf, &msg_SIZE);
		// 			if(errorIndicator < 0){
		// 				close(ClientsQueue[i].fd);
		// 				delClientFromQueue(ClientsQueue[i].fd);
		// 				//updateClientsOnChange(ClientsQueue[i].clientNum, 0);
		// 			}
		// 			else{
		// 				parseClientMsg(buf, &clientMove);
		// 				handleMsg(clientMove, i);
		// 			}
		// 		}
		// 	}
		// }

		// // handling move by client
		// if(FD_ISSET(ClientsQueue[clientIndexTurn].fd , &fdSetRead)){
		// 	printf("ready for read move \n");
		// 	exit(0);
		// 	// Client with turn is ready to send data.
		// 	// TODO: what if he sends a message? 
		// 	errorIndicator = receiveAll(ClientsQueue[clientIndexTurn].fd, buf, &msg_SIZE);
		// 	if(errorIndicator < 0){
		// 		close(ClientsQueue[i].fd);
		// 		delClientFromQueue(ClientsQueue[i].fd);
		// 	}
		// 	else{
		// 		parseClientMsg(buf, &clientMove);
		// 		CheckAndMakeClientMove(&game, clientMove);
		// 		if(sendProperDataAfterMove(game)){
		// 			//game over
		// 			exit(1);
		// 		}
		// 	}
		// }


	}
}

void SendCantConnectToClient(int fd){
	int errorIndicator;
	char buf[MSG_SIZE];
	struct gameData game;

	// -1 stands for too many clients connected
	game.valid =-1;
	createGameDataBuff(game, buf);

	errorIndicator = sendAll(fd, buf, &msg_SIZE);
	checkForNegativeValue(errorIndicator, "send", fd);

	close(fd);
}

void CheckAndMakeClientMove(struct gameData * game, struct clientMsg clientMove){

	if(clientMove.heap<0 || clientMove.heap>3){
		game->valid=0;
		return;
	}

	switch(clientMove.heap){
		case(0):
			if(game->heapA < clientMove.amount){
				game->valid=0;
			}
			else{
				game->valid=1;
				game->heapA-=clientMove.amount;
			}
			break;

		case(1):
			if(game->heapB < clientMove.amount){
				game->valid=0;
			}
			else{
				game->valid=1;
				game->heapB-=clientMove.amount;
			}
			break;

		case(2):
			if(game->heapC < clientMove.amount){
				game->valid=0;
			}
			else{
				game->valid=1;
				game->heapC-=clientMove.amount;
			}
			break;

		case(3):
			if(game->heapD < clientMove.amount){
				game->valid=0;
			}
			else{
				game->valid=1;
				game->heapD-=clientMove.amount;
			}
			break;

		default:
			game->valid=0;
	}

	if(IsBoardClear(*game)){
		if(game->isMisere){
			// all other clients win
			game->win=2;
		}
		else{
			// Client win
			game->win=1;
		}
	}
}

void checkForNegativeValue(int num, char* func, int sock){
	if(num<0){
		printf( "Error: %s\n", strerror(errno) );
		close(sock);
	}
}

int myBind(int sock, const struct sockaddr_in *myaddr, int size){
	return bind(sock, (struct sockaddr*)myaddr, sizeof(struct sockaddr_in));
}

void RemoveOnePieceFromBiggestHeap(struct gameData* game){
	int maxHeap;
	maxHeap = MaxNum(game->heapA, game->heapB,game->heapC ,game->heapD);

	if(maxHeap == game->heapD){
		game->heapD-=1;
		return;
	}
	if(maxHeap == game->heapC){
		game->heapC-=1;
		return;
	}
	if(maxHeap == game->heapB){
		game->heapB-=1;
		return;
	}
	if(maxHeap == game->heapA){
		game->heapA-=1;
		return;
	}
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
			printf("sending data...\n");
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

/**
 newFd - new client's fd returned from select
 isPlayer - 1 for player, 0 for viewer
*/
void addClientToQueue(int newFd, int isPlayer){
	struct clientData newClient;

	newClient.fd = newFd;
	newClient.clientNum = minFreeClientNum;
	newClient.isPlayer = isPlayer;

	minFreeClientNum++;
	
	if(isPlayer){
		conPlayers++;
	}
	else{
		conViewers++;
	}

	ClientsQueue[conPlayers+conViewers] = newClient;
}

/** fd - fd of client that was disconnected
	return value - 1 deleted client is a player, 0 for viewer
*/
int delClientFromQueue(int fd){
	int i;
	struct clientData delClient;

	/* find and copy deleted client*/
	for(i=0; i< conViewers+conPlayers; i++){
		if(ClientsQueue[i].fd == fd){
			delClient = ClientsQueue[i];
			break;
		}
	}

	/* preserve global turn*/
	if(i < clientIndexTurn){
		clientIndexTurn--;
	}

	/* move clients after deleted client to the left*/
	for(; i< conViewers+conPlayers - 1; i++){
		ClientsQueue[i] = ClientsQueue[i+1];
	}
	
	/* update globals */
	if(delClient.clientNum < minFreeClientNum){
		minFreeClientNum = delClient.clientNum;
	}
	if(delClient.isPlayer){
		conPlayers--;
		return 1;
	}
	else{
		conViewers--;
		return 0;
	}
}


/**
 	data - containing all off the game data
 	return value - 1 if gameOver, 0 o.w
*/
int sendProperDataAfterMove(struct gameData data){
	char buf[MSG_SIZE], bufToFollowingPlayer[MSG_SIZE];
	int i, errorIndicator;
	createGameDataBuff(data, buf);

	data.isMyTurn = 1;
	data.win = 0;
	createGameDataBuff(data, bufToFollowingPlayer);

	if(IsBoardClear(data)){
		// indicating that game is over
		data.win = clientIndexTurn;
		createGameDataBuff(data, buf);

		for(i=0 ; i < conPlayers + conViewers ; i++){
			errorIndicator = sendAll(ClientsQueue[i].fd, buf, &msg_SIZE);
		}
		return 1;
	}
	else{
		// advance turn 
		clientIndexTurn = (clientIndexTurn + 1) % (conViewers + conPlayers);

		if(data.valid){
			// sending valid move to all players besides the next one to play
			for(i=0 ; i < conPlayers + conViewers ; i++){
				if(i != clientIndexTurn){
					errorIndicator = sendAll(ClientsQueue[i].fd, buf, &msg_SIZE);
					if(errorIndicator<0){
						close(ClientsQueue[i].fd);
						delClientFromQueue(ClientsQueue[i].fd);
					}
				}
			}

			// sending valid move and YOUR_TURN to next client to play
			errorIndicator = sendAll(ClientsQueue[clientIndexTurn].fd, bufToFollowingPlayer, &msg_SIZE);
			if(errorIndicator<0){
				close(ClientsQueue[clientIndexTurn].fd);
				delClientFromQueue(ClientsQueue[clientIndexTurn].fd);
			}
		}

		else{
			// sending invalid move only to the player which made it
			errorIndicator = sendAll(ClientsQueue[clientIndexTurn].fd, buf, &msg_SIZE);
			if(errorIndicator<0){
				close(ClientsQueue[clientIndexTurn].fd);
				delClientFromQueue(ClientsQueue[clientIndexTurn].fd);
			}

			// advance turn 
			clientIndexTurn = (clientIndexTurn + 1) % (conViewers + conPlayers);
		}
	}
	return 0;
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
