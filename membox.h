/* membox.h by Giorgi Matteo, 517183 & Quarta Andrea, 517881 */


/*
	In caso non sia stata già inclusa la libreria degli errori,
	la si include qui
*/
#ifndef ERROR_H
#include "errors.h"
#endif

#ifndef __BOOL__
#define __BOOL__
typedef enum { FALSE, TRUE } bool;
#endif

#define BACKLOG 6
#define N 8
#define M 50


/*******************************************************************************
	Macro utile a stampare su un file FD
	le informazioni relasive al segnale contenute in SIG_INFO
*******************************************************************************/
#define INFO_SEGNALE(FD, SIG_INFO)											\
	do{																		\
		fprintf((FD), "Signal number: %d\n", (SIG_INFO).si_signo);			\
		fprintf((FD), "An errno value: %d\n", (SIG_INFO).si_errno);			\
		fprintf((FD), "Signal code: %d\n", (SIG_INFO).si_code);				\
		fprintf((FD), "Sending process ID: %d\n", (SIG_INFO).si_pid);		\
		fprintf((FD), "Real user ID of sending process: %d\n",				\
				(SIG_INFO).si_uid);											\
		fprintf((FD), "Exit value or signal: %d\n", (SIG_INFO).si_status);	\
		fprintf((FD), "User time consumed: %ld\n", (SIG_INFO).si_utime);	\
		fprintf((FD), "System time consumed: %ld\n", (SIG_INFO).si_stime);	\
		fprintf((FD), "File descriptor: %d\n", (SIG_INFO).si_fd);			\
	} while(0)


/*******************************************************************************
	Struttura lavoratore:
	tid    --> identificatore
	retval --> valore di ritorno al padre
	acceso --> flag indicante lo stato del lavoratore (acceso/spento)
*******************************************************************************/
typedef struct{
	pthread_t tid;
	void *retval;
	bool acceso;
} lavoratore_t;


/*******************************************************************************
	Struttura contenente le variabili di configurazione
	nomi[]   --> array dei nomi delle variabili di configurazione
	valori[] --> array dei valori delle variabili di configurtazione
*******************************************************************************/
struct config{
	char *nomi[N];
	char *valori[N];
};


/*******************************************************************************
	Enumerazione delle variabili di configurazione
	(utile per scorrere negli array delle struct config)
*******************************************************************************/
typedef enum{
	UNIXPATH,
	STAT_FILE_NAME,
	MAX_CONNECTIONS,
	THREADS_IN_POOL,
	STORAGE_SIZE,
	STORAGE_BYTE_SIZE,
	MAX_OBJ_SIZE,
	THE_END
} pars_vars;


/*******************************************************************************
	Variabili globali necessarie per la mutua esclusione
	e la comunicazione tra thread:
	mboxStats        --> struttura variabili per statistiche
	lock             --> mutex per la coda delle connessioni, mboxStats,
							stop_sigint, stop_sigusr2, coda_cfd, operativi
	dormi            --> cond-variable dove si sospendono i lavoratori
	stop_sigint      --> flag per il controllo di SIGINT
	stop_sigusr2     --> flag per il controllo di SIGUSR2
	coda_cfd         --> coda file descriptor peer socket create dalla accept
	array_lavoratori --> array pool dei lavoratori
	operativi        --> numero dei lavoratori attivi
*******************************************************************************/
struct statistics mboxStats = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t dormi = PTHREAD_COND_INITIALIZER;

volatile sig_atomic_t stop_sigint = 0;
volatile sig_atomic_t stop_sigusr2 = 0;

static queue_t *coda_cfd;
static lavoratore_t *array_lavoratori;
static int operativi = 0;


/*******************************************************************************
	Struttura Repository contenente tutte le funzioni utili al suo utilizzo:
	archivio       --> puntatore al repository
	crea_repo      --> funzione per creare il repository
	distruggi_repo --> funzione per liberare il repository
	cerca          --> funzione per cercare elemento nel repository
	inserisci      --> funzione per inserire elemento nel repository
	aggiorna       --> funzione per aggiornare elemento nel repository
	cancella       --> funzione per cancellare elemento nel repository
	funzione_hash  --> funzione per generare valore hash
	compara        --> funzione per comparare due chiavi 
	libera_key     --> funzione per liberare una chiave
	libera_data    --> funzione per liberare un data
*******************************************************************************/
struct repository{
	void *archivio;
	void* (*crea_repo) (int, unsigned int (*funzione_hash) (void*),
						int (*compara) (void*, void*));
	int (*distruggi_repo) (void*, void (*libera_key)(void*),
							void (*libera_data)(void*));
	void* (*cerca) (void*, void*);
	void* (*inserisci) (void*, void*, void*);
	void* (*aggiorna) (void*, void*, void*, void**);
	int (*cancella) (void*, void*, void (*libera_key) (void*),
						void (*libera_data) (void*));
	unsigned int (*funzione_hash) (void*);
	int (*compara) (void*, void*);
	void (*libera_key)(void*);
	void (*libera_data)(void*);
} repo;


/*******************************************************************************
	Macro per inizializzare la struct repository
*******************************************************************************/
#define INIZIALIZZA_REPO(REPO, CREA_REPO, DISTRUGGI_REPO, CERCA, INSERISCI,	\
		AGGIORNA, CANCELLA, FUNZIONE_HASH, COMPARA, LIBERA_KEY, LIBERA_DATA)\
	do{																		\
		(REPO).crea_repo =													\
  (void* (*)(int, unsigned int (*)(void*), int (*)(void*, void*))) (CREA_REPO);\
  																			\
		(REPO).distruggi_repo =												\
	(int (*)(void*, void (*)(void*), void (*)(void*))) (DISTRUGGI_REPO);	\
																			\
		(REPO).cerca = (void* (*)(void*, void*)) (CERCA);					\
																			\
		(REPO).inserisci = (void* (*)(void*, void*, void*)) (INSERISCI);	\
																			\
		(REPO).aggiorna = (void* (*)(void*, void*, void*, void**)) (AGGIORNA);\
																			\
		(REPO).cancella =													\
	(int (*)(void*, void*, void (*)(void*), void (*)(void*))) (CANCELLA);	\
																			\
		(REPO).funzione_hash = (unsigned int (*) (void*)) FUNZIONE_HASH;	\
																			\
		(REPO).compara = (int (*) (void*, void*)) COMPARA;					\
																			\
		(REPO).libera_key = (void (*)(void*)) LIBERA_KEY;					\
																			\
		(REPO).libera_data = (void (*)(void*)) LIBERA_DATA;					\
	} while(0)


/*******************************************************************************
	Funzione per la pulizia della memoria da installare con
	pthread_cleanup_push ed invocare con pthread_cleanup_pop
*******************************************************************************/
static inline void
spazzino(void* _args)
{
	void **args = (void**)_args;
	char *MY_FILE = (char*)args[0],
		 *SERVER_SOCK_PATH = (char*)args[1];
	struct config *extra = (struct config*)args[2];
	FILE **fout = (FILE**)args[3];

	/* Se presente, rimuovo la socket dalla directory,
	   poi libero MY_FILE (allocato con strdup) */
	if( remove(SERVER_SOCK_PATH)<0 && errno==ENOENT )
		err_msg("%s %s %s", "rimozione", SERVER_SOCK_PATH, "non riuscita");
	if( MY_FILE ) free(MY_FILE);

	/* Se allocati, libero array e coda */
	if( array_lavoratori )
		free(array_lavoratori);
	if( coda_cfd ){
		free(coda_cfd->q);
		free(coda_cfd);
	}

	/* Se allocato, libero struttura variabili extra */
	int n;
	if(extra){
		for(n=0; extra->nomi[n]; free(extra->nomi[n]), n++ );
		for(n=0; extra->valori[n]; free(extra->valori[n]), n++ );
		free(extra);
	}

	/* Se allocato, libero il repo */
	if( repo.archivio &&
			repo.distruggi_repo && repo.libera_key && repo.libera_data )
		(repo.distruggi_repo)(repo.archivio, repo.libera_key, repo.libera_data);

	/* Se interrotto con SIGUSR2, stampo le statistiche e chudo il file */
	#ifdef MEMBOX_STATS_
	if( *fout && (stop_sigusr2 || stop_sigint) ) printStats(*fout);
	#endif
	if( *fout && fclose(*fout) )
		err_msg("chiusura file statistaiche fallita");
}


/*******************************************************************************
	Funzione per il parsing del file di configurazione
*******************************************************************************/
static inline struct config*
parser(struct config *config_vars, char *MY_FILE)
{
	char *buf=NULL, *_buf, *save=NULL, *aux, *last;
	struct config *extra = NULL;
	size_t nbytes; int i, iExtra=0;
	FILE *fd;

	if( !(fd=fopen(MY_FILE, "r")) )
		err_exit("%s non esiste", MY_FILE);

	LABEL:
	/* Leggo una riga */
	for(; getline(&buf, &nbytes, fd)>0;){

		/* Tolgo gli spazi anteriori */
		for(_buf=buf; ; _buf++){
			if( !strlen(_buf) || *_buf=='#' ) goto LABEL;
			if( !isspace(*_buf) ) break;
		}

		/* Taglio il commento finale e controllo che ci sia un solo =,
		   poi controllo che il nome sia ben formattato */
		strtok(_buf, "#");
		if( !(save=strstr(_buf, "=")) || strstr(++save, "=") )
			err_exit("errato numero di '=' in \"%s\"", _buf);
		if(_buf == --save)
			err_exit("\"%s\" malformattato", _buf);

		/* Metto il terminatore alla fine del nome */
		for(aux=save, *save='\0'; _buf<aux && isspace(*--aux); *aux='\0');

		/* Controllo che il nome sia conusciuto,
		   altrimenti lo salvo come extra */
		for( i=0; config_vars->nomi[i] &&
				  strcmp(_buf, config_vars->nomi[i]); i++ );
		if( !config_vars->nomi[i] && iExtra<N ){
			if( !iExtra ){
				if( !(extra=malloc(sizeof(struct config))) ){
					err_msg("malloc fallita su nomi extra");
					continue;
				}
				memset(extra->nomi, 0, sizeof(extra->nomi));
				memset(extra->valori, 0, sizeof(extra->valori));
			}
			if( !(extra->nomi[iExtra]=malloc(M*sizeof(char))) ||
				!(extra->valori[iExtra]=malloc(M*sizeof(char))) ){
				if(extra->nomi[iExtra]) free(extra->nomi[iExtra]);
				err_msg("malloc fallita su %s", _buf);
				continue;
			}
			memset(extra->nomi[iExtra], '\0', M*sizeof(char));
			memset(extra->valori[iExtra], '\0', M*sizeof(char));
			strcpy(extra->nomi[iExtra], _buf);
			fprintf(stdout, "\"%s\" non trovato, aggiunto come extra\n", _buf);
		}

		/* Dal valore tolgo gli spazi in testa e in coda */
		for(; isspace(*++save););
		for(last=save+strlen(save); isspace(*--last); *last='\0');

		/* Adesso assegno il valore alla variabile */
		if( !config_vars->nomi[i] ) strcpy(extra->valori[iExtra++], save);
		else config_vars->valori[i] =
				i<=STAT_FILE_NAME ? strcpy(config_vars->valori[i], save)
								  : (void*)(long) atoi(save);
	}

	free(buf);
	fclose(fd);
	return extra;
}


/*******************************************************************************
	Funzione per la risposta.
	Verrà invocata dal lavoratore per scrivere nella peer-socket
	il messaggio di risposta al cliente
	e per aggiornare alcune delle variabili presenti in mboxStats 
*******************************************************************************/
static inline void
risposta(int cfd, op_t op, op_t answer, membox_key_t key,
		 unsigned int len, char *buf)
{
	int ris; void *ptr;
	ssize_t wr, ur;
	bool esito = answer==OP_OK ? TRUE : FALSE;
	key = key==-1 ? 0 : key;

	/* scrivo l'esito dell'operazione */
	for(ur=sizeof(int), ptr=&answer;
			(wr=write(cfd, ptr, ur))<ur; ur-=wr, ptr+=wr)
		if(wr<=0) err_exit("errore in scrittura");

	/* scrivo la chiave */
	for(ur=sizeof(unsigned long), ptr=&key;
			(wr=write(cfd, ptr, ur))<ur; ur-=wr, ptr+=wr)
		if(wr<=0) err_exit("errore in scrittura");

	/* :::::::::: Lock :::::::::: */
	if( ris=pthread_mutex_lock(&lock) )
		err_exit_en(ris, "lock fallita");

		switch( op ){
			/* rispondo ad una PUT_OP:
			   aggiorno nput, nput_failed, current_objects, max_objects,
			   current_size, max_size */
			case PUT_OP:
				mboxStats.nput += esito;
				mboxStats.nput_failed += !esito;

				mboxStats.current_objects += esito;
				mboxStats.max_objects =
					mboxStats.current_objects>mboxStats.max_objects
					? mboxStats.current_objects
					: mboxStats.max_objects;

				mboxStats.current_size += esito*len;
				mboxStats.max_size =
					mboxStats.current_size>mboxStats.max_size
					? mboxStats.current_size
					: mboxStats.max_size;
				break;

			/* rispondo ad una UPDATE_OP:
			   aggiorno nupdate, nupdate_failed */
			case UPDATE_OP:
				mboxStats.nupdate += esito;
				mboxStats.nupdate_failed += !esito;
				break;

			/* rispondo ad una GET_OP:
			   aggiorno nget, nget_failed ed invio e scrivo un data
			   nella peer-socket */
			case GET_OP:
				mboxStats.nget += esito;
				mboxStats.nget_failed += !esito;
				if( esito ){
					/* scrivo la lunghezza del buffer */
					for(ur=sizeof(unsigned int), ptr=&len;
							(wr=write(cfd, ptr, ur))<ur; ur-=wr, ptr+=wr)
						if(wr<=0) err_exit("errore in scrittura");

					/* scrivo il buffer */
					for(; (ris=write(cfd, buf, len))<len; len-=ris, buf+=ris)
						if(ris<=0) err_exit("errore in scrittura");
				}
				break;

			/* rispondo ad una REMOVE_OP:
			   aggiorno nremove, nremove_failed,
			   current_objects, current_size */
			case REMOVE_OP:
				mboxStats.nremove += esito;
				mboxStats.nremove_failed += !esito;

				mboxStats.current_objects -= esito;
				mboxStats.current_size -= esito*len;
				break;

			/* rispondo ad una LOCK_OP:
			   aggiorno nlock, nlock_failed */
			case LOCK_OP:
				mboxStats.nlock += esito;
				mboxStats.nlock_failed += !esito;
				break;
		}

	/* :::::::::: Unlock :::::::::: */
	if( ris=pthread_mutex_unlock(&lock) )
		err_exit_en(ris, "unlock fallita");
}