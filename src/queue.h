/* queue.h by Giorgi Matteo, 517183 & Quarta Andrea, 517881 */


#ifndef __BOOL__
#define __BOOL__
typedef enum { FALSE, TRUE } bool;
#endif

/*******************************************************************************
	Struttura coda:
	q      --> puntatore all'array circolare contenente gli elementi
	first  --> puntatore al primo elemento della coda
	last   --> puntatore all'ultimo elemento della coda
	dim    --> dimensione massima della coda
	num_el --> numero di elementi presenti nella coda
*******************************************************************************/
typedef struct queue{
	int *q;
	int *first;
	int *last;
	int dim;
	int num_el;
} queue_t;


queue_t* new_queue(int dim);

int display(FILE *stream, queue_t *coda);

int insert(queue_t *coda, int elem, bool voglio_allungare_la_coda);

int extract(queue_t *coda);

int empty(queue_t *coda);

int full(queue_t *coda);