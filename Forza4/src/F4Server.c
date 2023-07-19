#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>


#include <sys/stat.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include "../inc/shared_memory.h"
#include "../inc/semaphore.h"
#include "../inc/errExit.h"

/*DIMENSIONE SEMAFORI*/
#define SEM_DIM 10

/*DEFINE SEMAFORI*/
#define EMPTY 0 
//EMPTY semaforo usato in modo che il server aspetta
//che venga riempito clientPid
#define MUTEX 1
//MUTEX viene usato in modo che un solo processo client alla volta
//possa entrare dentro l'area condivisa e riempire il clientPid
//inizializzato a 1 cosi il primo client che entra lo setta
//MUTEX viene usata anche in modo che il server prende i dati dal buffer
//prima che il seconod client ci sovrascriva
#define WAIT_SECOND 2
#define MUTEX2 3
#define ASSEST 4
#define BC 5
#define BS 6
#define BC1 7
#define BC2 8
#define START 9

int endCount=0;
pid_t firstClientPid;
pid_t secondClientPid;
int playInTime;

int semid=-1;
int shmid=-1;
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


void sigHandler(int sig);
int create_sem_set(key_t semkey);
int isWinner(struct Matrix *matrix, char shape);
int isDraw(struct Matrix *matrix);
void closeFunction();
void withdrawHandler(int sig);
void playInTimeHandler(int sig);
void resetHandler(int sig);

int main(int argc,char *argv[]){
    // check command line input arguments
    if (argc != 5) {
        printf("Uso: %s colonne righe forma1 forma2\n", argv[0]);
        exit(1);
    }
    if(atoi(argv[1])<5 || atoi(argv[2])<5){
        printf("Righe e colonne devono essere maggiori o uguali a 5\n");
        printf("Righe e colonne devono essere maggiori o uguali a 5\n");
        return 1;
    }
    if(atoi(argv[1])>100 || atoi(argv[2])>100){
    	printf("Non puoi creare una matrice di gioco con più di 100 righe o 100 colonne\n");
    	return 1;
    }
    /*CREO SET DI SEMAFORI*/
    key_t semKey=ftok("src/F4Server.c",20);
    if(semKey==-1){
        closeFunction();
        errExit("ftok fallito");
    }
    semid=create_sem_set(semKey);
    //le forme usate nel gioco
    char *shape0=argv[3];
    char *shape1=argv[4];
    //Gestisco subito i segnali
    sigset_t mySet;
    sigfillset(&mySet);
    //ctrl c (pareggio)
    sigdelset(&mySet,SIGINT);
    //chiusura terminale (pareggio)
    sigdelset(&mySet,SIGHUP);
    //ritiro giocatore
    sigdelset(&mySet,SIGUSR1);
    //mossa in tempo
    sigdelset(&mySet,SIGUSR2);
    //per resettare counter dopo 5 secondi
    sigdelset(&mySet,SIGALRM);
    
    if(sigprocmask(SIG_SETMASK, &mySet, NULL)==-1){
        closeFunction();
        errExit("sigprocmask fallita\n");
    }
    if(signal(SIGINT, sigHandler)==SIG_ERR){
        closeFunction();
        errExit("cambio signal handler fallito\n");
    }   
    if(signal(SIGHUP, sigHandler)==SIG_ERR){
        closeFunction();
        errExit("cambio signal handler fallito\n");
    }   
    if(signal(SIGUSR1,withdrawHandler)==SIG_ERR){
        closeFunction();
        errExit("cambio signal handler fallito\n");
    }
    if(signal(SIGUSR2,playInTimeHandler)==SIG_ERR){
        closeFunction();
        errExit("cambio signal handler fallito\n");
    }
    if(signal(SIGALRM,resetHandler)==SIG_ERR){
        closeFunction();
        errExit("cambio signal handler fallito\n");
    }
    //creo chiave per il supporto client e con quella creo memoria condivisa
    key_t keyClient = ftok("src/F4Server.c",1);
    if(keyClient==-1){
        closeFunction();
        errExit("ftok fallito");
    }
    shmid=server_alloc_shared_memory(keyClient,sizeof(struct setClient));
    client= (struct setClient *) get_shared_memory(shmid,0);
    //creo chiave per tutte le informazioni tra processi e ci creo memoria condivisa
    key_t keyInfo = ftok("src/F4Server.c",2);
    if(keyInfo==-1){
        closeFunction();
        errExit("ftok fallito");
    }
    shmidInfo=server_alloc_shared_memory(keyInfo,sizeof(struct Information));
    cinfo=(struct Information *)get_shared_memory(shmidInfo,0);
    //creo chiave per le forme condivise c1->shape1 c2->shape2
    key_t keyShape = ftok("src/F4Server.c",3);
    if(keyShape==-1){
        closeFunction();
        errExit("ftok fallito");
    }
    shmidShape=server_alloc_shared_memory(keyShape,sizeof(struct Shape));
    shape= (struct Shape *)get_shared_memory(shmidShape,0);
    //creo chiave per matrice di gioco condivisa
    key_t keyMatrix = ftok("src/F4Server.c",4);
    if(keyMatrix==-1){
        closeFunction();
        errExit("ftok fallito");
    }
    shmidMatrix=server_alloc_shared_memory(keyMatrix, sizeof(struct Matrix));
    matrix= (struct Matrix *)get_shared_memory(shmidMatrix,0); 
    //creo chiave per variabile turn condivisa cosi il server sa di chi è il turno
    //e deve fare meno controlli per la vittoria, perchè solo l'ultimo che ha giocato
    //può vincere
    key_t keyTurn= ftok("src/F4Server.c",5);
    if(keyTurn==-1){
        closeFunction();
        errExit("ftok fallito");
    }
    shmidTurn=server_alloc_shared_memory(keyTurn,sizeof(int));
    turn=(int *)get_shared_memory(shmidTurn,0);

    key_t keyHowMany = ftok("src/F4Server.c",6);
    if(keyHowMany==-1){
        closeFunction();
        errExit("ftok fallito");
    }
        
    shmidHowMany=server_alloc_shared_memory(keyHowMany,sizeof(int));
    howMany=(int *)get_shared_memory(shmidHowMany,0);
    *howMany=0;

    key_t keyTimeRunOut = ftok("src/F4Server.c",7);
    if(keyTimeRunOut==-1){
        closeFunction();
        errExit("ftok fallito");
    }
    shmidTimeRunOut=server_alloc_shared_memory(keyTimeRunOut,sizeof(int));
    timeRunOut=(int *)get_shared_memory(shmidTimeRunOut,0);
    *timeRunOut=0;

    key_t keyIsRetired = ftok("src/F4Server.c",8);
    if(keyIsRetired==-1){
        closeFunction();
        errExit("ftok fallito");
    }
    shmidIsRetired=server_alloc_shared_memory(keyIsRetired,sizeof(int));
    isRetired=(int *)get_shared_memory(shmidIsRetired,0);
    *isRetired=-1;

    key_t keyM=ftok("src/F4Server.c",9);
    if(keyM==-1){
        closeFunction();
        errExit("ftok fallito");
    }
    shmidM=server_alloc_shared_memory(keyM,sizeof(char)*atoi(argv[1])*atoi(argv[2]));
    M=(char *)get_shared_memory(shmidM,0);
    

    semOp(semid,START,1);

     /*ASSEGNO FORME DI GIOCO (inserite come parametro) IN MEMORIA CONDIVISA*/
    shape->sh1=*shape0;
    shape->sh2=*shape1;

    /*ALLOCO MATRIX E LA INIZIALIZZO*/
    matrix->max_rows=atoi(argv[1]);
    matrix->max_cols=atoi(argv[2]);
    for(int i=0;i<matrix->max_rows;i++){
        for(int j=0;j<matrix->max_cols;j++){
            //la inizializzo con spazi vuoti
            M[i*matrix->max_rows+j]=' ';
        }
    }
    //nome del primo giocatore
    char firstPlayerName[250];
    printf("Aspettando che arrivi il primo giocatore...\n");
    /*PRENDO DA MEMORIA CONDIVISA PID DEI CLIENTE E I NOMI DEI PLAYER*/
    //aseptta arrivo del primo giocatore
    semOp(semid,EMPTY,-1);
    /*PRIMO PLAYER*/
    //serve il supporto perchè il primo che arriva come fa a sapere
    //che è il primo?
    //guardare how many non è sicuro
    //perchè prima che lo scheduler guarda howmany nel mio primo processo
    //potrebbe entrare un nuovo cliente aumentare howmany a 2 e il primo processo 
    //si ritrova con howmany a 2 e non può piu andare avanti
    //pero avrei potuto effettivamente sincronizzare l'incremento di howmany
    firstClientPid= client->pid;
    strcpy(firstPlayerName,client->playerName);
    //nelle info
    cinfo->pid_c1=firstClientPid;
    strcpy(cinfo->c1Name,firstPlayerName);
    printf("%s è arrivato...\n",firstPlayerName);
    
    /*FUNZIONALITÀ BOT*/
    if(client->wantBot){
        cinfo->pid_server=getpid();
        printf("%s gioca contro il bot...\n",firstPlayerName);
        while(1){
            /*FUNZIONALITÀ TIMER*/
            //play in time viene messo a 1 quando il giocatore fa  la sua giocata
            //e manda segnale che mette a 1 play in time
            playInTime=0;
            unsigned int t=30;
            while(playInTime==0 && t!=0)
                t=sleep(t);
            //se sono passati 30 secondi e playintime non è ancora a 1 significa che è scaduto il tempo
            //se invece playintime è a 1 significa che il player ha giocato, ha attivato il segnale e il segnale ha messo playintime a 1
            if(playInTime!=1){
                //cosi il client capisce che ha perso perchè è scaduto il tempo
                *timeRunOut=1;
                kill(cinfo->pid_c1,SIGUSR2);
                printf("Il bot ha vinto perchè il tempo di %s è scaduto\n",cinfo->c1Name);
                closeFunction();
                exit(0);
            }
            //blocco server che viene sbloccato dal client quando finisce di giocare
            semOp(semid,BS,-1);
            *turn=1;
            if(isWinner(matrix,*shape0)){
                printf("%s ha vinto\n",cinfo->c1Name);
                kill(cinfo->pid_c1,SIGUSR1);
                closeFunction();
                return 0;
            }
            if(isDraw(matrix)){
                printf("Pareggio");
                kill(cinfo->pid_c1,SIGQUIT);
                closeFunction();
                return 0;
            }
            //se arriva qua la paritta non è finita e sblocca il client che scrive che sta aspettando la giocata del bot
            semOp(semid,ASSEST,1);
            printf("Il bot sta facendo la sua giocata...\n");
            //faccio dormire 2 secondi cosi rendo piu realistico
            //senno risponde subito e non mi piace
            sleep(2);
            pid_t bot=fork();
            if(bot==-1){
                closeFunction();
                errExit("fork fallita");
            }
                
            else if(bot==0){
                execlp("./F4ClientBot", "ClientBot", shape1, NULL);
                closeFunction();
                errExit("execlp fallita\n");
            }
            if(wait(NULL)==-1){
                closeFunction();
                errExit("Wait fallita");
            }
                
            if(isWinner(matrix,*shape1)){
                printf("Il bot ha vinto\n");
                kill(cinfo->pid_c1,SIGUSR2);
                closeFunction();
                return 0;
            }
            if(isDraw(matrix)){
                printf("Pareggio\n");
                kill(cinfo->pid_c1,SIGQUIT);
                closeFunction();
                return 0;
            }
            printf("%s sta facendo la sua mossa...\n",cinfo->c1Name);
            //sblocca il client che fa la sua giocata
            semOp(semid,ASSEST,1);

        }
        return 0;
    }
    printf("Aspettando che arrivi il secondo giocatore...\n");
    //ora sbloco il prossimo player
    semOp(semid,MUTEX,1);
    char secondPlayerName[250];
    //aspetto che il porssimo player inserisca i suoi dati
    semOp(semid,EMPTY,-1);
    /*SECONDO PLAYER*/
    //Nel supporto
    secondClientPid=client->pid;
    //Nelle info
    cinfo->pid_c2=secondClientPid;
    //Nel supporto
    strcpy(secondPlayerName,client->playerName);
    //Nelle info
    strcpy(cinfo->c2Name,secondPlayerName);
    cinfo->pid_server=getpid();
    printf("È %s contro %s, buona fortuna\n", firstPlayerName, secondPlayerName);
    //libera il client 1 che stava aspettando che arrivasse il secondo client
    /*
    inoltre adesso siamo sicuri che nella memoria condivisa quando il 
    client va a prendere dati ci saranno, se avessi messo prima wait_second e empty
    potevano crearsi problemi perchè il client andava a prendersi i dati da cinfo prima 
    che venissero inseriti
    */
    semOp(semid,WAIT_SECOND,1);
    //serve ai client per sincronizzarsi sulla variabile condivisa
    semOp(semid,EMPTY,1);

    int winner;
    while(1){
        if(*turn==0){
            printf("%s sta facendo la sua mossa...\n",cinfo->c1Name);
        }else if(*turn==1){
            printf("%s sta facendo la sua mossa...\n",cinfo->c2Name);
        }
        /*FUNZIONALITÀ TIMER*/
        //play int time viene messo a 1 quando il giocatore fa  la sua giocata
        //e manda segnale che mette a 1 play in time
        playInTime=0;
        unsigned int t=30;
        while(playInTime==0 && t!=0)
            t=sleep(t);
        //se sono passati 30 secondi e playintime non è ancora a 1 significa che è scaduto il tempo
        if(playInTime!=1){
            *timeRunOut=1;
            if(*turn==0){
                kill(secondClientPid,SIGUSR1);
                kill(firstClientPid,SIGUSR2);
                printf("%s ha vinto perchè il tempo di %s è scaduto\n",secondPlayerName, firstPlayerName);
            }else{
                kill(firstClientPid,SIGUSR1);
                kill(secondClientPid,SIGUSR2);
                printf("%s ha vinto perchè il tempo di %s è scaduto\n",firstPlayerName, secondPlayerName);   
            }
            break;
        }
        //Aspetto che il client faccia la sua giocata e poi fa il check
        semOp(semid,BS,-1);
        //non ha senso controllare entrambi solo chi ha fatto l'ultima mossa può aver vinto
        //MANDO SIGUSR1 a chi vince e SIGUSR2 a chi perde
        if(*turn==0){
            winner=isWinner(matrix,*shape0);
            if(winner){
                kill(firstClientPid,SIGUSR1);
                kill(secondClientPid,SIGUSR2);
                printf("%s ha vinto\n",firstPlayerName);
                break;
            }
        }
        else if(*turn==1){
            winner=isWinner(matrix,*shape1);
            if(winner){
                kill(secondClientPid,SIGUSR1);
                kill(firstClientPid,SIGUSR2);
                printf("%s ha vinto\n",secondPlayerName);
                break;
            }
        }
        if(isDraw(matrix)){
            kill(secondClientPid,SIGQUIT);
            kill(firstClientPid,SIGQUIT);
            printf("Pareggio\n");
            break;
        }
        //libero il client bloccato che ha fatto la giocata in attesa che il server facesse il check
        semOp(semid,BC,1);
        //aspetta che il client setti turn giusto per la prossima giocata
        semOp(semid,BS,-1);
    }
    //libero e rimuovo memoria condivisa
    closeFunction();
    return 0;
}
//Gestisce chiusura server
void sigHandler(int sig){
    if(sig==SIGINT){ 
        if(endCount<1){
            alarm(5);
            endCount++;
            printf("\nSchiaccia ctrl C ancora per terminare (5 sec)\n");
        }
        else{
            alarm(0);
            kill(firstClientPid,SIGQUIT);
            kill(secondClientPid,SIGQUIT);
            closeFunction();
            exit(0);
        }
    }else if(sig==SIGHUP){
        kill(firstClientPid,SIGQUIT);
        kill(secondClientPid,SIGQUIT);
        closeFunction();
        exit(0);
    }
}
//Gestisce il ritiro di un giocatore mandando la vittoria all'altro 
void withdrawHandler(int sig){
    //segnale di ritiro
    if(sig==SIGUSR1){
        if(client->wantBot)
            printf("Bot ha vinto perchè %s si è ritirato\n",cinfo->c1Name);
        //se quando viene inviato turn era a 0 significa che è stato il c1 a mandarlo
        else if(*isRetired==0){
            printf("%s ha vinto perchè %s si è ritirato\n",cinfo->c2Name,cinfo->c1Name);
            //manda segnale di vittoria a player 2
            kill(cinfo->pid_c2,SIGUSR1);
        }
            
        //viceversa
        else if(*isRetired==1){
            printf("%s ha vinto perchè %s si è ritirato\n",cinfo->c1Name,cinfo->c2Name);
            //manda segnale di vittoria a player 1
            kill(cinfo->pid_c1,SIGUSR1);
        }
            
        closeFunction();
        exit(0);
    }
}
//Funzione che crea semafori
int create_sem_set(key_t semkey){   
    //create a semaphore set wit 2 semaphore
    int semid=semget(semkey,SEM_DIM,IPC_CREAT|IPC_EXCL|S_IRUSR|S_IWUSR);
    if(semid==-1){
        closeFunction();
        errExit("semget faklita\n");
    }
    //inizializzo set di semafori
    union semun arg;
    unsigned short values[]= {0,1,0,1,1,0,0,0,0,0};
    arg.array=values;

    if(semctl(semid,0,SETALL, arg)==-1){
        closeFunction();
        errExit("semctl SETALL fallita");
    }
    return semid;
}
//Funzione che controlla vittoria
int isWinner(struct Matrix *matrix, char shape){
    //0==no winner, 1==winner
    int i;
    int j;
    int counter;
    int res=0;
    int saved;
    //controllo le colonne
    for(i=0;i<matrix->max_rows &&res!=1;i++){
        counter=0;
        for(j=0;j<matrix->max_cols;j++){
            //NON DEVO FARE BREAK A PRESCINDERE
            if(M[i*matrix->max_cols+j]==shape){
                counter++;
                if(counter==4){
                    res=1;
                    break;
                }  
            }else
                counter=0;
        }
    }
    if(res==1)
        return res;
    //se nelle colonne non c'è nessuna win controllo le righe
    for(i=0;i<matrix->max_rows && res!=1;i++){
        counter=0;
        for(j=0;j<matrix->max_cols;j++){
            //NON DEVO FARE BREAK A PRESCINDERE
            if(M[j*matrix->max_cols+i]==shape){           
                counter++;
                if(counter==4){
                    res=1;
                    break;
                }  
            }else
                counter=0;
        }
    }
    if(res==1)
        return res;


    //controllo diagonali
    //controllo le diagonali da sinistra verso destra
    for(j=0;j<matrix->max_cols-2 && res!=1;j++){
        counter=0;
        saved=j;
        for(i=0; j<matrix->max_cols && i<matrix->max_rows;i++,j++){
            if(M[i*matrix->max_cols+j]==shape){
                counter++;
                if(counter==4){
                    res=1;
                    break;
                }
            }else{
                counter=0;
            }
        }
        //ripristino il valore di j
        j=saved;
    }
    if(res==1)
        return res;
    //controllo la parte in basso della diagonale da sinistra verso destra
    for(i=1;i<matrix->max_rows-2 && res!=1;i++){
        counter=0;
        saved=i;
        for(j=0;i<matrix->max_rows && j<matrix->max_cols;j++,i++){
            if(M[i*matrix->max_cols+j]==shape){
                counter++;
                if(counter==4){
                    res=1;
                    break;
                }
            }else{
                counter=0;
            }
        }
        //i ripristina il suo vecchio valore
        i=saved;
    }
    if(res==1)
        return res;
    //controllo le diagonali da destra verso sinistra
    //(ottimizzazione)SE J NON È ALMENO 4 NON È SUFFICIENTE A PRIORI RIUSCIRE A VINCERE
    for(j=matrix->max_cols-1 ;j>=2 && res!=1;j--){
        counter=0;
        saved=j;
        for(i=0;i<matrix->max_rows && j>=0;i++,j--){
            if(M[i*matrix->max_cols+j]==shape){
                counter++;
                if(counter==4){
                    res=1;
                    break;
                }
            }else{
                counter=0;
            }
            //dato che sto controllando l'altra daigonale j diminuisce
        }
        j=saved;
    }
    if(res==1)
        return res;
    //controllo la parte in basso della diagonale da destra verso sinistra
    for(i=1;i<matrix->max_rows-2 && res!=1;i++){
        counter=0;
        saved=i;
        for(j=matrix->max_cols-1;i<matrix->max_rows && j>=0;j--,i++){
            if(M[i*matrix->max_cols+j]==shape){
                counter++;
                if(counter==4){
                    res=1;
                    break;
                }
            }else{
                counter=0;
            }
        }
        i=saved;
    }
    return res;
}

int isDraw(struct Matrix *matrix){
    int res=1;
    for(int i=0;i<matrix->max_rows;i++){
        for(int j=0;j<matrix->max_cols;j++){
            if(M[i*matrix->max_cols+j]==' '){
                res=0;
                break;
            }
        }
    }
    return res;
}

//Funzione di chiusura attivata ogni volta che il programma termina 
//Chiude tutte le ipc (semafori e shm) e libera memoria condivisa
void closeFunction(){
    //se è -1 o non è stato ancora creato o è fallita
    if(shmid!=-1){
        free_shared_memory(client);
        remove_shared_memory(shmid);
    }
    
    if(shmidInfo!=-1){
        free_shared_memory(cinfo);
        remove_shared_memory(shmidInfo);
    }
    
    if(shmidShape!=-1){
        free_shared_memory(shape);
        remove_shared_memory(shmidShape);
    }
    
    if(shmidMatrix!=-1){
        free_shared_memory(matrix);
        remove_shared_memory(shmidMatrix);
    }
    
    if(shmidTurn!=-1){
        free_shared_memory(turn);
        remove_shared_memory(shmidTurn);
    }

    if(shmidHowMany!=-1){
        free_shared_memory(howMany);
        remove_shared_memory(shmidHowMany);
    }

    if(shmidTimeRunOut!=-1){
        free_shared_memory(timeRunOut);
        remove_shared_memory(shmidTimeRunOut);
    }

    if(shmidIsRetired!=-1){
        free_shared_memory(isRetired);
        remove_shared_memory(shmidIsRetired); 
    }

    if(shmidM!=-1){
        free_shared_memory(M);
        remove_shared_memory(shmidM);
    }
    
    if(semid!=-1){
        if(semctl(semid,0,IPC_RMID,NULL)==-1){
            closeFunction();
            errExit("semctl IPC_RMID fallita\n");
        }  
    }
}
//Se il giocatore che ha il turno gioca in tempo lancia questo segnale che lo salva
void playInTimeHandler(int sig){
    if(sig==SIGUSR2){
        playInTime=1;
    }
}

//Resetta il contatore per chiudere il server
void resetHandler(int sig){
    if(sig==SIGALRM){
        endCount=0;
    }
}

/************************************
*VR471795
*Alberto Ondei
*20/05/2023
*************************************/
