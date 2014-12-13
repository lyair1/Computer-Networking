 #include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> // for open flags
#include <time.h> // for time measurement
#include <assert.h>
#include <errno.h> 
#include <string.h>
#include <math.h>
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

int MSG_SIZE=50;

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
	char msgTxt[10];
};

struct clientMsg{
	int heap;
	int amount;
	int msg; // 1 - this is a message, 0 - this is a move
	int recp; // player id to send the message to (0 - p-1)
	int moveCount; // amount of move that were made
	char msgTxt[10];
};


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
struct clientMsg parseClientMsg(char buf[MSG_SIZE]);
void createClientMsgBuff(struct clientMsg data, char* buf);
void createGameDataBuff(struct gameData data, char* buf);
struct gameData parseGameData(char buf[MSG_SIZE]);
void PRINT_Debug(char* msg);
void SendCantConnectToClient(int fd);

#define DEBUG 1

void PRINT_Debug(char* msg){
	if(DEBUG){
		printf("%s\n",msg);
	}
}

int main(int argc, char** argv){
	int sockListen, errorIndicator;
	int clientNum[9], maxClientNum, conPlayers, conViewers;
	struct sockaddr_in myaddr;
	struct sockaddr addrBind;
	struct in_addr inAddr;
	/*struct socklen_t *addrlen;*/
	char buf[MSG_SIZE];
	fd_set fdSet;

	int M, port, p;
	struct gameData game;
	struct clientMsg clientMove;

	/*Region input Check*/
	if (1){
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
	}

	/*printf("Set all arguments, start server\n");*/

	// Set listner. accepting only in main loop
	if(1){
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

		errorIndicator=listen(sockListen, 5);
		checkForNegativeValue(errorIndicator, "listen", sock);
		/*printf("Succesfully started listening: %d\n", sock);*/
	}

	// initilaizing fdSet with listner only
	FD_ZERO(fdSet);
	FD_SET(sockListen, fdSet);

	while(1){
		// Waiting for 
		fdCurr = select(maxClientNum+1, fdSet, NULL, NULL, NULL);
		if(fdCurr == sockListen){
			// New Client is trying to connect

			/*printf("Trying to accept\n");*/
			fdCurr = accept(sockListen, (struct sockaddr*)NULL, NULL );
			checkForNegativeValue(fdCurr, "accept", fdCurr);
			/*printf("Accepted\n");*/
			if( (conViewers + conPlayers) == 9){
				// too much connected clients. sending "can't connect" to client
				SendCantConnectToClient(fdCurr);
			}
			FD_SET(fdCurr, fd_set);
			continue;
		}



		/*printf("trying to send all\n");*/
		errorIndicator = sendAll(sock, buf, &MSG_SIZE);
		checkForNegativeValue(errorIndicator, "send", sock);

		// If the game is over the server disconnect
		if (game.win != -1)
		{
			close(sock);
			exit(0);
		}

		errorIndicator = receiveAll(sock, buf, &MSG_SIZE);
		/*printf("Received data: %s with indicator: %d\n",buf, errorIndicator);*/
		checkForNegativeValue(errorIndicator, "recv", sock);

		sscanf(buf, "%d$%d", &clientMove.heap, &clientMove.amount);

		if(clientMove.heap<0 || clientMove.heap>3){
			game.valid=0;
		}
		else{
			CheckAndMakeClientMove(&game, clientMove);
		}

		if(IsBoardClear(game)){
			if(game.isMisere){
				// Server win
				game.win=2;
			}
			else{
				// Client win
				game.win=1;
			}

		}

		else{
			RemoveOnePieceFromBiggestHeap(&game);
			if(IsBoardClear(game)){
				if(game.isMisere){
					// Client win
					game.win=1;
				}
				else{
					// Server win
					game.win=2;
				}
			}
		}

		sprintf(buf, "%d$%d$%d$%d$%d$%d$%d",game.valid,game.win, game.isMisere,game.heapA,game.heapB,game.heapC,game.heapD);
	}

}

void SendCantConnectToClient(int fd){
	int errorIndicator;
	char buf[MSG_SIZE];
	struct gameData game;

	// -1 stands for too many clients connected
	game.valid =-1;
	createGameDataBuff(game, &buf);

	errorIndicator = sendAll(fd, buf, &MSG_SIZE);
	checkForNegativeValue(errorIndicator, "send", sock);
}

void CheckAndMakeClientMove(struct gameData * game, struct clientMsg clientMove){

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
}

void checkForNegativeValue(int num, char* func, int sock){
	if(num<0){
		printf( "Error: %s\n", strerror(errno) );
		close(sock);
		exit(1);
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

void createClientMsgBuff(struct clientMsg data, char* buf){
	sprintf(buf, "%d$%d$%d$%d$%d$%s$",
	 data.heap,
	 data.amount,
	 data.msg,
	 data.recp,
	 data.moveCount, 
	 data.msgTxt);
}

struct clientMsg parseClientMsg(char buf[MSG_SIZE]){
	struct clientMsg data;
	sscanf(buf, "%d$%d$%d$%d$%d$%s$",
	 &data.heap,
	 &data.amount,
	 &data.msg,
	 &data.recp,
	 &data.moveCount,
	 data.msgTxt);

	return data;
}

struct gameData parseGameData(char buf[MSG_SIZE]){
	struct gameData data;
	sscanf( buf, "%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%d$%s$",
	 &data.valid,
	 &data.isMyTurn,
	 &data.msg,
	 &data.win,
	 &data.numOfPlayers,
	 &data.myPlayerId, 
	 &data.playing, 
	 &data.isMisere, 
	 &data.heapA, 
	 &data.heapB, 
	 &data.heapC, 
	 &data.heapD,
	 &data.moveCount,
	 &data.msgTxt[0]);

	return data;
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
