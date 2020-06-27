/* errors.c by Giorgi Matteo, 517183 & Quarta Andrea, 517881 */


/*
	In questa libreria ho messo le funzioni per la gestione degli errori
	e la visualizzazione dell'errore pervenuto.
*/


#include <stdarg.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "errors.h"


/*******************************************************************************
	Funzione ausiliaria che prende un booleano (exit_3) e termina il processo 
	con exit o con _exit; oppure se settata la variabile di ambiente fittizia
	EF_DUMPCORE abortisce il processo.
*******************************************************************************/
static void
termina( bool exit_3 )
{
	char *s;
	if((s=getenv("EF_DUMPCORE")) && *s!='\0') abort();
	
	if(exit_3) exit(EXIT_FAILURE);
	else _exit(EXIT_FAILURE);
}


/*******************************************************************************
	Funzione ausiliaria che prende un booleano (usa_errore), un intero (errore),
	un booleano (flush_stdout), una stringa (format),
	un puntatore al primo argomento di una lista di variadics
	e stampa i variadics ed errore su di una stringa locale
	poi a sua volta stampata su stderror
*******************************************************************************/
static void
output_error( bool usa_errore, int errore, bool flush_stdout,
			  const char *format, va_list ap )
{
	#define BUF_SIZE 500
	char buf[BUF_SIZE],
		 messaggio[BUF_SIZE],
		 testo_errore[BUF_SIZE],
		 concat[BUF_SIZE]; memset(concat, '\0', sizeof(concat));

	/* Stampo nella messaggio tutta la lista di stringhe
	   che parte con format e scorre con ap */
	vsnprintf(messaggio, BUF_SIZE, format, ap);

	/* Se voglio usare l'errore:
	   in testo_errore stampo [nome_errore: descrizione_errore]... */
	if(usa_errore)
		snprintf(testo_errore, BUF_SIZE, "[%s %s]",
				 0<errore && errore<=ERRORE_MAX ?
				 strcat(strcat(concat, ename[errore]), ":") : "?UNKNOWN?:",
				 strerror(errore));

	/* ...altrimenti ci stampo : */
	else snprintf(testo_errore, BUF_SIZE, ":");

	/* Adesso su buf stampo mesaggio e testo_errore */
	snprintf(buf, BUF_SIZE, "Error >>> %s %s\n", testo_errore, messaggio);

	if(flush_stdout) fflush(stdout);
	fputs(buf, stderr);
	fflush(stderr);
}

/*******************************************************************************
	Funzione per visualizzare errno ed un messaggio di errore
*******************************************************************************/
void
err_msg(const char *format, ...)
{
	va_list argList;
	int saved_errno = errno;

	va_start(argList, format);
	output_error(TRUE, errno, TRUE, format, argList);
	va_end(argList);

	errno = saved_errno;
}

/*******************************************************************************
	Funzione per terminare il processo con exit,
	visualizzare errno ed un messaggiio di errore
*******************************************************************************/
void
err_exit(const char *format, ...)
{
	va_list argList;

	va_start(argList, format);
	output_error(TRUE, errno, TRUE, format, argList);
	va_end(argList);

	termina(TRUE);
}

/*******************************************************************************
	Funzione per terminare il processo con _exit,
	visualizzare errno ed un messaggiio di errore senza fare il flush di stdout
*******************************************************************************/
void
err_uexit(const char *format, ...)
{
	va_list argList;

	va_start(argList, format);
	output_error(TRUE, errno, FALSE, format, argList);
	va_end(argList);

	termina(FALSE);
}

/*******************************************************************************
	Funzione per terminare il processo con exit,
	visualizzare un numero di errore errnum ed un messaggio di errore
	(utile in programmi che usano funzioni della librerie POSIX per i threads)
*******************************************************************************/
void
err_exit_en(int errnum, const char *format, ...)
{
	va_list argList;

	va_start(argList, format);
	output_error(TRUE, errnum, TRUE, format, argList);
	va_end(argList);

	termina(TRUE);
}

/*******************************************************************************
	Funzione per terminare il processo con exit
	e visualizzare un messaggiio di errore
	(utile per il controllo di quelle funzioni di libreria
	che non settano errno)
*******************************************************************************/
void
fatal(const char *format, ...)
{
	va_list argList;

	va_start(argList, format);
	output_error(FALSE, 0, TRUE, format, argList);
	va_end(argList);

	termina(TRUE);
}

/*******************************************************************************
	Funzione che termina il processo con exit,
	utile per segnalare errori tra gli argomenti della lina di comando
*******************************************************************************/
void
err_usage(const char *format, ...)
{
	va_list argList;

	fflush(stdout);

	fprintf(stderr, "Usage: ");
	va_start(argList, format);
	vfprintf(stderr, format, argList);
	va_end(argList);

	fflush(stderr);
	exit(EXIT_FAILURE);
}