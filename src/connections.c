/* connections.c by Giorgi Matteo, 517183 & Quarta Andrea, 517881 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include "connections.h"

/**
 * @function openConnection
 * @brief Apre una connessione AF_UNIX verso il server membox.
 *
 * @param path Path del socket AF_UNIX 
 * @param ntimes numero massimo di tentativi di retry
 * @param secs tempo di attesa tra due retry consecutive
 *
 * @return il descrittore associato alla connessione in caso di successo
 *         -1 in caso di errore
 */
int
openConnection(char* path, unsigned int ntimes, unsigned int secs)
{
	int sock1, cfd;
	struct sockaddr_un struttura;

	strncpy(struttura.sun_path, SOCKET, UNIX_PATH_MAX);
	struttura.sun_family=AF_UNIX;
	sock1 = socket(AF_UNIX, SOCK_STREAM, 0);
	cfd = connect(sock1, (struct sockaddr*)&struttura, sizeof(struttura));

	while( cfd==-1 ){
		if( errno!=ENOENT ) return -1;
		sleep(secs);
		cfd = connect(sock1, (struct sockaddr*)&struttura, sizeof(struttura));
	}
	return sock1;
}

// -------- server side ----- 

/**
 * @function readHeader
 * @brief Legge l'header del messaggio
 *
 * @param fd     descrittore della connessione
 * @param hdr    puntatore all'header del messaggio da ricevere
 *
 * @return 0 in caso di successo -1 in caso di errore
 */
int
readHeader(long fd, message_hdr_t *hdr)
{
	ssize_t wr, ur; void *ptr;
	if( !fd || !hdr ) return -1;

	for(ur=sizeof(int), ptr=&(hdr->op);
				(wr=read(fd, ptr, ur))<ur; ur-=wr, ptr+=wr)
		if(wr<=0) return -1;

	for(ur=sizeof(unsigned long), ptr=&(hdr->key);
				(wr=read(fd, ptr, ur))<ur; ur-=wr, ptr+=wr)
		if(wr<=0) return -1;

	return 0;
}

/**
 * @function readData
 * @brief Legge il body del messaggio
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al body del messaggio
 *
 * @return 0 in caso di successo -1 in caso di errore
 */
int
readData(long fd, message_data_t *data)
{
	ssize_t wr, ur; int len;
	void *ptr;
	char *buf, *buf_;
	if( fd<0 || !data ) return -1;

	for(ur=sizeof(unsigned int), ptr=&len;
				(wr=read(fd, ptr, ur))<ur; ur-=wr, ptr+=wr)
		if(wr<=0) return -1;

	buf_ = buf = malloc((len)*sizeof(char));
	memset(buf, '\0', len);

	for(ur=len*sizeof(char); (wr=read(fd, buf, ur))<ur; ur-=wr, buf+=wr)
		if(wr<=0) return -1;

	data->len = len;
	data->buf = buf_;
	return 0;
}


/* da completare da parte dello studente con altri metodi di interfaccia */



// ------- client side ------
/**
 * @function sendRequest
 * @brief Invia un messaggio di richiesta al server membox
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return 0 in caso di successo -1 in caso di errore
 */
int
sendRequest(long fd, message_t *msg)
{
	ssize_t wr, ur; void *ptr;
	if( fd<0 || !msg ) return -1;

	for(ur=sizeof(int), ptr=&(msg->hdr.op);
			(wr=write(fd, ptr, ur))<ur; ur-=wr, ptr+=wr)
		if(wr<=0) return -1;
	
	for(ur=sizeof(unsigned long), ptr=&(msg->hdr.key);
			(wr=write(fd, ptr, ur))<ur; ur-=wr, ptr+=wr)
		if(wr<=0) return -1;

	if( msg->hdr.op==PUT_OP || msg->hdr.op==UPDATE_OP ){
		for(ur=sizeof(unsigned int), ptr=&(msg->data.len);
				(wr=write(fd, ptr, ur))<ur; ur-=wr, ptr+=wr)
			if(wr<=0) return -1;

		for(ur=sizeof(char)*(msg->data.len), ptr=msg->data.buf;
				(wr=write(fd, ptr, ur))<ur; ur-=wr, ptr+=wr)
			if(wr<=0) return -1;
	}
	return 0;
}


/**
 * @function readReply
 * @brief Legge un messaggio di risposta dal server membox
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da ricevere
 *
 * @return 0 in caso di successo -1 in caso di errore
 */
int
readReply(long fd, message_t *msg)
{
	ssize_t wr, ur;
	void *ptr;
	char *buf, *buf_;
	op_t op; int len;
	unsigned long key;
	if( fd<0 || !msg ) return -1;

	for(ur=sizeof(int), ptr=&op; (wr=read(fd, ptr, ur))<ur; ur-=wr, ptr+=wr)
		if(wr<=0) return -1;

	for(ur=sizeof(unsigned long), ptr=&key;
			(wr=read(fd, ptr, ur))<ur; ur-=wr, ptr+=wr)
		if(wr<=0) return -1;

	for(ur=sizeof(unsigned int), ptr=&len;
			(wr=read(fd, ptr, ur))<ur; ur-=wr, ptr+=wr)
		if(wr<=0) return -1;

	buf_ = buf = malloc((len)*sizeof(char));
	memset(buf, '\0', len);

	for(ur=len*sizeof(char); (wr=read(fd, buf, ur))<ur; ur-=wr, buf+=wr)
		if(wr<=0) return -1;

	msg->hdr.op = op;
	msg->hdr.key = key;
	msg->data.len = len;
	msg->data.buf = buf;
	return 0;
}