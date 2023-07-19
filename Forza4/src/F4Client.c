#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#include "../inc/shared_memory.h"
#include "../inc/semaphore.h"
#include "../inc/errExit.h"   

/*DIMENSIONE SEMAFORI*/
#define SEM_DIM 10

#define EMPTY 0 
//EMPTY semaforo usato in modo che il server aspetta
//che venga riempito clientPid
#define MUTEX 1
//MUTEX viene usato in modo che un solo processo client alla volta
//possa entrare dentro l'area condivisa e riempire il clientPid
//inizializzato a 1 cosi il primo client che entra lo setta
#define WAIT_SECOND 2
#define MUTEX2 3
#define ASSEST 4
#define BC 5
#define BS 6
#define BC1 7
#define BC2 8
#define START 9


void printMatrix(struct Matrix *matrix);
int isValid(int col,struct Matrix *matrix);
void closeFunction();
int currentRow(int col,struct Matrix *matrix);
bool isDigitNumber(char value[9]);

void sigHandlerWinner(int sig);
void sigHandlerLoser(int sig);
void sigHandlerWithdraw(int sig);
void sigHandlerDraw(int sig);
void sigHandlerShellClosed(int sig);


/*VARIABILI GLOBALI*/
int clientNumber=0;
int semid;
bool onlyOne=false;

int shmidClient=-1;
int shmidInfo=-1;
int shmidShape=-1;
int shmidMatrix=-1;
int shmidTurn=-1;
int shmidHowMany=-1;
int shmidTimeRunOut=-1;
int shmidIsRetired=-1;
int shmidM=-1;

struct setClient *client;
struct Information *cinfo;
struct Shape *shape;
struct Matrix *matrix;
int *turn;
int *howMany;
int *timeRunOut;
int *isRetired;
char *M;

int main(int argc,char *argv[]){
    if(argc!=2 &&argc!=3){
        printf("Uso: %s nome_player (opzionale)'*'\n", argv[0]);
        exit(1);
    }
    //creo chiave per set di semafori
    key_t semKey=ftok("src/F4Server.c",20);
    if(semKey==-1){
        closeFunction();
        errExit("ftok fallita");
    }
    //prendo il set di semafori creato dal server
    semid=semget(semKey, SEM_DIM, S_IRUSR | S_IWUSR);
    if(semid==-1){
        closeFunction();
        errExit("semget fallita");
    }
    /*BLOCCO I CLIENT FINCHÈ IL SERVER NON CREA TUTTE LE MEMORIE CONDIVISE*/
    semOp(semid,START,-1);
    //libero i client a cascata
    semOp(semid,START,1);
    //creo chiave per supporto client
    //per capire a chi assegnare il ruolo di primo giocatore uso un supporto tramite shm
    //il primo client che inserisce i propri dati (pid nome e se richiede bot) nel supporto viene considerato primo giocatore
    key_t keyClient = ftok("src/F4Server.c",1);
    if(keyClient==-1){
        closeFunction();
        errExit("ftok fallita");
    }
    shmidClient=alloc_shared_memory(keyClient,sizeof(struct setClient));
    client =(struct setClient *)get_shared_memory(shmidClient,0);
    //creo chiave per info generali
    //nelle info verranno copiati i pid e i nomi client inseriti nel supporto
    //e inoltre verrò messo anche il pid del server
    //nel corso del mio programma usero questa shm per mandare segnali tra giocatori e server
    key_t keyInfo = ftok("src/F4Server.c",2);
    if(keyInfo==-1){
        closeFunction();
        errExit("ftok fallita");
    }
    shmidInfo=alloc_shared_memory(keyInfo,sizeof(struct Information));
    cinfo=(struct Information *)get_shared_memory(shmidInfo,0);
    //creo chiave per le shape condivise
    //nelle shape viene salvata la forma del primo e secondo client 
    key_t keyShape = ftok("src/F4Server.c",3);
    if(keyShape==-1){
        closeFunction();
        errExit("ftok fallita");
    }
    shmidShape=alloc_shared_memory(keyShape,sizeof(struct Shape));
    shape= (struct Shape *)get_shared_memory(shmidShape,0);

    //alloco e prendo memoria condivisa per la matrice di gioco
    key_t keyMatrix = ftok("src/F4Server.c",4);
    if(keyMatrix==-1){
        closeFunction();
        errExit("ftok fallita");
    }
    shmidMatrix=alloc_shared_memory(keyMatrix, sizeof(struct Matrix));
    matrix= (struct Matrix *)get_shared_memory(shmidMatrix,0); 
    //creo chiave per il turno condiviso
    //questa variabile serve al server 
    //cosi quando fa i controlli capisce di chi era il turno e chi ha vinto o perso
    key_t keyTurn= ftok("src/F4Server.c",5);
    if(semKey==-1){
        closeFunction();
        errExit("ftok fallita");
    }
    shmidTurn=alloc_shared_memory(keyTurn,sizeof(int));
    turn=(int *)get_shared_memory(shmidTurn,0);
    //inizializzato a 0 perchè parte il primo client
    *turn=0;
    //creo chiave per capire quanti client ci sono
    key_t keyHowMany = ftok("src/F4Server.c",6);
    if(keyHowMany==-1){
        closeFunction();
        errExit("ftok fallita");
    }
        
    shmidHowMany=alloc_shared_memory(keyHowMany,sizeof(int));
    howMany=(int *)get_shared_memory(shmidHowMany,0);
    //creo chiave per sapere se il tempo del player che sta giocando è sacduto
    key_t keyTimeRunOut = ftok("src/F4Server.c",7);
    if(keyTimeRunOut==-1){
        closeFunction();
        errExit("ftok fallita");
    }
    shmidTimeRunOut=alloc_shared_memory(keyTimeRunOut,sizeof(int));
    timeRunOut=(int *)get_shared_memory(shmidTimeRunOut,0);
    
    key_t keyIsRetired = ftok("src/F4Server.c",8);
    if(keyIsRetired==-1){
        closeFunction();
        errExit("ftok fallito");
    }
    shmidIsRetired=alloc_shared_memory(keyIsRetired,sizeof(int));
    isRetired=(int *)get_shared_memory(shmidIsRetired,0);
    
    key_t keyM=ftok("src/F4Server.c",9);
    if(keyM==-1){
        closeFunction();
        errExit("ftok fallito");
    }
    shmidM=alloc_shared_memory(keyM,sizeof(char)*matrix->max_cols*matrix->max_rows);
    M=(char *)get_shared_memory(shmidM,0);

    semOp(semid,MUTEX2,-1);
    (*howMany)++;
    semOp(semid,MUTEX2,1);
    //i processi che rendono howmany maggiori di 2 li faccio uscire
    if(*howMany>2){
        printf("Due giocatori stanno già giocando, torna più tardi quando finiscono\n");
        closeFunction();
        exit(0);
    }
    //se howMany sta a 2 ci sono due client e il primo client avrà gia  settato
    //se vuole il bot o meno e controllo, se voleva il bot allora server un client solo
    //e esce
    if(*howMany==2 && client->wantBot==true){
        printf("Un giocatore sta già giocando contro il bot, aspetta che finisca\n");
        closeFunction();
        exit(0);
    }
    //se argc sta a 3 ed è arrivato fino a questo punto del codice senza uscire prima
    //significa che un client sta aspettando un altro giocatore e questo client qua vuole giocare contro il bot
    //quindi va fatto uscire
    //inoltre decremento altrimenti il prossimo client si ritrova con howMany a 3 e non riesce a giocare
    if(*howMany==2 && argc==3){
        semOp(semid,MUTEX2,-1);
        (*howMany)--;
        semOp(semid,MUTEX2,1);
        printf("Non puoi giocare contro il bot, un player sta già aspettando un altro player con cui giocare\nSe vuoi puoi giocare come player\n");
        closeFunction();
        exit(0);
    }
    //Dichiarazione di altre variabili, valid usata per capire se una mossa è valida
    int valid=1;
    int col;
    int row;
    char value[9];
    //Non gestisco subito il segnale ctrl c perchè non ha senso
    sigset_t mySet;
    sigfillset(&mySet);
    //vittoria
    sigdelset(&mySet,SIGUSR1);
    //sconfitta
    sigdelset(&mySet,SIGUSR2);
    //pareggio o chiusura processo server
    sigdelset(&mySet,SIGQUIT);
    //chiusura terminale (ritiro)
    sigdelset(&mySet,SIGHUP);
    //control c (ritiro)
    sigdelset(&mySet,SIGINT);
    //maschera di segnali
    if(sigprocmask(SIG_SETMASK, &mySet, NULL)==-1){
        closeFunction();
        errExit("sigprocmask fallita\n");
    }
    //riceve sigusr1 da server
    if(signal(SIGUSR1, sigHandlerWinner)==SIG_ERR){
        closeFunction();
        errExit("cambio signal handler fallito\n");
    }
    //riceve sigusr2 da server
    if(signal(SIGUSR2, sigHandlerLoser)==SIG_ERR){
        closeFunction();
        errExit("cambio signal handler fallito\n");
    }
    //riceve sigquit da server
    if(signal(SIGQUIT,sigHandlerDraw)==SIG_ERR){
        closeFunction();
        errExit("cambio signal handler fallito\n");
    }
    //genera sighupMy
    if(signal(SIGHUP,sigHandlerWithdraw)==SIG_ERR){
        closeFunction();
        errExit("cambio signal handler fallito\n");
    }
    //genera sigint
    if(signal(SIGINT, sigHandlerWithdraw)==SIG_ERR){
        closeFunction();
        errExit("cambio signal handler fallito\n");
    }  
    /*GESTISCO BOT CON UN IF E RETURN*/
    if(argc==3 && *argv[2]=='*'){
        //only one mi dice se c'è solo un client (caso del bot)
        //Metto onlyone a true prima dei segnali per sicurezza
        //Altrimenti con una sfortuna assurda se manda il segnale
        //prima che il codice arrivi a vedere onlyOne true dopo si buggerebbe
        //questa variabile verrà usata solo dal client stesso come supporto
        onlyOne=true;
        //sta nelle shm di supporto
        //serve al server per capire se attivare la funzionalità bot
        client->wantBot=true;
        strcpy(client->playerName,argv[1]);
        client->pid=getpid();
        //adesso che ho inserito i dati del primo e unico client nel supporto
        //libero il server che stava aspettando il primo client
        semOp(semid,EMPTY,1);
        printf("Stai giocando contro il bot\n");
        int time=0;
        //gestisco la sincronizzazione del gioco
        while(1){
            //mutex era inizializzato a 1 quindi il client gioca per primo
            semOp(semid,ASSEST,-1);
            *turn=0; //0 client 1 bot
            //Se time è maggiore di 0 ed è il turno del player significa che il bot ha fatto la mossa
            //e non ha ancora vinto
            system("clear");
            if(time>0){
                printf("\nIl bot ha fatto la sua mossa, matrice aggiornata:\n");
            }
            else{
                printf("\nMatrice di gioco iniziale:\n");
            }
            //stampa la matrice prima di giocare cosi vede la mossa che ha fatto il bot
            //oppure la matrice vuota iniziale
            printMatrix(matrix);
            time++;
            do{
                //in valid (inizializzata a 1) viene memorizzato il motivo del perchè la mossa non è valida
                //valid va bene solo se sta a 1
                if(valid==-1)
                    printf("\nGIOCATA SBAGLIATA, ci sono %d colonne massime, non puoi giocare oltre, ripeti\n",matrix->max_cols);
                if(valid==-2)
                    printf("\nGIOCATA SBAGLIATA, la colonna selezionata è piena, ripeti\n");
                if(valid==-3)
                    printf("\nGIOCATA SBAGLIATA, la colonna deve essere maggiore di 0, ripeti\n");
                printf("\n[TURNO TUO(hai 30 secondi massimo)] Inserisci la colonna:\n");
                
                bool isNumber;
                do{
                    fgets(value,sizeof(value),stdin);
                    isNumber= isDigitNumber(value);
                    if(!isNumber){
                        printf("Devi inserire un numero intero!!!\n");
                        printf("\n[TURNO TUO(hai 30 secondi massimo)] Inserisci la colonna:\n");
                    }
                    //scanf non ci andava !!
                }while(!isNumber);
                col=atoi(value);
                row=currentRow(col,matrix);
                valid=isValid(col,matrix);
            }while(valid!=1);
            kill(cinfo->pid_server,SIGUSR2);
            //memorizzo la giocata fatta nella matrice con la shape del client
            //nel caso di client bot il client userà sempre la prima shape
            M[row*matrix->max_cols+col-1]=shape->sh1;
            system("clear");
            printf("\nMatrice di gioco dopo la tua giocata:\n");
            printMatrix(matrix);
            //sblocco il server che controlla se vinco e fa giocare il bot
            semOp(semid,BS,1);
            //prima di scrivere che il bot sta giocando
            //blocco e aspetto che il server controlla che non abbia gia vinto prima
            //se non facessi questo controllo scriverebbe che il bot sta giocando 
            //anche quando ho fatto l'ultima mossa e ho vinto
            semOp(semid,ASSEST,-1);
            printf("\n------------------------------------\n");
            printf("\nIl bot sta giocando aspetta\n");
            printf("\n------------------------------------\n");
        }
        return 0;
    }
    /*QUESTO CODICE VIENE ESEGUITO SOLO SE NONC C'È MODALITÀ BOT*/
    //ricordiamoci che mutex era inizializzata a 1
    //quindi il primo client passa e inserisce i suoi dati nel supporto
    //e mentre il primo client inserisce i suoi dati se arriva un secondo client
    //rimane bloccato
    semOp(semid,MUTEX, -1);
    client->pid=getpid();
    strcpy(client->playerName,argv[1]);
    client->wantBot=false;
    semOp(semid,EMPTY,1);
    printf("Aspetta che arrivi un altro giocatore...\n");
    //aspetta che arrivi un secondo giocatore, verrà liberato dal servere
    //quando il secondo giocatore inserirà i suoi dati nel supporto
    semOp(semid,WAIT_SECOND,-1);
    //libera a cascata il secondo client
    semOp(semid,WAIT_SECOND,1);
    system("clear");
    //mi setto variabile clientNumber per comodità per capire quale client sta giocando
    //senza dover ogni volta controllare il pid
    if(getpid()==cinfo->pid_c1){
        clientNumber=0;
    }
    else if(getpid()==cinfo->pid_c2){
        clientNumber=1;
    }
    
    int c1Turn=1;
    int c2Turn=0;
    //iniziano a giocare
    while(1){
        if(clientNumber==0){
            if(!c1Turn){
                c1Turn=1;
                printf("\n------------------------------------\n");
                printf("\nAspetta che %s faccia la sua giocata...\n",cinfo->c2Name);
                printf("\n------------------------------------\n");
                semOp(semid,BC1,-1);
            }
            c1Turn=0;
        }
        else if(clientNumber==1){
            if(!c2Turn){
                c2Turn=1;
                printf("\n------------------------------------\n");
                printf("\nAspetta che %s faccia la sua giocata...\n",cinfo->c1Name);
                printf("\n------------------------------------\n");
                semOp(semid,BC2,-1);
            }
            c2Turn=0;
        }
        //Stampa o la matrice iniziale o la mossa fatta precedentemente dall'avversario
        system("clear");
        printf("\n[Matrice di gioco corrente]\n");
        printMatrix(matrix);
        //inserisce la colonna finchè non inserisce una valida o scade il minuto
        do{
            if(valid==-1)
                printf("\nGIOCATA SBAGLIATA, ci sono %d colonne massime, non puoi giocare oltre, ripeti\n",matrix->max_cols);
            if(valid==-2)
                 printf("\nGIOCATA SBAGLIATA, la colonna selezionata è piena, ripeti\n");
            if(valid==-3)
                printf("\nGIOCATA SBAGLIATA, la colonna deve essere maggiore di 0, ripeti\n");
            printf("\n[TURNO TUO(hai 30 secondi massimo)] Inserisci la colonna:\n");
            bool isNumber;
            do{
                fgets(value,sizeof(value),stdin);
                isNumber= isDigitNumber(value);
                if(!isNumber){
                    printf("Devi inserire un numero intero!!!\n");
                    printf("\n[TURNO TUO(hai 30 secondi massimo)] Inserisci la colonna:\n");
                }
            }while(!isNumber);    
            col=atoi(value);
            printf("%d\n",col);
            valid=isValid(col,matrix);//!!!
            row=currentRow(col,matrix);
        }while(valid!=1);
        //mando sigusr2 per dire che ha finito di fare la mossa
        //cosi vedo se ha fatto int tempo a farla, se fa in tempo a mandare il 
        //segnale significa che ce l'ha fatta
        kill(cinfo->pid_server,SIGUSR2);
        //inserisce la sua forma
        if(clientNumber==0){
            M[row*matrix->max_cols+col-1]=shape->sh1;
        }else if(clientNumber==1){
            M[row*matrix->max_cols+col-1]=shape->sh2;
        }
        //stampa matrice dopo la giocata
        system("clear");
        printf("\n[Matrice di gioco dopo la tua giocata]\n");
        printMatrix(matrix);
        //Libero il server per fare i check dopo aver fatto la mia giocata
        semOp(semid,BS,1);
        //Blocco il client
        //verrà liberato dal server dopo aver fatto check se c'è stata vitoria o sconfitta
        semOp(semid,BC,-1);
        //ricordiamoci che turn era inizializzata a 0 quindi non ci sono problemi
        if(*turn==0)
            *turn=1;
        else if(*turn==1)
            *turn=0;
        //libero il server dopo aver settato turn giusto
        //altrimenti ci potrebbero essere problemi di scheduling
        semOp(semid,BS,1);
        //libero l'altro client che era in attesa
        if(clientNumber==0)
            semOp(semid,BC2,1);
        else
            semOp(semid,BC1,1);
    }

    return 0;
}
//stampa matrice
void printMatrix(struct Matrix *matrix){
    int j;
    for(int i=0;i<matrix->max_rows;i++){
        for(j=0;j<matrix->max_cols;j++){
            printf("|%c|",M[i*matrix->max_cols+j]);
        }
        printf("\n");
    }
    for(j=1;j<=matrix->max_cols;j++){
    	if(j>=10){
    		printf(" %d",j);
    	}else{ 
    		printf(" %d ",j);	
    	}
        
    }
    printf("\n");
}
//controlla validità di colonna inserita
int isValid(int col,struct Matrix *matrix){
    int i;
    //controllo sia maggiore di 0 PRIMA DEGLI ALTRI!!! altrimenti va a -1 e segm fault
    if(col <=0){
        return -3;
    }
    //controllo non abbia inserito oltre
    if(col>matrix->max_cols)
        return -1; 
    //controllo la colonna non sia piena
    for(i=0;i<matrix->max_rows;i++){
        if(M[i*matrix->max_cols+col-1]==' ')
            break;
    }

    if(i==matrix->max_rows)
        return -2;   
    return 1;
}
//gestisce la vittoria
void sigHandlerWinner(int sig){
    if(sig==SIGUSR1){
        /*
        Gestisco se la vittoria deriva da una buona giocata del vincitore
        o perchè il tempo dell'avversario è scaduto
        */
        if(clientNumber==0){
            if(*timeRunOut){
                printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
                printf("\nHAI VINTO perchè il tempo di %s è scaduto, gg %s\n",cinfo->c2Name,cinfo->c1Name);
                printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
            }else{
                printf("\n*****************************************\n");
                printf("\nHAI VINTO, gg %s\n",cinfo->c1Name);
                printf("\n*****************************************\n");
            }
            
        }
        else if(clientNumber==1){
            if(*timeRunOut){
                printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
                printf("\nHAI VINTO perchè il tempo di %s è scaduto, gg %s\n",cinfo->c1Name,cinfo->c2Name);
                printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"); 
            }else{
                printf("\n*****************************************\n");
                printf("\nHAI VINTO,gg %s\n",cinfo->c2Name);
                printf("\n*****************************************\n");
            }
        }
        closeFunction();
        exit(0);
    }
    
}
//gesitsce la sconfitta
void sigHandlerLoser(int sig){
    if(sig==SIGUSR2){
        /*
        Sia nel caso di sconfitta contro il bot, o di sconfitta di c1 o di c2
        gestisco se la sconfitta deriva da una mossa dell'opponent o perchè 
        al perdente è scaduto il tempo, con la variabile condivisa timeRunOut
        */
        //stampo l'ultima mossa solo se non ha perso perchè è scaduto il tempo
        if(!*timeRunOut){
            system("clear");
            printf("\n[Matrice di gioco finale]\n");
            printMatrix(matrix);
        }
        if(onlyOne){
            if(*timeRunOut){
                printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
                printf("\nHAI PERSO perchè il tuo tempo è scaduto, %s\n",cinfo->c1Name);
                printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
            }else{
                printf("\n*****************************************\n");
                printf("\nIl bot ha vinto, tu HAI PERSO %s\n",cinfo->c1Name);
                printf("\n*****************************************\n");
            }
        }
        else if(clientNumber==0){
            if(*timeRunOut){
                printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
                printf("\nHAI PERSO perchè il tuo tempo è scaduto, %s\n",cinfo->c1Name);
                printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
            }else{
                printf("\n*****************************************\n");
                printf("\n%s ha vinto, tu HAI PERSO %s\n",cinfo->c2Name,cinfo->c1Name);
                printf("\n*****************************************\n");
            }
        }
        else if(clientNumber==1){
            if(*timeRunOut){
                printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
                printf("\nHAI PERSO perchè il tuo tempo è scaduto, %s\n",cinfo->c2Name);
                printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
            }else{
                printf("\n*****************************************\n");
                printf("\n%s ha vinto,tu HAI PERSO %s\n",cinfo->c1Name,cinfo->c2Name);
                printf("\n*****************************************\n");
            }
        }
        closeFunction();
        exit(0);
    }
    
}
//gestisco ritiro di un giocatore
//il ritiro corrisponde a ctrl c o a chiusura terminale
void sigHandlerWithdraw(int sig){
    if(sig==SIGINT || sig==SIGHUP){
        //is Retired prende il client number cosi server capisce chi si è ritirato
        *isRetired=clientNumber;
        printf("\n*****************************************\n");
        printf("\n HAI PERSO per ritiro\n");
        printf("\n*****************************************\n");
        //SIGUSR1 se mandato ai client significa che hanno vinto
        //mentre se mandato a server notifica ritiro
        kill(cinfo->pid_server,SIGUSR1);

        closeFunction();

        exit(0);
    }
    
}
//gestisco pareggio o chiusura server
void sigHandlerDraw(int sig){
    if(sig==SIGQUIT){
        printf("\n*****************************************\n");
        printf("\nPAREGGIO\n");
        printf("\n*****************************************\n");
        closeFunction();
        exit(0);
    }
}
//funzione attivata quando chiude
void closeFunction(){
    if(shmidClient!=-1)
        free_shared_memory(client);
    if(shmidInfo!=-1)
        free_shared_memory(cinfo);
    if(shmidShape!=-1)
        free_shared_memory(shape);
    if(shmidMatrix!=-1)
        free_shared_memory(matrix);
    if(shmidTurn!=-1)
        free_shared_memory(turn);
    if(shmidHowMany!=-1)
        free_shared_memory(howMany);
    if(shmidTimeRunOut!=-1)
        free_shared_memory(timeRunOut);
    if(shmidIsRetired!=-1)
        free_shared_memory(isRetired);
    if(shmidM!=-1)
        free_shared_memory(M);
}
//restituisce la prima riga libera corrispondente a una colonna in matrice
int currentRow(int col,struct Matrix *matrix){
    int i;
    for(i=1;i<matrix->max_rows;i++){
        if(M[i*matrix->max_cols +col-1]==shape->sh1 || M[i*matrix->max_cols+col-1]==shape->sh2)
            break;
    }

    return i-1;
}
//restituisce true se è inserito un numero, false altrimenti
bool isDigitNumber(char value[9]){
    int length=strlen(value);
    //ho gia tolto prima il \n 
    for(int i=0;i<length-1;i++){
        if(!(isdigit(value[i]))){
            return false;
        }
    }
    return true;
}


/************************************
*VR471795
*Alberto Ondei
*20/05/2023
*************************************/
