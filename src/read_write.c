/* read_write.c by Giorgi Matteo, 517183 & Quarta Andrea, 517881 */


/*
In questa libreria ho messo tutte le funzioni
che gestiscono l'entrata e uscita dalla struttura condivisa
dai vari thread.

Ho evitato la starvation degli scrittori, 
bloccando i lettori in entrata nel caso ci siano degli scrittori in attesa e
riattivando uno scrittore in attesa all'uscita dell'ultimo lettore.

Ho evitato la starvarion dei lettori,
riattivando tutti i lettori in attesa all'uscita di uno scrittore.

Questa libreria può essere usata come una classica libreria lettori-scrittori
(eventualmente con l'aggiunta dei bloccatori) oppure
come una libreria lettori-scrittori con il check delle operazioni da fare.
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include "errors.h"
#define __OP__
#include "read_write.h"


/*******************************************************************************
	Si chiede la lock sul repo
*******************************************************************************/
int
lockRepo(int index)
{
	if(index<0) return 21;
	extern struct variabili vars;
	int ris;
	/* :::::::::: Lock :::::::::: */
	if( ris=pthread_mutex_lock(&mux) )
		err_exit_en(ris, "lock fallita");

		if( vars.activeBlocker==index ){
			/* :::::::::: Unlock :::::::::: */
			if( ris=pthread_mutex_unlock(&mux) )
				err_exit_en(ris, "unlock fallita");
			return 11;
		}
		if( vars.activeBlocker>=0 || vars.waitingBlocker ){
			/* :::::::::: Unlock :::::::::: */
			if( ris=pthread_mutex_unlock(&mux) )
				err_exit_en(ris, "unlock fallita");
			return 21;
		}
		vars.waitingBlocker++;
		while( vars.activeWriters || vars.activeReaders )
			pthread_cond_wait(&lockGo, &mux);
		vars.waitingBlocker--;
		vars.activeBlocker = index;

	/* :::::::::: Unlock :::::::::: */
	if( ris=pthread_mutex_unlock(&mux) )
		err_exit_en(ris, "unlock fallita");
	return 11;
}


/*******************************************************************************
	Si rilascia la lock sul repo
*******************************************************************************/
int
unlockRepo(int index)
{
	extern struct variabili vars;
	int ris;
	/* :::::::::: Lock :::::::::: */
	if( ris=pthread_mutex_lock(&mux) )
		err_exit_en(ris, "lock fallita");

		if( vars.activeBlocker==index )
			vars.activeBlocker = -1;
			/* :::::::::: Unlock :::::::::: */
			if( ris=pthread_mutex_unlock(&mux) )
				err_exit_en(ris, "unlock fallita");
			return 11;
		if( vars.activeBlocker>=0 || vars.waitingBlocker ){
			/* :::::::::: Unlock :::::::::: */
			if( ris=pthread_mutex_unlock(&mux) )
				err_exit_en(ris, "unlock fallita");
			return 22;
		}

	/* :::::::::: Unlock :::::::::: */
	if( ris=pthread_mutex_unlock(&mux) )
		err_exit_en(ris, "unlock fallita");
	return 11;
}


/*******************************************************************************
	Il lettore chiede di leggere
*******************************************************************************/
int
startRead(int index)
{
	extern struct variabili vars;
	int ris;
	/* :::::::::: Lock :::::::::: */
	if( ris=pthread_mutex_lock(&mux) )
		err_exit_en(ris, "lock fallita");

		if(vars.activeBlocker==index){
			/* :::::::::: Unlock :::::::::: */
			if( ris=pthread_mutex_unlock(&mux) )
				err_exit_en(ris, "unlock fallita");
			return 0;
		}
		if( vars.activeBlocker>=0 || vars.waitingBlocker ){
			/* :::::::::: Unlock :::::::::: */
			if( ris=pthread_mutex_unlock(&mux) )
				err_exit_en(ris, "unlock fallita");
			return 21;
		}
		vars.waitingReaders++;
		while( vars.activeWriters ||
				(vars.activeReaders && vars.waitingWriters) )
			pthread_cond_wait(&readGo, &mux);
		vars.waitingReaders--;
		vars.activeReaders++;

	/* :::::::::: Unlock :::::::::: */
	if( ris=pthread_mutex_unlock(&mux) )
		err_exit_en(ris, "unlock fallita");
	return 0;
}


/*******************************************************************************
	Il lettore ha finito di leggere
	ed esce della struttura condivisa
*******************************************************************************/
void
doneRead(void)
{
	extern struct variabili vars;
	int ris;
	/* :::::::::: Lock :::::::::: */
	if( ris=pthread_mutex_lock(&mux) )
		err_exit_en(ris, "lock fallita");

		if( vars.activeBlocker>=0 ){
			/* :::::::::: Unlock :::::::::: */
			if( ris=pthread_mutex_unlock(&mux) )
				err_exit_en(ris, "unlock fallita");
			return;
		}
		vars.activeReaders--;
		if(!vars.activeReaders)
			if(vars.waitingWriters) pthread_cond_signal(&writeGo);
			else if(vars.waitingBlocker) pthread_cond_signal(&lockGo);

	/* :::::::::: Unlock :::::::::: */
	if( ris=pthread_mutex_unlock(&mux) )
		err_exit_en(ris, "unlock fallita");
}


/*******************************************************************************
	Lo scrittore chiede di scrivere
*******************************************************************************/
int
startWrite(int index, int *do_check)
{
	extern struct variabili vars;
	extern int byteDisponibili;
	extern int posizioniDisponibili;
	extern int grandezzaMassima;
	int ris, err;
	/* :::::::::: Lock :::::::::: */
	if( ris=pthread_mutex_lock(&mux) )
		err_exit_en(ris, "lock fallita");

		if( (vars.activeBlocker>=0 && vars.activeBlocker!=index) ||
				vars.waitingBlocker ){
			/* :::::::::: Unlock :::::::::: */
			if( ris=pthread_mutex_unlock(&mux) )
				err_exit_en(ris, "unlock fallita");
			return 21;
		}

		/* qui si controlla che l'operazione richiesta
		   sia effettivamente eseguibile (solo se definjito __OP__) */
		#ifdef __OP__
		if( do_check ){
			op_t operation = (op_t) do_check[OPERATION];
			int vecchio = (int) do_check[VECCHIO],
				nuovo = (int) do_check[NUOVO];
			if( err=check(operation, vecchio, nuovo) ){
				/* :::::::::: Unlock :::::::::: */
				if( ris=pthread_mutex_unlock(&mux) )
					err_exit_en(ris, "unlock fallita");
				return err;
			}
		}
		#endif

		if( vars.activeBlocker==index ){
			/* :::::::::: Unlock :::::::::: */
			if( ris=pthread_mutex_unlock(&mux) )
				err_exit_en(ris, "unlock fallita");
			return 0;
		}

		vars.waitingWriters++;
		while( vars.activeWriters || vars.activeReaders )
			pthread_cond_wait(&writeGo, &mux);
		vars.waitingWriters--;
		vars.activeWriters++;

	/* :::::::::: Unlock :::::::::: */
	if( ris=pthread_mutex_unlock(&mux) )
		err_exit_en(ris, "unlock fallita");
	return 0;
}


/*******************************************************************************
	lo scrittore ha finito di scrivere
	ed esce dalla struttura condivisa
*******************************************************************************/
void
doneWrite(void)
{
	extern struct variabili vars;
	int ris;
	/* :::::::::: Lock :::::::::: */
	if( ris=pthread_mutex_lock(&mux) )
		err_exit_en(ris, "lock fallita");

		if( vars.activeBlocker>=0 ){
			/* :::::::::: Unlock :::::::::: */
			if( ris=pthread_mutex_unlock(&mux) )
				err_exit_en(ris, "unlock fallita");
			return;
		}
		vars.activeWriters = 0;
		if(vars.waitingReaders) pthread_cond_broadcast(&readGo);
		else
			if(vars.waitingWriters) pthread_cond_signal(&writeGo);
			else if(vars.waitingBlocker) pthread_cond_signal(&lockGo);

	/* :::::::::: Unlock :::::::::: */
	if( ris=pthread_mutex_unlock(&mux) )
		err_exit_en(ris, "unlock fallita");
}