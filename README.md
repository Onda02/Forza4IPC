# Forza4
Forza4 è una replica digitale del classico gioco da tavolo "Forza 4". Il gioco è progettato per essere giocato da due giocatori, alternando turni per inserire le proprie pedine nel tabellone. Lo scopo è allineare quattro pedine consecutive verticalmente, orizzontalmente o diagonalmente prima dell'avversario.

Il gioco è stato scritto in C a scopo didattico per imparare system call relative a segnali, semafori,
memoria condivisa, gestione dei processi figli e exec e per imparare a fare interagire diversi processi tra di loro.



## Eseguire i programmi e come giocare
Questo gioco sfrutta le sistem call SYSTEMV ed è in grado di funzionare solo in ambiente UNIX/LINUX.
Per giocare devi:
1. Clonare il repository sul tuo computer.
2. Spostarti dentro la cartella principale 'Forza4' e compilare col comando make.
3. Avviare il gioco.

Per avviare il gioco bisogna aprire tre shell delle quali:
- Una serve per poter eseguire il programma server che fa da arbitro tra i due giocatori
- Le altre due servono per eseguire i due programma client che rappresentano i due giocatori

### Server
Il server va eseguito in questo modo:

 ```shell
./F4Server.c righe colonne forma1 forma2
 ```

 Dove:
- Righe e colonne sono due interi e rappresentano la dimensione del campo da gioco che devono essere almeno 5*5 e sia le righe che le colonne devono essere più piccole di 100
- Forma1 e forma2 sono due char e rappresentano le i gettoni usati dai due giocatori, quindi se scriviamo X P per esempio, quando il giocatore 1 inserirà la mossa verrà stampato a schermo la X mentre quando farà la mossa il giocatore 2 verrà stampata a schermo la P.

Una volta eseguito il server aspetterà l'arrivo degli altri giocatori client.
Il server farà da arbitro e quindi stabilirà dopo ogni mossa dei giocatori se uno dei due giocatori ha vinto o se è pareggio e segnalerà ai client il risultato della partita.

Se si vuole smettere di giocare è sufficiente premere due volte in 5 secondi CTRL C e il server notificherà ai client della fine della partita con un pareggio.

### Client
Ci sono due modalità di gioco:
1. Player1 vs player2
2. Player1 vs bot
#### Player1 vs player2
In questo caso il client va avviato inserendo il nome del giocatore in questo modo:

 ```shell
./F4Client.c nomeGiocatore
 ```
Il primo client aspetterà che si colleghi un altro client e successivamente il gioco può iniziare.
Nel caso in cui l'utente decidesse di aprire ulteriori shell ed eseguire più di due processi client viene avvisato che c'è già una partita in corso e che deve aspettare la fine di tale partita.

A ogni turno viene chiesto di scrivere la colonna in cui fare la giocata entro 30 secondi di tempo, se il tempo scade verrà considerato come sconfitta.
Inoltre nel caso viene premuto CTRL C viene considerato come ritiro e quindi anche in questo caso come sconfitta.

Quando il giocatore gioca aspetta che l'avversario faccia la sua mossa e si continua cosi finchè uno dei due giocatori non vince.
#### Player1 vs bot
In questo caso per indicare che si vuole giocare contro un bot il primo processo client va avviato in questo modo:
 ```shell
./F4Client.c  nomeGiocatore \*
 ```
Inserendo \* si sta indicando che si vuole giocare contro un bot che giocherà mosse causali.

## Descrizione scelte progettuali
Ora mi concentrerò a descrivere come ho sviluppato il codice, questa parte serve solo se si è interessati a capire il funzionamento, per eseguire il gioco basta seguire gli step precedenti
### Segnali
![Elaborazione segnali](https://github.com/Onda02/Forza4IPC/blob/main/signalDia.pdf)
### Creazione e rimozione IPC
Per generare chiavi IPC ho usato la system call ftok, mettendo come file di
riferimento quello del server e inserendo un numero che incrementavo ogni volta
che mi serviva un’altra IPC.
Inoltre tramite l’uso di sincronizzazione tra client e server ho fatto in modo
che l’unico a creare le IPC sia il server, mentre il client le prende.
Quindi il server usa una funzione speciale che ha attivate le flag IPC_CREAT
(per crearle) e IPC_EXCL (in modo che se fosse già presente nel sistema, dia
errore e termini il programma).
La rimozione delle IPC viene fatta solo dal server
### Memoria condivisa
- "struct setClient *client": memoria condivisa che ha come scopo principale quello di capire chi è il primo giocatore e chi il secondo, quindi di distinguere i due client. In sostanza il primo client che riuscirà a inserire i dati dentro questa struttura verrà considerato primo player. Questo è stato possibile grazie alla sincronizzazione tra i due client e tra il server tramit l’uso di semafori, perchè mentre il primo client inserisce i dati il secondo non può arrivare e inserire i suoi prima che il server non li abbia letti.
- "Information *cinfo": memoria condivisa in cui vengono inserite informazioni generali, nelle info verranno copiati i pid e i nomi client inseriti precedentemente nella setClient, e inoltre verrà messo anche il pid del
server, in modo che i client possano inviargli segnali.
- "Shape *shape" vengono inserite le forme di gioco inserite come parametro, serviranno ai client in quanto quando faranno la mossa riempiranno la casella di gioco con la loro forma
- "int *turn" in questa variabile il client che sta giocando scriverà che è il suo turno (0 per client 1 e 1 per client 2) cosi il server capisce chi ha fatto l’ultima mossa
- "int *howMany": mi dice quanti client stanno giocando, inizializzata a 0 dal server, se i client sono piu di due termina, oppure se un giocatore sta già giocando col bot non può esserci un altro giocatore e se viene inserito termina, oppure se c’è già un giocatore che aspetta non può essere inserito un giocatore che vuole il bot e termina.
- "int *timeRunOut": viene usata per una questione estetica, non ha scopi logici, in sostanza se il server nota che è scaduto il tempo, setta questa variabile a 1 e manda la kill di sconfitta a chi ha fatto scadere il tempo
(e di vittoria all’altro) e settando questa variabile a 1 i client possono stampare a video il motivo della sconfitta (quindi per tempo scaduto)
- "int *isRetired": anche questa ha solo scopi estetici, viene settata dal client e il server stampa a video chi si è ritirato (se si è ritirato qualcuno)
### Semafori
L’uso dei semafori si può distinguere in 3 categorie:
- Distinguere i client: In questa fase bisogna distinguere chi è il primo
client e chi il secondo, guardando chi inserisce per primo i dati in struttura
condivisa "setClient".
  - MUTEX: inizializzato a 1 serve per fare in modo che i client inseriscano i dati in setClient in modo mutualmente esclusivo.
  - EMPTY: inizializzato a 0, il server non prende i dati in setClient finchè qualcuno non li inserisce, quando il player li inserisce fa libera
  - Dopo che il server prende i dati del primo client libera MUTEX, di prima e da il via libera al secondo possibile client di entrare
  - WAIT_SECOND: il primo client aspetta che arrivi il secondo client prima di andare avanti
  - wait_second verrà liberato dal server quando quest’ultimo prende i dati del secondo client messi in setClient
- Fase di gioco:
  - BC1: blocca il client 1 quando non è il suo turno e verrà sbloccato quando il client 2 ha finito di giocare
  - BC2: blocca il client 2 quando non è il suo turno e verrà dal client 1 dopo che il server ha fatto il check di vittoria o pareggio (non ha senso liberarlo prima che il server faccia il check)
  - BS: blocca il server: viene usato 2 volte:
    1. La prima volta blocca il server per aspettare che il client faccia la giocata, e dopo fa il check di vittoria o pareggio
    2. La seconda volta viene bloccato per aspettare che il client setti la variabile condivisa "turn" al turno giusto, altrimenti potrebbero esserci problemi di scheduling e quando il server fa il check lo fa col "turn" sbagliato
    3. Nel bot ASSEST viene usato per bloccare il client, mentre BS per bloccare il server
  - BC: blocca il client (quello che sta giocando indipendentemente da 1 o 2) che aspetta il server faccia il check dopo la giocata
    
- Altri semafori:
  - START: usato per fare in modo che il server crei le shared memory prima che il client le vada a prendere, volendo questo semaforo si potrebbe evitare dato che il server deve essere il primo processo a essere runnato e la creazione delle IPC è una delle prime cose che fa, però l’ho messo per sicurezza
  - MUTEX_2: usato per aumentare o diminuire howmany in modo mutualmente esclusivo tra i due client, non potevo usare MUTEX per questione di scheduling, dato che successivamente viene usato per altro.
### Descrizione generale del funzionamento
Ora spiegerò ad alto livello gli step che seguono il processo server e client.
#### Processo server
Il processo server compierà queste azioni in ordine:
1. Crea il set di semafori, setta la maschera di segnali, crea tutte le memorie
condivise di cui avrà bisogno e inizializza la matrice di gioco vuota (con
tutti spazi)
2. Aspetta che arrivi il primo giocatore e quando arriva prende i dati condivisi
nella struttura condivisa setClient
3. Nella struttura condivisa Information ci va ad aggiungere i dati del client
appena arrivato
4. Controlla se vuole giocare col bot
5. Se si arbitra la partita tra il client e client bot (processo che verrà eseguito
ad ogni iterazione)
6. Se no aspetta che arrivi il secondo giocatore e quando arriva prende i dati
condivisi nella struttura condivisa setClient
7. Arbitra la partita tra i due client
#### Processo client
Il processo client compierà queste azioni in ordine:
1. Prende set di semafori dal server
2. Aspetta che il server crei tutte le memorie condivise
3. Prende le memorie condivise dal server
4. Setta la maschera di segnali
5. Se vuole bot gioca la partita contro il bot
6. Altrimenti inserisce i suoi dati nella struttura condivisa setClient e gioca
contro l’altro client

