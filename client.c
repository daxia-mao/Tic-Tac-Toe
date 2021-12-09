#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdbool.h>

#define SERVERPORTNUM 3490

#define MAXRCVLEN 4096
#define STRLEN 200

char name[STRLEN];
char peername[STRLEN];
pthread_t  tid;
int hostport;
int gameflag;

long serversocket;
long peersocket;

void *peerthread(char *port);
void *serverthread(void *arg);

/* 該playgame 為借用開源程式碼 ，未來在修正*/
void playgame(int socket, char * buffer, int playerID)
{
	int len, datasocket = socket;

	printf("\nSTARTING GAME\n");

	int i = 0;                                   /* Loop counter                         */
	int player = 0;                              /* Player number - 1 or 2               */  
	int go = 0;                                  /* Square selection number for turn     */
	int row = 0;                                 /* Row index for a square               */  
	int column = 0;                              /* Column index for a square            */
	int line = 0;                                /* Row or column index in checking loop */
	int winner = 0;                              /* The winning player                   */
	char board[3][3] = {                         /* The board                            */
					   {'1','2','3'},          /* Initial values are reference numbers */
					   {'4','5','6'},          /* used to select a vacant square for   */
					   {'7','8','9'}           /* a turn.                              */
					 };
				 

	/* The main game loop. The game continues for up to 9 turns */
	/* As long as there is no winner                            */
	for( i = (0 ); i<(9 ) && winner==0; i++)
	{
		/* Display the board */
		printf("\n\n");
		printf(" %c | %c | %c\n", board[0][0], board[0][1], board[0][2]);
		printf("---+---+---\n");
		printf(" %c | %c | %c\n", board[1][0], board[1][1], board[1][2]);
		printf("---+---+---\n");
		printf(" %c | %c | %c\n", board[2][0], board[2][1], board[2][2]);
	  
		player = i%2 + 1;                           /* Select player */

		/* Take appropriate actions based on turn. */
		do
		{
			if (player == playerID)
			{
				printf("\n%s, please enter the number of the square "
				"where you want to place your %c: ", name,(player==1)?'X':'O');
				scanf("%d", &go);
				send(datasocket, &go, sizeof(go), 0); /* Send your selection */
			}
			else 
			{
				printf("\nWaiting for %s...\n", peername);
				len = recv(datasocket, &go, MAXRCVLEN, 0); /* Receive peer's selection */
				printf("%s chose %d\n", peername, go);
			}

			row = --go/3;                                 /* Get row index of square      */
			column = go%3;                                /* Get column index of square   */
		} while(go<0 || go>9 || board[row][column]>'9');

		board[row][column] = (player == 1) ? 'X' : 'O';        /* Insert player symbol   */

		/* Check for a winning line - diagonals first */     
		if((board[0][0] == board[1][1] && board[0][0] == board[2][2]) ||
		 (board[0][2] == board[1][1] && board[0][2] == board[2][0]))
			winner = player;
		else
		/* Check rows and columns for a winning line */
		for(line = 0; line <= 2; line ++)
			if((board[line][0] == board[line][1] && board[line][0] == board[line][2])||
			 (board[0][line] == board[1][line] && board[0][line] == board[2][line]))
				winner = player;

	}
	/* Game is over so display the final board */
	printf("\n\n");
	printf(" %c | %c | %c\n", board[0][0], board[0][1], board[0][2]);
	printf("---+---+---\n");
	printf(" %c | %c | %c\n", board[1][0], board[1][1], board[1][2]);
	printf("---+---+---\n");
	printf(" %c | %c | %c\n", board[2][0], board[2][1], board[2][2]);

	/* Display result message */
	if(winner == 0)
		printf("\nHow boring, it is a draw.\n");
	else if (winner == playerID){
		printf("\nCongratulations %s, YOU ARE THE WINNER!\n", name);
	}
	else{
		printf("\n%s wins this round...\n", peername);
	}

	/* Ask to play another round */
	printf("\nPlay another round? (y/n) ");
	fgetc(stdin);
	fgets(buffer, sizeof buffer, stdin);
	buffer[strlen(buffer)-1] = '\0';
	printf("\nWating for %s to acknowledge...\n", peername);

	/* Set while loop flag to true if both players agree to play again otherwise set it to false */
	gameflag=0;
	if (strcmp(buffer, "y") == 0) gameflag = 1;
	send(datasocket, buffer, strlen(buffer), 0);
	len = recv(datasocket, buffer, MAXRCVLEN, 0);
	buffer[len] = '\0';
	if (strcmp(buffer, "y") != 0 && gameflag==1)
	{
		printf("\n%s has declined...\n", peername);
		gameflag = 0;
	}
}


void *peerthread(char *port){

    if(strcmp(port, "error") == 0){
        pthread_create(&tid, NULL, serverthread, (void *)&serverthread);
        pthread_exit(0);
    }

    printf("對手的PORT: %d\n", atoi(port));

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = INADDR_ANY;
    dest.sin_port = htons(atoi(port));

    long peersocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    if(peersocket == -1){
        close(peersocket);
        pthread_create(&tid, NULL, serverthread, (void *)&serverthread);
        pthread_exit(0);
    }

    if(connect(peersocket, (struct sockaddr *)&dest, sizeof(struct sockaddr)) == -1){
        perror("connect: ");
    }

    send(peersocket, name, strlen(name), 0);
    
    char buffer[MAXRCVLEN+1];
    int len;
    len = recv(peersocket, buffer, MAXRCVLEN, 0);
    buffer[len] = '\0';

    if(strcmp(buffer, "Y") == 0 || strcmp(buffer, "y") == 0){
        printf("系統訊息：對方接受了你的邀請\n");
        gameflag = 1;
    }
    else{
        printf("系統訊息：對方拒絕了你的邀請\n");
        gameflag = 1;

    }

    while(gameflag){
        playgame(peersocket, buffer, 1);
    }

    close(peersocket);
    pthread_create(&tid, NULL, serverthread, (void *)&serversocket);
    pthread_exit(0);

}

void *serverthread(void *arg){
    
    printf("連線至伺服器...\n");

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    int len;
    char buffer[MAXRCVLEN];


    long serversocket = *((long *) arg);

    /*----- 傳送HOST PORT給Server -----*/
    sprintf(buffer, "%d", hostport);
    len = send(serversocket, buffer, MAXRCVLEN, 0);
    buffer[len] = '\0';
    printf("SEND HOST PORT: %s\n", buffer);

    /*----- 接收歡迎界面 -----*/
    // len = recv(serversocket, buffer, MAXRCVLEN, 0);
    // buffer[len] = '\0';
    // printf("%s\n", buffer);


    printf("操作方式：\n1.login <玩家名稱>\n2.list\n3.invite <玩家名稱>\n4.logout\n");

    while(1){
        printf("> ");
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strlen(buffer)-1] = '\0';
        
        char arg1[STRLEN], arg2[STRLEN];
        int n = sscanf(buffer, "%s %s", arg1, arg2);

        if(strcmp(buffer, "list") == 0 && n == 1){
            send(serversocket, buffer, strlen(buffer), 0);
            len = recv(serversocket, buffer, MAXRCVLEN, 0);
            buffer[len] = '\0';
            printf("\n%s\n", buffer);
        }

        else if(strcmp(arg1, "login") == 0 && n == 2){    //sscanf return 匹配成功的數量
            if(name == NULL || strlen(name) < 1){
                send(serversocket, buffer, strlen(buffer), 0);
                len = recv(serversocket, buffer, MAXRCVLEN, 0);
                buffer[len] = '\0';
                printf("\n%s\n", buffer);
                strcpy(name, arg2);
            }
            else{
                printf("系統訊息：你已經使用這個名字加入了: %s\n",name);
            }
        }
        else if(strcmp(arg1, "invite") == 0 && n == 2){
            if(name == NULL || strlen(name) < 1){
                printf("系統訊息：你得先登入才能邀請別人...\n");
            }
            else{
                /* bug: 若玩家名稱為1字元的話會有奇怪的輸出 */
                strcpy(peername, arg2);
                send(serversocket, buffer, strlen(buffer), 0);
                len = recv(serversocket, buffer, strlen(buffer), 0);
                buffer[len] = '\0';
                fprintf(stderr, "len: %d\nbuffer: %s\n", len, buffer);
                pthread_create(&tid, NULL, (void *)peerthread, buffer);
                pthread_exit(0);
            }
        }
        else if(strcmp(arg1, "logout") == 0 && n == 1){
            name[0] = '\0';
            printf("系統訊息：謝謝你的遊玩，再見!\n");
            close(serversocket);
            exit(1);
        }
        else{
                printf("操作方式：\n1.login <玩家名稱>\n2.list\n3.invite <玩家名稱>\n4.logout\n");
                
            }
        }
}

int main(int argc, char *argv[]){
    char display[4096];
    char tempbuffer[128];
    /*----- 顯示歡迎界面 -----*/
    FILE *fp = fopen("text", "rw");
    while(fgets(tempbuffer, 128, fp) != 0){
        strcat(display, tempbuffer);
    }
    fclose(fp);
    printf("%s\n",display);

    if(argc != 2){
        exit(1);
    }

    hostport = atoi(argv[1]);

    /*----- Set Server Info and connection -----*/
    struct sockaddr_in dest;
    serversocket = socket(AF_INET, SOCK_STREAM, 0);         //這裡沒做錯誤檢查
    memset(&dest, 0, sizeof dest);
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = INADDR_ANY;
    dest.sin_port = htons(SERVERPORTNUM);
    
    connect(serversocket,  (struct sockaddr *)&dest, sizeof(struct sockaddr));
   
    pthread_create(&tid, NULL, serverthread, (void*) &serversocket);   


    /*----- 等待其他玩家的邀請 -----*/
    struct sockaddr_in peer;

    struct sockaddr_in host;
    socklen_t socksize = sizeof(struct sockaddr_in);
    memset(&host, 0, sizeof(host));
    host.sin_family = AF_INET;
    host.sin_addr.s_addr = INADDR_ANY;
    host.sin_port =  ntohs(hostport);
    
    int hostsocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    bind(hostsocket, (struct sockaddr *)&host, sizeof(struct sockaddr));
    listen(hostsocket, 1);


    int peersocket;
    peersocket = accept(hostsocket, (struct sockaddr *)&peer, &socksize);
    char buffer[MAXRCVLEN];
    int len;

    while(peersocket){
        pthread_cancel(tid);

        len = recv(peersocket, buffer, MAXRCVLEN, 0);
        buffer[0] = '\0';
        strcpy(peername, buffer);

        printf("系統訊息：%s 想要挑戰你。要同意請求嗎？（Y/N)", peername);

        fgets(buffer, sizeof(buffer), stdin);
        buffer[strlen(buffer)-1] = '\0';

        if(strcmp(buffer, "Y") == 0 || strcmp(buffer, "y") == 0){
            printf("系統訊息：你接受了邀請\n");
            gameflag = 1;
            send(peersocket, buffer, strlen(buffer), 0);
            buffer[0] = '\0';
        }
        else{
            printf("系統訊息：你拒絕了邀請\n");
            gameflag = 0;
            send(peersocket, buffer, strlen(buffer), 0);
            buffer[0] = '\0';
        }

       while(gameflag){
			playgame(peersocket, buffer, 2); 
		}
        peername[0] = '\0';
        pthread_create(&tid, NULL, serverthread, (void *)&serversocket);
        peersocket = accept(hostsocket, (struct sockaddr *)&peer, &socksize);

    }

    return EXIT_SUCCESS;
}