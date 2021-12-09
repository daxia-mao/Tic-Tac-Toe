#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PROTOPORT 3490
#define QLEN 10
#define MAXRCVLEN 4096
#define STRLEN 200

char display[4096];

/*----- 定義玩家資訊 -----*/
struct playerInfo{
    char name[STRLEN];
    char ip[INET6_ADDRSTRLEN];
    int state;
    int port;
};

/*----- Global Variable -----*/
struct playerInfo *playerList;
int playerList_size = 0;
int playerList_online_size = 0;
pthread_mutex_t mut;
/*---------------------------*/

/*----- 有關玩家清單的函數 -----*/

/*----- 查詢玩家清單，如果玩家的名字在清單中（不管上線與否），回傳其index -----*/
int playerList_get(struct playerInfo *list, char *name){
    if(playerList_size < 1){
        return -1;
    }

    int i;
    for(i=0; i<playerList_size; i++){
        // 如果玩家在表中存在，回傳其index。
        if(strcmp(list[i].name , name) == 0){
            return i;
        }
    }

    return -1;
}

/*----- 若不再清單內，則加入玩家清單，若下線，則上線。 -----*/
int playerList_set(struct playerInfo *list, char *name, char *ip, int port){
    
    if(playerList_size >= 100){
        return -1;
    }
    
    int i = playerList_get(list, name);
    // 若玩家清單內沒有這個人的話，將其加入
    if(i == -1){
        strcpy(list[playerList_size].name, name);
        strcpy(list[playerList_size].ip, ip);
        list[playerList_size].port = port;
        list[playerList_size].state = 1;

        playerList_online_size++;
        playerList_size++;
        return 1;
    }
    // 若玩家清單內有這個人，但狀態是下線的話，將其上線
    else if(i != -1 && list[i].state == 0){
        playerList_online_size++;
        list[i].state = 1;
        strcpy(list[i].ip, ip);
        list[i].port = port;
        return 1;
    }
    
    return -1;
}

/*----- 將玩家清單打印至Buufer之中 -----*/
int playerList_print(struct playerInfo *list, char *buf){
    printf("---------- PLAYER LIST ----------\n");

    printf("Number of online player: %d\n\n", playerList_online_size);
    char tempbuffer[1024];
    
    buf[0] = '\0';

    int i;
    for(i=0; i<playerList_size; i++){
        if(list[i].state == 1){
            sprintf(tempbuffer,"Player: # %s # is online , Port:%d \n", list[i].name, list[i].port);
            strcat(buf, tempbuffer);
        }
        else{
            sprintf(tempbuffer,"Player: # %s # is offline , Port:%d \n", list[i].name, list[i].port);
            strcat(buf, tempbuffer);
        }
    }
    printf("\n");
}

/*----- 如果該玩家在清單內，且是上線狀態的話，將其標記為下線 -----*/
int playerList_remove(struct playerInfo *list, char *name){
    int i = playerList_get(list, name);
    if(i != -1 && list[i].state == 1){
        playerList_online_size--;
        list[i].state = 0;
        return 1;
    }

    return -1;
}

/*---------------------------*/

int sendall(int s, char *buf, size_t len)
{
  int total = 0; // 我們已經送出多少 bytes 的資料
  int bytesleft = len; // 我們還有多少資料要送
  int n;

  while(total < len) {
    n = send(s, buf+total, bytesleft, 0);
    if (n == -1) { break; }
    total += n;
    bytesleft -= n;
  }

  len = total; // 傳回實際上送出的資料量

  return n==-1?-1:0; // 失敗時傳回 -1、成功時傳回 0
}
void *server_Thread(void *arg){
    
    /*------ 接收來自Client的訊息------*/
    int len;
    char buf[MAXRCVLEN+1];      //Socket send及recv用的buffer
    char name[STRLEN+1];        //儲存玩家的名字

    /*----- 從pthread取得傳來的參數 -----*/
    long peer_sfd = (long)arg;

    struct sockaddr_in peeraddr;
    socklen_t peeraddrlen = sizeof(peeraddr);

    /*----- 通過getpeername取得Client的IP位址，來存至ip[]中 -----*/
    char ip[INET_ADDRSTRLEN];
    getpeername(peer_sfd, (struct sockaddr *)&peeraddr, &peeraddrlen);
    inet_ntop(AF_INET, &(peeraddr.sin_addr), ip, INET_ADDRSTRLEN);

    recv(peer_sfd, buf, MAXRCVLEN, 0);
    int peerport = atoi(buf);
    printf("RECV PEER PORT: %d\n", peerport);

    /*------ 發送歡迎界面 -----*/
    // send(peer_sfd, display, strlen(display), 0);

    while(len = recv(peer_sfd, buf, MAXRCVLEN, 0)){

        /*----- 解析從 Client 收到的訊息，分為兩個子字串（arg1,arg2）-----*/
        char arg1[STRLEN], arg2[STRLEN];
        buf[len] = '\0';
        int n = sscanf(buf, "%s %s", arg1, arg2);

        /*----- 處理 LOGIN 命令 -----*/
        if(strcmp(arg1, "login") == 0 && arg2!=NULL){
            pthread_mutex_lock(&mut);

            /*----- 若該玩家名字未在清單中，則加入至清單，否則不加入 -----*/
            if(playerList_set(playerList, arg2, ip, peerport) != -1){
                strcpy(name, arg2);
                sprintf(buf, "系統訊息：玩家 %s 加入了伺服器\n", arg2);
                printf("系統訊息：玩家 %s 加入了伺服器\n", arg2);
            }

            pthread_mutex_unlock(&mut);

            /*----- 回傳Client端的加入是否成功（僅作顯示提示用） -----*/
            send(peer_sfd, buf, strlen(buf), 0);
        }

        /*----- 處理 LIST 命令 -----*/
        else if(strcmp(arg1, "list") == 0){
            pthread_mutex_lock(&mut);

            /*----- 如果玩家清單有一個以上的話，將所有玩家名字放入Buffer當中 -----*/
            if(playerList_size > 0){
                playerList_print(playerList, buf);
                printf("@SERVER:\n%s\n", buf);
            }
            else{
                sprintf(buf, "系統訊息：現在沒有玩家在線上哦~\n");
            }

            pthread_mutex_unlock(&mut);

            /*----- 回傳Client端的請求，若成功的話回傳所有玩家名字，否則回傳錯誤訊息 -----*/
            send(peer_sfd, buf, strlen(buf), 0);
        }
        else if(strcmp(arg1, "invite") == 0 && arg2 != NULL){
            pthread_mutex_lock(&mut);
            
            int i = playerList_get(playerList, arg2);

            if(i != -1 && playerList[i].state == 1){
                sprintf(buf, "%d", playerList[i].port);
            }
            else{
                sprintf(buf, "error");
            }

            pthread_mutex_unlock(&mut);

            send(peer_sfd, buf, strlen(buf), 0);

        }
    }
    
    /*----- 玩家中斷和伺服器的連線，因為recv回傳0 -----*/
    pthread_mutex_lock(&mut);

    if(name != NULL && strlen(name) > 1){
        playerList_remove(playerList, name);
    }

    pthread_mutex_unlock(&mut);
    close(peer_sfd);
    pthread_exit(0);
}

int main(int argc, char *argv[]){
    char tempbuffer[128];
    
    /*----- 顯示歡迎界面 -----*/
    FILE *fp = fopen("text", "rw");
    while(fgets(tempbuffer, 128, fp) != 0){
        strcat(display, tempbuffer);
    }
    fclose(fp);
    printf("%s\n",display);

    /*----- 初始化玩家清單 -----*/
    playerList = (struct playerInfo *)malloc(sizeof(struct playerInfo) * 100);
    
    pthread_mutex_init(&mut, NULL);

    /*----- 設定伺服器資訊，以便後續Socket API使用-----*/
    struct sockaddr_in server_Info;

    memset((char *)&server_Info, 0, sizeof(server_Info));
    server_Info.sin_family = AF_INET;
    server_Info.sin_addr.s_addr = INADDR_ANY;

    /*----- 設定PORT號碼 -----*/
    int port;

    if(argc > 1){
        port = atoi(argv[1]);
    }
    else{
        port = PROTOPORT;
    }

    if(port > 0){
        server_Info.sin_port = htons((u_short)port);
    }
    else{
        fprintf(stderr, "錯誤的PORT號碼 %s\n",argv[1]);
        exit(1);
    }
    
    /*----- getprotobyname() , 取得TCP協議的編號來作為socket()的參數 -----*/
    struct protoent *ptrp;
    if((ptrp = getprotobyname("tcp")) == 0){
        fprintf(stderr, "cannot map 'tcp' to protocol number");
        exit(1);
    }

    /*----- socket() , 取得伺服器的Socket檔案描述符sd -----*/
    int server_sfd;
    server_sfd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
    if(server_sfd < 0){
        fprintf(stderr, "socket creation failed\n");
        exit(1);
    }
    
    int yes = 0;
    /*----- reuse port number -----*/
    if(setsockopt(server_sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
        perror("setsockopt");
        exit(1);
    }

    /*----- bind() -----*/
    if(bind(server_sfd, (struct sockaddr *)&server_Info, sizeof server_Info) < 0){
        fprintf(stderr, "bind failed\n");
        exit(1);
        
    }

    /*----- listen() -----*/
    if(listen(server_sfd, QLEN) < 0){
        fprintf(stderr, "listen failed\n");
        exit(1);
    }

    /*----- accept() -----*/
        
    struct sockaddr_in connect_Info;
    int connect_Info_size = sizeof(connect_Info);
    long connect_sfd;

    pthread_t tid;

    fprintf(stderr, "Server up and runnig.\n");

    while(1){
        if( (connect_sfd = accept(server_sfd, (struct sockaddr *)&connect_Info, &connect_Info_size)) < 0){
            send(connect_sfd, display, strlen(display), 0);
            fprintf(stderr, "accept failed\n");
            exit(1);
        }
        pthread_create(&tid, NULL, server_Thread, (void *)connect_sfd);
    }

    close(server_sfd);

}