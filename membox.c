/* membox.c by Giorgi Matteo, 517183 & Quarta Andrea, 517881 */


/*
 * membox Progetto del corso di LSO 2016 
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Pelagatti, Torquati
 * 
 */
/**
 * @file membox.c
 * @brief File principale del server membox
 */

#define _POSIX_C_SOURCE 200809L
#include <alloca.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>

#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <errno.h>
#include "stats.h"
#include "message.h"
#include "queue.h"
#include "errors.h"
#include "icl_hash.h"
/*
	Si definisce qui __OP__ che servirà, ad includere ops.h in read_write.h .
	Data la capacità della libreria read_write di essere utilizzata
	in qualsiasi contesto, anche in eventuali altri codici dove non sono
	richieste operazioni nel repository, questa operazione è qui necessaria.
*/
#define __OP__
#include "read_write.h"
#include "connections.h"
#include "membox.h"


/*
	Funzioni per lavoratore, gestore
	e funzioni di servizio per la libreria icl_hash
*/
static void *main_lavoratore(void *args);
static void *main_gestore(void *args);
static inline unsigned int FNV1_hash(void *key, int len);
static inline unsigned int hashing(void *data);
static inline int compare_key(void *first_key, void *second_key);
static inline void free_key(void *key);
static inline void free_data(void *data);


/*
	Thread principale che si occupa di settare la socket,
	creare il thread-gestore dei segnali, creare il pool di thread,
	compiera il ciclo di "arbitro" dove accetta le connessioni, fare la join
	sui thread suddetti ed invocare cleanup_pop per pulire la memoria.
*/
int
main(int argc, char **argv, char **envp)
{
	int ris, n, k, sfd, cfd; void *ptr;
	ssize_t wr, ur;
	char *MY_FILE, *SERVER_SOCK_PATH;
	FILE *fout = NULL;
	sigset_t block_mask;
	struct sockaddr_un addr;
	pthread_t tid_gestore;
	void *retval_gestore;
	extern char *optarg;
	extern int optind, opterr, optopt;


	/***************************************************************************
		Prendo il file di configurazione dagli argomenti
	***************************************************************************/
	while( (ris=getopt(argc, argv, "f:"))!=-1 ) {
		switch( ris ){
			case 'f':
				if( !(MY_FILE=strdup(optarg)) )
					err_exit("strdup fallita");
				break;

			case '?':
				err_exit("opzione -%c o argomento sbagliati\n", optopt);
		}
	}


	/***************************************************************************
		Dichiaro localmete la struttura contenente le variabili
		di configurazione e la riempio invocando il parser
	***************************************************************************/
	struct config config_vars = {
		.nomi = {
			/* 0 */ [UNIXPATH] = "UnixPath",
			/* 1 */ [STAT_FILE_NAME] = "StatFileName",
			/* 2 */ [MAX_CONNECTIONS] = "MaxConnections",
			/* 3 */ [THREADS_IN_POOL] = "ThreadsInPool",
			/* 4 */ [STORAGE_SIZE] = "StorageSize",
			/* 5 */ [STORAGE_BYTE_SIZE] = "StorageByteSize",
			/* 6 */ [MAX_OBJ_SIZE] = "MaxObjSize",
			/* 7 */ [THE_END] = NULL
		},
		.valori = {
			/* 0 */ [UNIXPATH] = (char*) alloca(M*sizeof(char)),
			/* 1 */ [STAT_FILE_NAME] = (char*) alloca(M*sizeof(char)),
			/* 2->6 */ [MAX_CONNECTIONS ... MAX_OBJ_SIZE] = (char*)(long) 0,
			/* 7 */ [THE_END] = NULL
		}
	};
	struct config *extra = parser(&config_vars, MY_FILE);
	SERVER_SOCK_PATH = config_vars.valori[UNIXPATH];
	fout = (FILE*)config_vars.valori[STAT_FILE_NAME];


	/***************************************************************************
		Installo un handler per le azioni finali
		da fare prima della terminazione del thread
		e richiamo la macro inizializza per inizializzare le tre variabili
		globali utili alla libreria read_write
	***************************************************************************/
	void **clean_args[4] = {
		[0] = (void*) MY_FILE,
		[1] = (void*) SERVER_SOCK_PATH,
		[2] = (void*) extra,
		[3] = (void*) &fout
	};
	pthread_cleanup_push(spazzino, (void*)clean_args);

	#ifdef __OP__
	initialize(
		(int)(long)config_vars.valori[STORAGE_BYTE_SIZE],
		(int)(long)config_vars.valori[STORAGE_SIZE],
		(int)(long)config_vars.valori[MAX_OBJ_SIZE]
	);
	#endif


	/***************************************************************************
		Setto la maschera per inibire i segnali
		INT, QUIT, TERM, PIPE, USR1, USR2
	***************************************************************************/
	sigemptyset(&block_mask);
	sigaddset(&block_mask, SIGINT);
	sigaddset(&block_mask, SIGQUIT);
	sigaddset(&block_mask, SIGTERM);
	sigaddset(&block_mask, SIGPIPE);
	sigaddset(&block_mask, SIGUSR1);
	sigaddset(&block_mask, SIGUSR2);
	pthread_sigmask(SIG_SETMASK, &block_mask, NULL);


	/***************************************************************************
		Apro il file per il dump delle statistiche
		e creo gli argomenti per il thread-gestore dei segnali...
	***************************************************************************/
	if( !(fout=fopen(config_vars.valori[STAT_FILE_NAME], "a+")) )
		err_exit("fopen %s fallita", config_vars.valori[STAT_FILE_NAME]);
	void **args[3] = {
		[0] = (void*)(&sfd),
		[1] = (void*)(fout),
		[2] = (void*)(&block_mask)
	};


	/***************************************************************************
		...creo il gestore per i suddetti segnali
	***************************************************************************/
	if( ris=pthread_create(&tid_gestore, NULL,
						   &main_gestore, (void*)args) ){
		setenv("EF_DUMPCORE", "dump", 1);
		err_exit_en(ris, "pthread_create fallita sul gestore dei segnali");
	}


	/***************************************************************************
		Inizializzo le variabili esterne definite in membox.h
	***************************************************************************/
	int tip = (int)(long)config_vars.valori[THREADS_IN_POOL];
	int mcn = (int)(long)config_vars.valori[MAX_CONNECTIONS];

	coda_cfd = new_queue( mcn<=tip ? mcn : mcn-tip );
	array_lavoratori = malloc( sizeof(lavoratore_t)*tip);
	if( !coda_cfd || !array_lavoratori )
		err_exit("malloc strutture fallita");


	/***************************************************************************
		Rimuovo una eventuale vecchia socket,
		ne creo una nuova e la inizializzo
	***************************************************************************/
	if( remove(SERVER_SOCK_PATH)<0 && errno==ENOENT)
		err_msg("%s %s %s", "rimozione", SERVER_SOCK_PATH, "non riuscita");
	if( (sfd=socket(AF_UNIX, SOCK_STREAM, 0))<0 ){
		setenv("EF_DUMPCORE", "dump", 1);
		err_exit("socket non creata");
	}
	memset( &addr, 0, sizeof(struct sockaddr_un) );
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, SERVER_SOCK_PATH, sizeof(addr.sun_path)-1);


	/***************************************************************************
		Faccio bind e listen
	***************************************************************************/
	if( bind(sfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un))<0 )
		err_exit("bind fallita");
	if( listen(sfd, BACKLOG)<0 )
		err_exit("listen fallita");
	fprintf(stdout, "%c[%d;%dmSERVER IN ASCOLTO...%c[%dm\n",
			0x1B, 1, 34, 0x1B, 0);


	/***************************************************************************
		Alloco ed inizializzo il repository...
	***************************************************************************/
	INIZIALIZZA_REPO(repo, icl_hash_create, icl_hash_destroy, icl_hash_find,
					icl_hash_insert, icl_hash_update_insert, icl_hash_delete,
					hashing, compare_key, free_key, free_data);

	int st = (int)(long)config_vars.valori[STORAGE_SIZE];
	repo.archivio =
				!st ? (repo.crea_repo)(1024, repo.funzione_hash, repo.compara)
					: (repo.crea_repo)(st, repo.funzione_hash, repo.compara);


	/***************************************************************************
		Creo i lavoratori...
	***************************************************************************/
	for(k=n=tip; n--;){
		array_lavoratori[n].acceso = 0;
		ris = pthread_create( &array_lavoratori[n].tid, NULL,
							  &main_lavoratore, (void*)(long)n );
		if( ris ){
			if( !--k ){
				setenv("EF_DUMPCORE", "dump", 1);
				err_exit_en(ris,"pthread_create fallita su tutti i lavoratori");
			}
			err_msg("pthread_create fallita sul lavoratore %d", n);
		}
	}


	/***************************************************************************
		...faccio da arbitro,
		controllando ogni ciclo se è arrivato un segnale...
	***************************************************************************/
	for(;;){
		/* :::::::::: Lock :::::::::: */
		if( ris=pthread_mutex_lock(&lock) )
			err_exit_en(ris, "lock fallita");

			if(stop_sigint || stop_sigusr2){
				/* :::::::::: Unlock :::::::::: */
				if( ris=pthread_mutex_unlock(&lock) )
					err_exit_en(ris, "unlock fallita");
				break;
			}

		/* :::::::::: Unlock :::::::::: */
		if( ris=pthread_mutex_unlock(&lock) )
			err_exit_en(ris, "unlock fallita");


		/***********************************************************************
			...mi metto in ascolto sulla accept
			(in caso la accept fallisca e troppi file sono aperti,
			abortisco - la situazione è critica)
		***********************************************************************/
		if( (cfd=accept(sfd, NULL, NULL))<0 ){
			err_msg("accept fallita");
			if(errno==EMFILE){
				setenv("EF_DUMPCORE", "dump", 1);
				err_exit("troppe socket aperte");
			}
			continue;
		}
		fprintf(stdout, "%c[%d;%dmACCEPT EFFETTUATA (CFD=%d)%c[%dm\n",
				0x1B, 1, 34, cfd, 0x1B, 0);


		/* :::::::::: Lock :::::::::: */
		if( ris=pthread_mutex_lock(&lock) )
			err_exit_en(ris, "lock fallita");

			if( (mcn==tip && operativi==tip) || !insert(coda_cfd, cfd, FALSE) ){
				op_t op = OP_FAIL;
				membox_key_t key = 0;
				
				for(ur=sizeof(int), ptr=&op;
						(wr=write(cfd, ptr, ur))<ur; ur-=wr, ptr+=wr)
					if(wr<=0) err_exit("errore in scrittura");
				
				for(ur=sizeof(unsigned long), ptr=&key;
						(wr=write(cfd, ptr, ur))<ur; ur-=wr, ptr+=wr)
					if(wr<=0) err_exit("errore in scrittura");
				
				if( (ris=close(cfd))<0 )
					err_exit_en(ris, "close fallita");
				/* :::::::::: Unlock :::::::::: */
				if( ris=pthread_mutex_unlock(&lock) )
					err_exit_en(ris, "unlock fallita");
				continue;
			}
			else pthread_cond_signal(&dormi);

		/* :::::::::: Unlock :::::::::: */
		if( ris=pthread_mutex_unlock(&lock) )
			err_exit_en(ris, "unlock fallita");
	}
	if( pthread_cond_broadcast(&dormi) )
		err_msg("pthread_cond_broadcast fallita\n");


	/***************************************************************************
		...poi li aspetto, assieme al gestore
		e lancio l'hadler-pulitore
	***************************************************************************/
	for(n=tip; n--;)
		if( pthread_join(array_lavoratori[n].tid, &array_lavoratori[n].retval) )
			err_msg("pthread_join fallita su lavoratore numero %d", n);
	if( pthread_join(tid_gestore, &retval_gestore) )
		err_msg("pthread_join fallita sul gestore");

	fprintf(stdout,
		"%c[%d;%dmTUTTI I THREAD DI MEMBOX SONO TERMINATI%c[%dm\n",
		0x1B, 1, 31, 0x1B, 0);
	pthread_cleanup_pop(1);
	return 0;
}


/*
	Funzione di start del thread gestore dei segnali
	che si occuperà di rimanere in ascolto dei segnali
	presenti nella maschera passata per argomento.
*/
static void*
main_gestore(void *args)
{
	void **_args = (void**)args;
	int *sfd = (int*)_args[0], ris;
	FILE *fout = (FILE*)_args[1];
	sigset_t *wait_mask = (sigset_t*)_args[2];
	siginfo_t sig_info;


	for(;;){
		/***********************************************************************
			Adesso aspetto uno dei segnali presenti in wait_mask
			e li gestisco come da consegna
		***********************************************************************/
		sigwaitinfo(wait_mask, &sig_info);
		switch( sig_info.si_signo ){
			case SIGINT:
			case SIGQUIT:
			case SIGTERM:
				fprintf(stdout, "%c[%d;%dm::: RICEVUTO SEGNALE :::%c[%dm\n",
						0x1B, 1, 31, 0x1B, 0);
				INFO_SEGNALE(stderr, sig_info);

				if( ris=pthread_mutex_lock(&lock) )
					err_exit_en(ris, "lock fallita");
					stop_sigint = 1;
					mboxStats.concurrent_connections =
									operativi + coda_cfd->num_el;
				if( ris=pthread_mutex_unlock(&lock) )
					err_exit_en(ris, "unlock fallita");
				
				if( (ris=shutdown(*sfd, SHUT_RDWR))<0 )
					err_exit_en(ris, "shutdown fallita");

				printStats(fout);
				return (void *)0;


			case SIGUSR2:
				fprintf(stdout, "%c[%d;%dm::: RICEVUTO SEGNALE :::%c[%dm\n",
						0x1B, 1, 31, 0x1B, 0);
				INFO_SEGNALE(stderr, sig_info);
				
				if( ris=pthread_mutex_lock(&lock) )
					err_exit_en(ris, "lock fallita");
					stop_sigusr2 = 1;
					mboxStats.concurrent_connections =
									operativi + coda_cfd->num_el;
				if( ris=pthread_mutex_unlock(&lock) )
					err_exit_en(ris, "unlock fallita");
				if( (ris=shutdown(*sfd, SHUT_RDWR))<0 )
					err_exit_en(ris, "shutdown fallita");

				printStats(fout);
				return (void *)0;


			case SIGUSR1:
				fprintf(stdout, "%c[%d;%dm::: RICEVUTO SEGNALE :::%c[%dm\n",
						0x1B, 1, 31, 0x1B, 0);
				INFO_SEGNALE(stderr, sig_info);

				if( ris=pthread_mutex_lock(&lock) )
					err_exit_en(ris, "lock fallita");
					mboxStats.concurrent_connections =
									operativi + coda_cfd->num_el;
				if( ris=pthread_mutex_unlock(&lock) )
					err_exit_en(ris, "unlock fallita");

				printStats(fout);
				break;


			default:
				break;
		}
	}
}


/*
	Funzione di start dei thread-lavoratore che si occuperanno di estrarre
	le connessioni dalla coda_cfd ed eseguire le operazioni richieste
	dal cliente in ascolto sulla socket.
*/
static void*
main_lavoratore(void *args)
{
	int index = (int)(long)args, cfd, ris;
	/***************************************************************************
		All'inizio del ciclo lavoratore estraggo dalla coda,
		dopo aver controllato se è arrivato un SIGINT o un SIGUSR2
		guardando le due variabili globali stop_sigint e stop_sigusr2.
		Nel mentre incremento e decremento la variabile globale
		contatore dei lavoratori attivi.
	***************************************************************************/
	for(;;){
		/* :::::::::: Lock :::::::::: */
		if( ris=pthread_mutex_lock(&lock) )
			err_exit_en(ris, "lock fallita");

			if( array_lavoratori[index].acceso ){
				array_lavoratori[index].acceso = 0;
				operativi--;
			}
			for(;;){
				if(stop_sigint || stop_sigusr2){
					/* :::::::::: Unlock :::::::::: */
					if( ris=pthread_mutex_unlock(&lock) )
						err_exit_en(ris, "unlock fallita");
					return (void*)0;
				}
				if( (cfd=extract(coda_cfd))>=0 ) break;
				else pthread_cond_wait(&dormi, &lock);
			}
			array_lavoratori[index].acceso = 1;
			operativi++;

		/* :::::::::: Unlock :::::::::: */
		if( ris=pthread_mutex_unlock(&lock) )
			err_exit_en(ris, "unlock fallita");


		/***********************************************************************
			Adesso che ho estratto un cfd,
			inizio a risolvere le richieste del cliente
			(prima di ogni richiesta controllo se è arrivato un SIGINT)
		***********************************************************************/
		for(;;){
			/* :::::::::: Lock :::::::::: */
			if( ris=pthread_mutex_lock(&lock) )
				err_exit_en(ris, "lock fallita");

				if(stop_sigint){
					array_lavoratori[index].acceso = 0;
					operativi--;
					/* :::::::::: Unlock :::::::::: */
					if( ris=pthread_mutex_unlock(&lock) )
						err_exit_en(ris, "unlock fallita");
					return (void*)0;
				}

			/* :::::::::: Unlock :::::::::: */
			if( ris=pthread_mutex_unlock(&lock) )
				err_exit_en(ris, "unlock fallita");

			fprintf(stdout,
					"%c[%d;%dmLAVORATORE %d (TID=%ld) OPERATIVO%c[%dm\n",
					0x1B, 1, 32, index, pthread_self(), 0x1B, 0);


			/*******************************************************************
				Posso adesso iniziare a leggere le operazioni scritte da
				cliente nella socket:
				leggo un message_hdr_t (request) e se la lettura è fallita,
				il cliente non ha più operazioni da chiedere;
				chiudo quindi la socket, mi assicuro che il cliente non
				abbia lasciato bloccato il repository
				e torno nel for(;;) esterno ad estrarre una nuova connessione
			*******************************************************************/
			message_hdr_t request;
			message_data_t *data;
			unsigned long *kkk;

			if( readHeader(cfd, &request)<0){
				if( (ris=close(cfd))<0 )
					err_exit_en(ris, "close fallita");
				unlockRepo(index);
				break;
			}
			if( !(data=malloc(sizeof(message_data_t))) ||
				!(kkk=malloc(sizeof(unsigned long))) ){
				err_msg("malloc risposta fallita");
				risposta( cfd, request.op, OP_FAIL, -1, 0, NULL );
				if(data) free(data);
				continue;
			}
			*kkk = request.key;

			int rest_1 = -1;
			void *rest_2 = NULL,
				 *rest_3 = NULL;
			int do_check[3];

			switch( request.op ){
				/***************************************************************
					E' richiesta una PUT_OP:
					leggo un message_data_t (*data), chiedo una startWrite,
					inserisco in repo il data ed invoco risposta per comunicare
					al cliente l'esito dell'operazione.
				***************************************************************/
				case PUT_OP:
					if( readData(cfd, data)==-1 ){
						risposta( cfd, PUT_OP, OP_FAIL, -1, 0, NULL );
						free(kkk); free(data);
						break;
					}
					do_check[OPERATION] = PUT_OP;
					do_check[VECCHIO] = 0;
					do_check[NUOVO] = data->len;
					if( ris=startWrite(index, do_check) ){
						risposta( cfd, PUT_OP, ris, -1, 0, NULL );
						free(kkk); (repo.libera_data)(data);
						break;
					}
					rest_2 = (repo.inserisci)(repo.archivio, kkk, data);
					doneWrite();
					if( !rest_2 ){
						risposta(cfd, PUT_OP, OP_PUT_ALREADY, -1, 0, NULL);
						free(kkk); (repo.libera_data)(data);
						break;
					}
					risposta(cfd, PUT_OP, OP_OK, -1, 0, NULL);
					break;


				/***************************************************************
					E' richiesta una UPDATE_OP:
					leggo un message_data_t (*data), chiedo una startRead e
					mi assicuro che l'oggetto da aggiornare sia presente
					in repo; poi chiedo una startWrite, aggiorno l'oggetto ed
					invoco risposta per comunicare al cliente
					l'esito dell'operazione.
				***************************************************************/
				case UPDATE_OP:
					if( readData(cfd, data)==-1 ){
						risposta( cfd, UPDATE_OP, OP_FAIL, -1, 0, NULL );
						free(kkk); free(data);
						break;
					}
					if( ris=startRead(index) ){
						risposta(cfd, UPDATE_OP, ris, -1, 0, NULL);
						free(kkk); (repo.libera_data)(data);
						break;
					}
					rest_3 = (repo.cerca)(repo.archivio, kkk);
					doneRead();
					if( !rest_3 ){
						risposta(cfd, UPDATE_OP, OP_UPDATE_NONE, -1, 0, NULL);
						free(kkk); (repo.libera_data)(data);
						break;
					}
					do_check[OPERATION] = UPDATE_OP;
					do_check[VECCHIO] = ((message_data_t*)rest_3)->len;
					do_check[NUOVO] = data->len;
					if( ris=startWrite(index, do_check) ){
						risposta( cfd, UPDATE_OP, ris, -1, 0, NULL );
						free(kkk); (repo.libera_data)(data);
						break;
					}
					void *aux = NULL;
					rest_2 = (repo.aggiorna)(repo.archivio, kkk, data, &aux);
					doneWrite();
					risposta( cfd, UPDATE_OP,
								rest_2 ? OP_OK : OP_UPDATE_NONE, -1, 0, NULL );
					if(aux) (repo.libera_data)(aux);
					break;


				/***************************************************************
					E' richiesta una GET_OP:
					chiedo una startRead, cerco l'oggetto associato alla chiave
					passatami ed invoco risposta per comunicare
					al cliente l'esito dell'operazione e scrivere nella socket
					l'oggetto trovato
				***************************************************************/
				case GET_OP:
					if( ris=startRead(index) ){
						risposta(cfd, GET_OP, ris, -1, 0, NULL);
						free(kkk); free(data);
						break;
					}
					rest_3 = (repo.cerca)(repo.archivio, kkk);
					doneRead();
					risposta( cfd, GET_OP,
							rest_3 ? OP_OK : OP_GET_NONE, -1,
							rest_3 ? ((message_data_t*)rest_3)->len : 0,
							rest_3 ? ((message_data_t*)rest_3)->buf : NULL );
					free(kkk); free(data);
					break;


				/***************************************************************
					E' richiesta una REMOVE_OP:
					chiedo una startRead e mi assicuro che l'oggetto da
					rimuovere sia presente in repo; poi chiedo una startWrite,
					rimuovo l'oggetto ed invoco risposta per comunicare
					al cliente l'esito dell'operazione
				***************************************************************/
				case REMOVE_OP:
					if( ris=startRead(index) ){
						risposta(cfd, REMOVE_OP, ris, -1, 0, NULL);
						free(kkk); free(data);
						break;
					}
					rest_3 = (repo.cerca)(repo.archivio, kkk);
					doneRead();
					if( !rest_3 ){
						risposta(cfd, REMOVE_OP, OP_REMOVE_NONE, -1, 0, NULL);
						free(kkk); free(data);
						break;
					}
					do_check[OPERATION] = REMOVE_OP;
					do_check[VECCHIO] = ((message_data_t*)rest_3)->len;
					do_check[NUOVO] = 0;
					if( ris=startWrite(index, do_check) ){
						risposta(cfd, REMOVE_OP, ris, -1, 0, NULL);
						free(kkk); free(data);
						break;
					}
					rest_1 = (repo.cancella)(repo.archivio, kkk,
										repo.libera_key, repo.libera_data);
					doneWrite();
					risposta(cfd, REMOVE_OP,
								rest_1 ? OP_REMOVE_NONE : OP_OK, -1, 0, NULL);
					free(kkk); free(data);
					break;


				/***************************************************************
					E' richiesta una LOCK_OP:
					chiedo una lockRepo e comunico al cliente (con risposta)
					se è possibile bloccare il repository
				***************************************************************/
				case LOCK_OP:
					risposta(cfd, LOCK_OP, lockRepo(index), -1, 0, NULL);
					free(kkk); free(data);
					break;


				/***************************************************************
					E' richiesta una UNLOCK_OP:
					chiedo una unlockRepo e comunico al cliente (con risposta)
					l'esito dell'operazione
				***************************************************************/
				case UNLOCK_OP:
					ris = unlockRepo(index);
					risposta(cfd, ris==OP_LOCK_NONE ? LOCK_OP : UNLOCK_OP,
								ris, -1, 0, NULL);
					free(kkk); free(data);
					break;
			}
		}
	}
}


/*
	Funzione ausiliaria per il calcolo del valore hash
*/
static inline unsigned int
FNV1_hash(void *key, int len)
{
	unsigned char *p = (unsigned char*)key;
	unsigned int h = 2166136261u; int i;
	for(i=0; i<len; i++) h=(h*16777619)^p[i];
	return h;
}


/*
	Funzione hash
*/
static inline unsigned int
hashing(void *data)
{
	return FNV1_hash(data, sizeof(unsigned long));
}


/*
	Funzione di comparazione tra due chiavi
*/
static inline int
compare_key(void *first_key, void *second_key)
{	
	return ( *(unsigned long*)first_key == *(unsigned long*)second_key );
}


/*
	Funzione preposta a liberare una chiave
*/
static inline void
free_key(void *key)
{
	if( key ){
		membox_key_t *tmp = (membox_key_t*)key;
		free( tmp );
	}
}


/*
	Funzione preposta a liberare una struttura data
*/
static inline void
free_data(void *data)
{
	if( data ){
		message_data_t *tmp = (message_data_t*)data;
		if(tmp->buf) free(tmp->buf);
		if(tmp) free(tmp);
	}
}