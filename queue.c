/* queue.c by Giorgi Matteo, 517183 & Quarta Andrea, 517881 */


/*
	In questa libreria ho messo le funzioni per la gestione di code di interi
	implementate con array circolari ridimensionabili
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "errors.h"
#include "queue.h"


/*******************************************************************************
	Macro per settare i valori nella struttura queue_t
*******************************************************************************/
#define RE_SET( CODA, NEW_Q, NEW_DIM, FIRST, LAST, NUM_EL )		\
	do{															\
		(CODA)->q = (NEW_Q);									\
		(CODA)->first = (FIRST);								\
		(CODA)->last = (LAST);									\
		(CODA)->dim = (NEW_DIM);								\
		(CODA)->num_el = (NUM_EL);								\
	} while(0)


/*******************************************************************************
	Funzione che prende un intedo (dim) e crea una nuova coda
	con dim posti disponibili
*******************************************************************************/
queue_t*
new_queue(int dim)
{
	errno = 0;
	queue_t *new; int *que;
	if( !(new=malloc(sizeof(queue_t))) || !(que=malloc(dim*sizeof(int))) ){
		if( new ) free(new);
		errno = ENOMEM;
		err_msg("creazione coda fallita");
		return NULL;
	}
	RE_SET( new, que, dim, que, que, 0 );
	return new;
}


/*******************************************************************************
	Funzione che prende un FILE* (stream), una coda (coda),
	stampa su stream gli elementi della coda separati da spazi e
	restituisce il numero di elementi stampati
*******************************************************************************/
int
display(FILE *stream, queue_t *coda)
{
	int *a, d, n=0;
	fprintf(stream, "gli elementi della coda sono: ");

	for( a=coda->first, d=coda->num_el; d; d--){
		fprintf(stream, "%d ", *a); n++;
		a = ( a == (coda->q)+(coda->dim)-1 ) ? coda->q : a+1;
	}
	fprintf(stream, "\n");
	return n;
}


/*******************************************************************************
	Funzione che prende una coda (coda), un intero (elem), un booleano (resize)
	e inserisce elem in coda. Se resize==TRUE,
	inserisce elemen anche in caso di coda piena, viceversa non lo inserisce
*******************************************************************************/
int
insert(queue_t *coda, int elem, bool resize)
{
	errno = 0;
	if( full(coda) ){
		if( !resize ) return 0;
		int *aux, *a, *b, d;
		/* rialloco la coda */
		if( !(aux=malloc(2*(coda->dim)*sizeof(int))) ){
			errno = ENOMEM;
			err_msg("coda piena e riallocazione fallita");
			return 0;
		}
		for(d=0; d<2*(coda->dim); d++) aux[d]=-1;
		for( a=coda->first, b=aux, d=coda->num_el; d--; b++){
			*b = *a;
			a = ( a == (coda->q)+(coda->dim)-1 ) ? coda->q : a+1;
		}
		free( coda->q );
		RE_SET( coda, aux, 2*(coda->dim), coda->q, (coda->q)+(coda->num_el),
				(coda->num_el) );
	}

	/* inserisco l'elemento */
	*(coda->last) = elem;
	(coda->num_el)++;

	coda->last =
		( coda->last == (coda->q)+(coda->dim)-1 ) ? coda->q : (coda->last)+1;
	
	return 1;
}


/*******************************************************************************
	Funzione che prende una coda e ne estrae l'elemento in testa
*******************************************************************************/
int
extract(queue_t *coda)
{
	errno = 0;
	if( empty(coda) ){
		errno = ENOBUFS;
		err_msg("coda vuota, non posso estrarre");
		return -1;
	}
	int x = *(coda->first);
	(coda->num_el)--;
	coda->first =
		( coda->first == (coda->q)+(coda->dim)-1 ) ? coda->q : (coda->first)+1;
	
	return x;
}


/*******************************************************************************
	Funzione che prende una coda e restituisce 1 se vuota, 0 altrimenti
*******************************************************************************/
int
empty(queue_t *coda)
{
	return coda->num_el ? 0 : 1;
}


/*******************************************************************************
	Funzione che prende una coda e restituisce 1 se piena, 0 altrimanti
*******************************************************************************/
int
full(queue_t *coda)
{
	return coda->num_el==coda->dim ? 1 : 0;
}