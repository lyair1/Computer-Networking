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


void checkForNegativeValue(int num, char* func, int sock);
void checkForZeroValue(int num, int sock);
int myBind(int sock, const struct sockaddr_in *myaddr, int size);
int IsBoardClear(struct gameData game);
void RemoveOnePieceFromBiggestHeap(struct gameData * game);
int MaxNum(int a, int b, int c, int d);
void CheckAndMakeClientMove(struct gameData * game, struct move clientMove);
int sendAll(int s, char *buf, int *len);
int receiveAll(int s, char *buf, int *len);

int main(int argc, char** argv){
	int sock, errorIndicator;
	struct sockaddr_in myaddr  ;
	struct sockaddr addrBind;
	struct in_addr inAddr;
	/*struct socklen_t *addrlen;*/
	char buf[MSG_SIZE];

	int M,port;
	struct gameData game;
	struct move clientMove;

/*Region input Check*/
	#if (1)
	if(argc<=2 || argc>=5){
		printf("Illegal arguments\n");
		exit(1);
	}

	/*printf("argv[1] %s\n", argv[2]);*/
	sscanf(argv[1],"%d",&M);

	game.heapA = M;
	game.heapB = M;
	game.heapC = M;
	game.heapD = M;

	if( strcmp(argv[2],"0") ==0 ){
		game.isMisere =0;
	}
	else if(strcmp(argv[2],"1") ==0 ){
		game.isMisere=1;
	}
	else{
		printf("Illegal arguments. Misere should be 0/1\n");
		exit(1);
	}

	if(argc==4){
		sscanf(argv[3],"%d",&port);
	}
	else{
		port =6325;
	}

	#endif

	/*printf("Set all arguments, start server\n");*/

	sock = socket(AF_INET, SOCK_STREAM, 0);
	checkForNegativeValue(sock, "socket", sock);
	/*printf("Succesfully got a socket number: %d\n", sock);*/


	addrBind.sa_family = AF_INET;
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons(port);
	inAddr.s_addr = htonl( INADDR_ANY );
	myaddr.sin_addr = inAddr;
	errorIndicator=myBind(sock, &myaddr, sizeof(addrBind));
	checkForNegativeValue(errorIndicator, "bind", sock);
	/*printf("Succesfully binded %d\n", sock);*/

	errorIndicator=listen(sock, 5);
	checkForNegativeValue(errorIndicator, "listen", sock);
	/*printf("Succesfully started listening: %d\n", sock);*/

	/*printf("Trying to accept\n");*/
	sock = accept(sock, (struct sockaddr*)NULL, NULL );
	checkForNegativeValue(sock, "accept", sock);
	/*printf("Accepted\n");*/

	game.valid=1;
	game.win = -1;

	sprintf(buf, "%d$%d$%d$%d$%d$%d$%d",game.valid,game.win, game.isMisere,game.heapA,game.heapB,game.heapC,game.heapD);
	while(1){
		errorIndicator = sendAll(sock, buf, &MSG_SIZE);
		checkForNegativeValue(errorIndicator, "send", sock);

		// If the game is over the server disconnect
		if (game.win != 0)
		{
			close(sock);
			exit(0);
		}

		errorIndicator = receiveAll(sock, buf, &MSG_SIZE);
		//printf("Received data: %s with indicator: %d\n",buf, errorIndicator);
		checkForNegativeValue(errorIndicator, "recv", sock);

		sscanf(buf, "%d$%d", &clientMove.heap, &clientMove.amount);

		if(clientMove.heap<0 || clientMove.heap>3){
			game.valid=0;
		}
		else{
			CheckAndMakeClientMove(&game, clientMove);
		}

		if(IsBoardClear(game)){
			// Client win
			game.win=1;
		}

		else{
			RemoveOnePieceFromBiggestHeap(&game);
			if(IsBoardClear(game)){
				// server win
				game.win=2;
			}
		}

		sprintf(buf, "%d$%d$%d$%d$%d$%d$%d",game.valid,game.win, game.isMisere,game.heapA,game.heapB,game.heapC,game.heapD);
	}

}

void CheckAndMakeClientMove(struct gameData * game, struct move clientMove){

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

void checkForZeroValue(int num, int sock){
	if(num==0){
		printf( "Disconnected from Client\n");
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
			checkForZeroValue(n, s);
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
			checkForZeroValue(n, s);
			if (n == -1) { break; }
			total += n;
			bytesleft -= n;
	  	}
	*len = total; /* return number actually sent here */
	  	
	 return n == -1 ? -1:0; /*-1 on failure, 0 on success */
 }