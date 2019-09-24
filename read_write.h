/* read_write.h by Giorgi Matteo, 517183 & Quarta Andrea, 517881 */


#ifndef __RW__
#define __RW__

#ifndef __BOOL__
#define __BOOL__
typedef enum { FALSE, TRUE } bool;
#endif


/*******************************************************************************
	Struttura contenente le variabili utili all'accesso
	nella struttura condivisa dei thread:
	waitingReaders --> numero lettori in attesa
	activeReaders  --> numero lettori attivi
	waitingWriters --> numero scrittori in attesa
	activeWriters  --> numero scrittori attivi
	waitingBlocker --> numero bloccatori in attesa
	activeBlocker  --> indice del bloccatore attivo
							(-1 se non ci sono bloccatori attivi)
*******************************************************************************/
static struct variabili{
	int waitingReaders;
	int activeReaders;
	int waitingWriters;
	int activeWriters;
	int waitingBlocker;
	int activeBlocker;
} vars = {
	.waitingReaders = 0,
	.activeReaders = 0,
	.waitingWriters = 0,
	.activeWriters = 0,
	.waitingBlocker = 0,
	.activeBlocker = -1
};


/*******************************************************************************
	Variabili globali necassarie per la mutua esclusione:
	mux     --> mutex per la struct variabili
	readGo  --> cond-variable dove si sospendono i lettori
	writeGo --> cond-variable dove si sospendono gli scrittori
	lockGo  --> cond-variable dove si sospendono i bloccatori
*******************************************************************************/
static pthread_mutex_t mux = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t readGo = PTHREAD_COND_INITIALIZER;
static pthread_cond_t writeGo = PTHREAD_COND_INITIALIZER;
static pthread_cond_t lockGo = PTHREAD_COND_INITIALIZER;


/*******************************************************************************
	Se definita __OP__ includo ops.h (per poter usare le operazioni),
	definisco una check per il controllo della validità dell'operazione,
	tre variabili di disponibilita per la check e una macro che le inizializza
	byteDisponibili      --> spazio disponibile nel repository
	posizioniDisponibili --> posizioni disponibile nel repository
	grandezzaMassima     --> grandezza massima del repository
*******************************************************************************/
#ifdef __OP__
#include "ops.h"

int byteDisponibili;
int posizioniDisponibili;
int grandezzaMassima;

typedef enum{
	OPERATION,
	VECCHIO,
	NUOVO
} do_check_t;


/* funzione di servizio per la write che restituisce i codici di errore */
static inline int
check(op_t operation, int vecchio, int nuovo)
{
	extern int byteDisponibili;
	extern int posizioniDisponibili;
	extern int grandezzaMassima;
	switch(operation){
		/* ::::: Put ::::: */
		case PUT_OP:
			if( grandezzaMassima>0 && nuovo>grandezzaMassima )
				return 15;
			if( byteDisponibili==0 ||
						(byteDisponibili>0 && nuovo>byteDisponibili) )
				return 16;
			if( !posizioniDisponibili )
				return 14;

			byteDisponibili = (byteDisponibili==-1)
							  ? -1 : byteDisponibili-nuovo;

			posizioniDisponibili = (posizioniDisponibili==-1)
								   ? -1 : posizioniDisponibili-1;
			break;

		/* ::::: Update ::::: */
		case UPDATE_OP:
			if( vecchio!=nuovo ) return 19;
			break;

		/* ::::: Remove ::::: */
		case REMOVE_OP:
			byteDisponibili = (byteDisponibili==-1)
							  ? -1 : byteDisponibili+vecchio;

			posizioniDisponibili = (posizioniDisponibili==-1)
								   ? -1 : posizioniDisponibili+1;
			break;

		/* ::::: Altre ::::: */
		default:
			return 21;
	}
	return 0;
}


/* macro che inizializza le variabili di disponibilità */
#define initialize(bdisp, pdisp, maxObj)									\
	do{																		\
		extern int byteDisponibili;											\
		extern int posizioniDisponibili;									\
		extern int grandezzaMassima;										\
		byteDisponibili = (bdisp)>0 ? (bdisp) : -1;							\
		posizioniDisponibili = (pdisp)>0 ? (pdisp) : -1;					\
		grandezzaMassima = (maxObj)>0 ? (maxObj) : -1;						\
	} while(0)
#endif


int lockRepo(int index);

int unlockRepo(int index);

int startRead(int index);

void doneRead(void);

int startWrite(int index, int *do_check);

void doneWrite(void);

#endif