#!/bin/bash
chmod +x statistiche.sh
LOGFILE=txt/logfile.txt
clear
echo "Script STATISTICHE.SH"
if test -f "$LOGFILE"; then
	echo "File di log standard $LOGFILE presente"
	else
	echo "File di log standard $LOGFILE non presente, cerco il nome nel file di configurazione"
	while read riga; do
		if [[ $riga == "LOG_FILE_NAME"* ]]; then
			LOGFILE=${riga#*./}
			break
		fi
	done < txt/config_file.txt
	#implementare ricerca tramite configfile.txt
	if [[ $LOGFILE == "txt/logfile.txt" ]]; then
		echo "File di log non trovato, ABORT"
		exit 1;
		else
			echo "----------"
			echo "File di log: $LOGFILE"
	fi
fi


let WRITEDIM=0; #dimensione scritture con successo
let WRITECOUNT=0; #numero scritture con successo
let READDIM=0; #dimensione letture con successo
let READCOUNT=0; #numero letture con successo
let LOCKCOUNT=0; #numero di lock con successo
let UNLOCKCOUNT=0; #numero di unlock con successo

READNUM=$(grep -w -c 'richiesta:4' $LOGFILE)
WRITENUM=$(grep -w -c 'richiesta:6' $LOGFILE)
OPENLOCK=$(grep -w -c 'inserito nello storage e bloccato' $LOGFILE)
LOCKCOUNT=$(grep -w -c 'correttamente bloccato' $LOGFILE)
UNLOCKCOUNT=$(grep -w -c 'correttamente sbloccato' $LOGFILE)

while read riga; do
	if [[ $riga == *"Sono stati scritti"* ]]; then
		#echo "E' una riga di scrittura"
		WRITTEN=${riga%bytes*}
		WRITTEN=${WRITTEN##*scritti}
		let WRITEDIM=$[$WRITEDIM + $WRITTEN]
		let WRITECOUNT=$[$WRITECOUNT+1]
	fi
	if [[ $riga == *"Sono stati appesi"* ]]; then
		#echo "E' una riga di append"
		WRITTEN=${riga%bytes*}
		WRITTEN=${WRITTEN##*appesi}
		let WRITEDIM=$[$WRITEDIM + $WRITTEN]
		let WRITECOUNT=$[$WRITECOUNT+1]
	fi
	if [[ $riga == *"Sono stati letti con successo"* ]]; then
		#echo "E' una riga di lettura"
		READ=${riga%bytes*}
		READ=${WRITTEN##*appesi}
		let READDIM=$[$READDIM + $READ]
		let READCOUNT=$[$READCOUNT+1]
	fi
	if [[ $riga == "Numero massimo di file memorizzati"* ]]; then
		MAXFILENUM=${riga##*:}
	fi
	if [[ $riga == "Dimensione massima raggiunta"* ]]; then
		MAXFILEDIM=${riga##*:}
	fi
	if [[ $riga == "Numero attivazioni dell'algoritmo di rimpiazzamento"* ]]; then
		REPL=${riga##*:}
	fi
	if [[ $riga == "Numero massimo di connessioni contemporanee"* ]]; then
		MAXCONTCONN=${riga##*:}
	fi
done < $LOGFILE
let WRITEAVG=0
if [ $WRITECOUNT != 0 ]; then
	if [ $WRITEDIM != 0 ]; then
		WRITEAVG=$[$WRITEDIM / $WRITECOUNT]
	fi
fi
let READAVG=0
if [ $READCOUNT != 0 ]; then
	if [ $READDIM != 0 ]; then
		READAVG=$[$READDIM / $READCOUNT]
	fi
fi
echo "-> Numero scritture richieste (writeFile): $WRITENUM"
echo "-> Numero scritture effettuate: $WRITECOUNT"
echo "-> Bytes scritti: $WRITEDIM bytes"
echo "-> Media bytes scritti per scrittura: $WRITEAVG bytes"
echo "----------"
echo "-> Numero letture richieste (readFile): $READNUM"
echo "-> Numero letture effettuate: $READCOUNT"
echo "-> Bytes letti: $READDIM bytes"
echo "-> Media bytes letti per lettura: $READAVG bytes"
echo "----------"
echo "-> Numero di lock effettuate: $LOCKCOUNT"
echo "-> Numero di unlock effettuate: $UNLOCKCOUNT"
echo "-> Numero di open-lock: $OPENLOCK"
echo "----------"
echo "-> Numero massimo di file memorizzati: $MAXFILENUM bytes"
echo "-> Dimensione massima di file memorizzati: $MAXFILEDIM bytes"
echo "-> Attivazioni dell'algoritmo di rimpiazzamento: $REPL"
echo "-> Massimo numero di connessioni contemporanee: $MAXCONTCONN bytes"
echo "----------"
grep -w 'WORKER' $LOGFILE | sort -u > workerid.txt
while read riga; do
	riga=${riga%]}
	riga=${riga#[}
	echo "$riga ha eseguito $(grep -c "$riga" txt/journal.txt) operazioni"
done < workerid.txt
rm workerid.txt

