
# PSOP - Sistem de Broker de Mesaje

# Tabelul conținutului

- [Prezentare generala](#prezentare-generala)
- [Instalare](#instalare)
- [Utilizare](#utilizare)
- [Fluxul Aplicatiei](#fluxul-aplicatiei)
- [Structuri de date](#structuri-de-date)
  - [Packet](#packet)
  - [Queue and Message](#queue-and-message)
  - [Exchange](#exchange-uri-si-structurile-de-date-folosite-de-aceastea)
- [Functionalitati viitoare](#functionalitati-viitoare)

## Prezentare generala

Acest proiect implementează un sistem de broker de mesaje, destinat facilitării schimbului de mesaje între **producători** și **consumatori** prin intermediul unui server centralizat. Sistemul este conceput pentru a permite producătorilor să trimită mesaje către server, iar consumatorilor să se aboneze la anumite topice pentru a primi mesajele. Serverul gestionează distribuția mesajelor folosind un mecanism de exchange. Tipurile de exchange suportate includ **Direct Exchange** și **Topic Exchange**, prezentate in detaliu ulterior. Proiectul include o biblioteca statica (API) care permite utilizatorului sa comunice cu serverul si sa preia atat rolul producatorului, cat si rolul consumatorului, și server, fiecare având roluri și funcționalități bine definite pentru a asigura un flux eficient de mesaje.

## Instalare

Ruleaza comanda **make** in directorul root al proiectului
```bash
make
```
In urma executarii comenzii va fi creat executabilul **server** si biblioteca statica **messagebroker**.

> [!WARNING]
> Proiectul foloseste biblioteca [libuuid](https://github.com/util-linux/util-linux/tree/master/libuuid). 
> <br> Se recomanda instalarea ei, dar aceasta poate fi omisa deoarece este implementata o varianta proprie.<br>
> Biblioteca poate fi instalata astfel:<br>

### Debian, Ubuntu, Linux Mint
```bash
sudo apt install uuid-dev
```
### Fedora, RHEL, CentOS
```bash
sudo dnf install libuuid-devel
```
### Arch Linux, Manjaro
```bash
sudo pacman -S util-linux-libs
```
## Utilizare

### Pornire server
```bash
./server
```
### Presupunand ca structura proiectului vostru este organizat astfel:
```makefile
project/
├── lib/                # Biblioteca statică
│   └── libmessagebroker.a
├── include/            # Fișierele de antet
│   ├── message_broker.h
│   ├── log.h
│   ├── packet.h
│   └── cJSON.h
├── src/                # Codul sursă pentru clientul de test
│   └── test_client.c
```
### Proiectul poate fi compilat folosind comanda:
```bash
gcc -o test_client src/test_client.c -Iinclude -Llib -lmessagebroker
```
## Fluxul aplicatiei
```mermaid
sequenceDiagram
    participant MessageBroker
    participant Server
    participant Exchange

    MessageBroker->>Server: Authenticate (PKT_AUTH)
    Server-->>MessageBroker: Authentication Success (PKT_AUTH_SUCCESS)
    Server-->>MessageBroker: Authentication Failure (PKT_AUTH_FAILURE)

    MessageBroker->>Server: Send Message (PKT_PRODUCER_PUBLISH)
    Server-->>MessageBroker: Acknowledge (PKT_PRODUCER_ACK)
    Server-->>MessageBroker: Error Message (PKT_BAD_FORMAT | INCOMPLETE)

    Server->>Exchange: Store Message (Based on Type)

    MessageBroker->>Server: Subscribe to Topic (PKT_SUBSCRIBE)

    MessageBroker->>Server: Request Messages (PKT_CONSUMER_REQUEST)
    Server->>Exchange: Retrieve Messages (Based on request Type)
    Server-->>MessageBroker: Send Messages (PKT_CONSUMER_RESPONSE)
    MessageBroker->>Server: Disconnect (PKT_DISCONNECT)
```
1. Din perspectiva producatorului:
   - creeaza si trimite mesaje in format JSON catre server
   - primeste si gestioneaza confirmari sau mesaje de eroare de la server
   - foloseste un mecanism de retry:
     + se incearca trimiterea pachetului de un anumit numar de ori la un inteval de timp care creste progresiv pana la primirea confirmarii de la server sau pana la epuizarea incercarilor
2. Server:
   + gestioneaza conexiuni multiple
   + permite autentificarea clientilor pe baza username-ului si a unei parole
   + primeste mesaje de la producatori
   + stocheaza mesajele in functie de tipul mesajului (direct, topic)
   + primeste cereri de mesaje de la consumatori
   + permite abonarea consumatorilor la anumite topice (necesita autentificare)
   + trimite mesaje catre consumatori
4. Din perspectiva consumatorului:
   + trimite cereri pentru a primi mesaje
   + se poate abona la anumite topice
   + primeste mesaje de la server
5. Exchange:
   + stocheaza si gestioneaza mesajele aferente
   + exista 2 tipuri de astfel de exchange-uri
     - direct exchange: coada identificata printr o cheie (ex: "key 1")
     - topic exchange: mesajele sunt rutate către cozi pe baza unui topic specific. Topicurile sunt definite ca șiruri de caractere separate prin puncte (.), de exemplu: sport.football, news.weather
  
## Structuri de date
### Packet
```mermaid
classDiagram
    class packet {
        +packet_type packet_type
        +unique_id id
        +char payload[MAX_PACKET_SIZE]
    }

    class packet_type {
        <<enumeration>>
        PKT_UNKNOWN
        PKT_BAD_FORMAT
        PKT_INCOMPLETE
        PKT_EMPTY_QUEUE
        PKT_PRODUCER_PUBLISH
        PKT_PRODUCER_ACK
        PKT_CONSUMER_REQUEST
        PKT_CONSUMER_RESPONSE
        PKT_AUTH
        PKT_AUTH_SUCCESS
        PKT_AUTH_FAILURE
        PKT_DISCONNECT
        PKT_SUBSCRIBE
    }

    class unique_id {
        #if HAS_UUID
        uuid_t uuid
        #else
        char custom_id[37]
        #endif
    }

    packet --> packet_type
    packet --> unique_id
```
1. pachet_type
   + Tipul pachetului, definit printr-o enumerare packet_type. Aceasta indică scopul și conținutul pachetului.
   + Enumerarea packet_type include următoarele valori:
     - PKT_UNKNOWN: Tip necunoscut
     - PKT_BAD_FORMAT: Format greșit
     - PKT_INCOMPLETE: Pachet incomplet
     - PKT_EMPTY_QUEUE: Coada este goală
     - PKT_PRODUCER_PUBLISH: Publicare de către producător
     - PKT_PRODUCER_ACK: Confirmare de la producător
     - PKT_PRODUCER_NACK: Negație de la producător
     - PKT_CONSUMER_REQUEST: Cerere de la consumator
     - PKT_CONSUMER_RESPONSE: Răspuns către consumator
     - PKT_AUTH: Autentificarea clientului la server
     - PKT_AUTH_SUCCESS: Confirmă autentificarea reușită
     - PKT_AUTH_FAILURE: Notifică eșecul autentificării
     - PKT_DISCONNECT: Solicită sau notifică deconectarea clientului
     - PKT_SUBSCRIBE: Abonare la un topic specific
2. id:
   + Identificator unic al pachetului, definit prin structura unique_id. Acesta este utilizat pentru a urmări pachetele și a asigura unicitatea lor.
   + Structura unique_id poate conține:
     - uuid_t uuid: UUID (dacă este disponibil)
     - char custom_id[37]: ID personalizat (dacă UUID nu este disponibil)
3. payload:
   + Datele utile ale pachetului, stocate într-un array de caractere de dimensiune MAX_PACKET_SIZE. Aceste date reprezintă conținutul efectiv al pachetului, care poate fi un mesaj, o cerere sau alte informații relevante.
##
### Queue and Message
```mermaid
classDiagram
    direction LR
    class queue {
        +char* q_topic
        +char* q_binding_key
        +int q_size
        +q_node* first_node
        +q_node* last_node
        +queue* next_queue
        +pthread_mutex_t mutex
        +pthread_cond_t not_empty_cond
    }

    class q_node {
        +message message
        +q_node* next
    }

    class message {
        +message_type type
        +char payload[MAX_PAYLOAD_SIZE]
    }

    class message_type {
        <<enumeration>>
        MSG_EMPTY
        MSG_INVALID
        MSG_DIRECT
        MSG_FANOUT
        MSG_TOPIC
    }

    queue --> q_node
    q_node --> message
    message --> message_type
```
Mesage:<br>
1. type
   + Tipul mesajului, definit printr-o enumerare message_type. Aceasta indică scopul și conținutul mesajului.
   + Enumerarea message_type include următoarele valori:
       - MSG_EMPTY: Mesaj gol
       - MSG_INVALID: Mesaj invalid
       - MSG_DIRECT: Mesaj direct
       - MSG_FANOUT: Mesaj fanout (inca neimplementat)
       - MSG_TOPIC: Mesaj topic
2. payload
   + Datele utile ale mesajului, stocate într-un array de caractere de dimensiune MAX_PAYLOAD_SIZE. Aceste date reprezintă conținutul efectiv al mesajului.<br>
##
Queue:<br>
+ Sincronizarea este realizată folosind mutex-uri și variabile de condiție pentru a gestiona accesul la cozi și pentru a semnala starea acestora (de exemplu, dacă o coadă este goală sau nu).
+ In cazul listelor de cozi este folosit algoritmul Round Robin pentru a asigura o distribuire echitabila.

1. q_topic
   + Topic-ul cozii, utilizat pentru a identifica mesajele care aparțin unui anumit subiect.
2. q_binding_key
   + Cheia de legătură a cozii, utilizată pentru a lega mesajele de coadă în cazul unui direct exchange.
3. q_size
   + Dimensiunea cozii, reprezentând numărul de mesaje stocate în coadă.
4. first_node
   + Primul nod din coadă, indicând începutul listei de mesaje.
5. last_node
   + Ultimul nod din coadă, indicând sfârșitul listei de mesaje.
6. next_queue
   + Următoarea coadă în listă, utilizată pentru a lega mai multe cozi într-o structură de tip listă.
7. mutex
   + Mutex pentru sincronizare, utilizat pentru a asigura accesul concurent sigur la coadă.
8. not_empty_cond
   + Condiție pentru a semnala ca exista mesaje in coada.
##
### Exchange-uri si structurile de date folosite de aceastea
```mermaid
classDiagram
    class direct_exchange {
        +hash_table* bindings
    }

    class fanout_exchange {
        +queue* queues
    }

    class topic_exchange {
        +trie_node* trie
    }

    class hash_table {
        +hash_node** buckets
    }

    class hash_node {
        +char *binding_key;
        +queue *queues;
        +int current_queue_index;
        +int queue_count;
    }

    class trie_node {
        +queue* queue
        +bool is_end_of_topic
        +char* topic_part
        +trie_node* children[MAX_CHILDREN]
    }

    class queue{
        ...
    }

    direct_exchange --> hash_table
    hash_table --> hash_node
    topic_exchange --> trie_node
    trie_node --> queue
```

Direct_exchange:<br>
1. bindings
   + Un tabel hash care leagă cheile de cozi. Fiecare cheie este asociată cu o coadă specifică. <br><br>

Topic_exchange:<br>
1. trie
   + Un trie pentru gestionarea topicelor. Fiecare nod din trie reprezintă o parte a unui topic și poate avea copii care reprezintă sub-topicuri. <br><br>

Fanout_exchange:<br>
1. queues
   + O listă de cozi către care sunt trimise mesajele. Toate cozile din această listă vor primi mesajele trimise către exchange-ul fanout. <br>

## Functionalitati viitoare
1. Server:
   - adaugare fanout exchange
   - posibilitatea monitorizarii cozilor prin comenzi din terminal
##
