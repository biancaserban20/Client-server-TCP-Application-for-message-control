# Client-server-TCP-Application-for-message-control
CUPRINS
-------
- Introducere
- Realizarea temei

INTRODUCERE
-----------
Din punctul meu de vedere, tema a avut un nivel de dificultate mediu. Durata de
rezolvare a temei a fost de aproximativ 25 ore. Consider ca mi-a fost de folos
aceasta tema deoarece am putut intelege mai bine informatiile si exercitiile de
la laborator legate de multiplexare si aplicatia client-server.

REALIZAREA TEMEI
----------------
Precizez ca in realizarea acestei teme m-am folosit si de rezolvarile exercitiilor
din laboratoarele 6 si 8.
Am implementat 2 programe: server si client.
In server:
- main:

Se dezactiveaza bufferingul si stochez argumentul primit la rulare (portul
serverului). Se completeaza informatiile serverului (port, adresa ip etc.). Creez
2 socketi (unul pentru UDP, unul pentru TCP) si dezactivez algoritmul Neagle pentru
TCP. Se creeaza un set de files descriptors = UDP socket, TCP socket si socketul 0 
(STDIN).

In while, se interpreteaza diferite mesaje primite dupa urmatoarele cazuri:

a) se citeste de la STDIN:
Aici singurul input acceptat este "exit" care duce la inchiderea serverului
si a tuturor clientilor. Daca un alt input este tastat, se afiseaza un mesaj 
corespunzator.

b) se primeste un mesaj pe UDP socket:
Se cauta topicul primit (in mesaj) intr-un vector de topicuri declarata la inceputul
programului. Daca nu exita, se adauga la acest vector, altfel se trimite mesajul 
tuturor clientilor TCP subscriberi la acel topic si conectati la server in acel 
moment(functia sendMsgtoSubscribers).

c) se primeste un mesaj pe TCP socket:
Intai, clientul TCP isi trimite id-ul pentru ca serverul sa poata verifica daca exista un
client deja conectat cu acel id. In acest caz, se afiseaza mesajul
"Client <id> already connected".
Altfel, daca exita acel id in database si statustul este de unconnected, atunci se 
afiseaza "New client <id> connected". Daca nu s-a gasit acel id, se adauga noul client 
in database si se afiseaza mesajul de mai devreme. Acest database este de fapt un vector
de TCPsubscribers. 

d) un alt socket, adica serverul primeste mesaje de la un client TCP:
Daca nr de bytes primiti este 0, inseamna ca acel client s-a deconectat. Actualizez 
statusul in database so afisez mesajul "Client <id> disconnected.". 
Se inchide socketul si se scoate din set.
Altfel, clientul va trimite serverului o comanda de tip subscribe/unsubscribe.
Pentru Subscribe:
- parcurg vectorul de topics si verific daca exista acel topic. In caz afirmativ,
abonez acel client la acel topic (daca nu este deja abonat). Altfel, adaug topicul in 
lista. Actualizez databaseul, adica adaug noul Subscription la vectorul de abonari al acelui
client.
Pentru Unsubscribe:
- caut topicul in lista, scot acel topic din vectorul de topics al acelui client si fac
modificarile necesare.

Modificarile aduse programului client, preluat din lab 8:
In while, se interpreteaza diferite mesaje primite dupa urmatoarele cazuri:
a) se citeste de la STDIN:
daca inputul este:
- "exit", atunci se deconecteaza acel client de la server si se inchide
socketul.
- "Subscribe"/"Unsubscribe" se trimite un mesaj de tip TCPcommand catre server care contine id-ul,
topicul, SF si tipul comenzii ( 1 = Subscribe; 0 = Unsubscribe). La subscribe, poate primi mesaje 
de la server care sa il anunte daca s-a abonat cu succes sau era deja connectat si printeaza mesaje
corespunzatoare.

b) socketul pe care primeste mesaje UDP de la server
daca nr. de bytes primiti este 0 inseamna ca serverul s-a inchis, deci se va inchide si acest socket.
Altfel, va interpreta mesajul UDP primit si il va afisa.

Precizare: Toate structurile de date se gasesc in helpers.h
