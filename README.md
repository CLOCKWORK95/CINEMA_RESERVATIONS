# CINEMA_RESERVATIONS
TCP client-server Cinema Reservations software. 
Implementazione, in linguaggio C, di un software con architettura client-server, 
sviluppato su sistema operativo di famiglia UNIX o WinAPI, per la gestione di prenotazioni per una sala 
cinematografica. Il sistema deve permettere:
▪ la visualizzazione della mappa corrente dei posti a sedere; 
▪ la prenotazione di posti, con la relativa conferma di prenotazione (o aborto della transazione), e la 
generazione di un codice univoco di prenotazione;
▪ la cancellazione di una prenotazione precedentemente effettuata, attraverso l’utilizzo del codice di 
prenotazione.
Per quanto concerne le scelte di progetto, si è deciso di realizzare un’architettura con server multi-thread 
(concorrente), per la gestione parallela di connessioni full-duplex: per fare ciò, si è utilizzato il protocollo 
TCP a livello di trasporto, che garantisce la trasmissione affidabile e ordinata dei dati sul canale di 
comunicazione. Riguardo l’implementazione, si è scelto di sviluppare l’applicativo per sistemi UNIX. 
È stato utilizzato l’API del socket di Berkley per la programmazione socket.
