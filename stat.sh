# stat.sh by Giorgi Matteo, 517183 & Quarta Andrea, 517881


#!/bin/bash

widthCols=15
spazi=
simboloAllineatore="."
IFS=" -"
fileStat=
trovato=false
genericRow=

maxConn=0
maxObj=0
maxSize=0

P=false
U=false
G=false
R=false
C=false
S=false
O=false
M=false

function formatta
{
	i=0
	spazi=$(($widthCols-${#1}))
	for((i=0; i<spazi; i++))
	{
			genericRow=$genericRow$simboloAllineatore
	}
	genericRow=$genericRow$1
}

function stampa()
{
	while read -r TIMESTAMP PUT PUTFAILED UPDATE UPDATEFAILED GET GETFAILED REMOVE REMOVEFAILED LOCK LOCKFAILED CONNECTIONS SIZE MAXSIZE OBJECTS MAXOBJECTS
	do
		if $M ; then #massimo numero di connessioni raggiunte
			if [[ $CONNECTIONS -gt $maxConn ]]; then
				maxConn=$CONNECTIONS
			fi
			if [[ $MAXOBJECTS -gt $maxObj ]]; then #MASSIMO NUMERO DI OGGETTI MEMORIZZATI
				maxObj=$OBJECTS
			fi
			if [[ $MAXSIZE -gt $maxSize ]]; then #MASSIMA GRANDEZZA RAGGIUNTA
				maxSize=$SIZE
			fi
		else
			formatta $TIMESTAMP
			if $P ; then #PUT FALLITE ED ESEGUITE
				formatta $PUT
				formatta $PUTFAILED
			fi
			if $U ; then #UPDATE FALLITE ED ESEGUITE
				formatta $UPDATE
				formatta $UPDATEFAILED
			fi
			if $G ; then #GET FALLITE ED ESEGUITE
				formatta $GET
				formatta $GETFAILED
			fi
			if $R ; then #REMOVE FALLITE ED ESEGUITE
				formatta $REMOVE
				formatta $REMOVEFAILED
			fi
			if $L ; then
				formatta $LOCK
				formatta $LOCKFAILED
			fi
			if $C ; then #CONNESSIONI IN UN ISTANTE
				formatta $CONNECTIONS
			fi
			if $S ; then #SIZE IN UN ISTANTE
				formatta $SIZE
			fi
			if $O ; then #NOGGETTI IN UN ISTANTE
				formatta $OBJECTS
			fi
			echo -e $genericRow
			genericRow=""
		fi
	done < "$fileStat"
	if $M ; then
		formatta $maxConn
		formatta $maxObj
		formatta $maxSize
		echo -e $genericRow"\n"
	fi
}

for args ; do
	case $args in
		--help)
			echo -e "Il seguente script ha bisogno di un file di statistiche e uno/nessuno dei seguenti parametri per stampare le statistiche di cui si ha bisogno:\n"
			echo -e "-p) stampa le PUT eseguite e fallite dal server per tutti i timestamp\n"
			echo -e "-u) stampa le UPDATE eseguite e fallite dal server per tutti i timestamp\n"
			echo -e "-g) stampa le GET eseguite e fallite dal server per tutti i timestamp\n"
			echo -e "-r) stampa le REMOVE eseguite e fallite dal server per tutti i timestamp\n"
			echo -e "-c) stampa il numero di connessioni per tutti i timestamp\n"
			echo -e "-s) stampa la size in KB per tutti i timestamp\n"
			echo -e "-o) stampa il numero di oggetti nel repository per tutti i timestamp\n"
			echo -e "-m) questa opzione è esclusiva rispetto alle altre:\n"
			echo -e "\tstampa il massimo numero di connessioni raggiunte dal server,\
il massimo numero di oggetti memorizzati ed la massima size in KB raggiunta"
			echo -e "\n\nInoltre, senza nessuna opzione, lo script stampa tutte le statistiche eccetto quelle del parametro m\n\n"
			exit 1
		;;
	esac
done

for args ; do
	case $args in
    	-p)
    		P=true
    		;;
    	-u)
    		U=true
    		;;
    	-g)
			G=true
			;;
		-r)
			R=true
			;;
		-c)
			C=true
			;;
		-s)
			S=true
			;;
		-o)
			O=true
			;;
		-m)
			M=true
			;;
		*)
			if [ -e $args ]; then
				trovato=true
				fileStat=$args
			else
				echo "invalid argument"
				break
			fi
  esac
done
if $trovato ; then
	if ! $M ; then
		if ! $P && ! $U && ! $G && ! $R && ! $C && ! $S && ! $O; then
			P=true
			U=true
			G=true
			R=true
			C=true
			S=true
			O=true
		fi;
		formatta "TIMESTAMP"
		if $P ; then
			formatta "PUT"
			formatta "PUTFAILED"
		fi
		if $U ; then
			formatta "UPDATE"
			formatta "UPDATEFAILED"
		fi
		if $G ; then
			formatta "GET"
			formatta "GETFAILED"
		fi
		if $R ; then
			formatta "REMOVE"
			formatta "REMOVEFAILED"
		fi
		if $C ; then
			formatta "CONNECTIONS"
		fi
		if $S ; then
			formatta "SIZE"
		fi
		if $O ; then
			formatta "OBJECTS"
		fi
	else
		formatta "MAXCONNECTIONS"
		formatta "MAXOBJECTS"
		formatta "MAXSIZE"
	fi
	echo -e $genericRow"\n"
	genericRow=""
	stampa
fi