
# Proiect PSOP - Sistem de Broker de Mesaje

Acest proiect implementează un sistem de tip broker de mesaje, cu rolul de a facilita schimbul de mesaje între producători și consumatori printr-un server centralizat. Producătorii trimit mesaje către server, iar consumatorii pot primi aceste mesaje în funcție de anumite criterii de subscriere.

## Structura Proiectului

- **Producer**: Producătorii generează și trimit mesaje către server.
- **Consumer**: Consumatorii se abonează la chei sau topice și primesc mesaje în funcție de subscriere.
- **Server**: Serverul primește mesaje de la producători și le redirecționează către consumatori folosind două metode de exchange:
  - `Direct Exchange`
  - `Topic Exchange`

## Fișiere Importante

- `producer.c`: Implementarea pentru producătorul de mesaje.
- `consumer.c`: Implementarea pentru consumatorul de mesaje.
- `server.c`: Implementarea serverului de mesaje.
- `socketutil.c` și `socketutil.h`: Funcții auxiliare pentru gestionarea comunicațiilor prin socket.
- `hash_table.c` și `trie.c`: Structuri de date utilizate pentru gestionarea mesajelor și a topice-urilor.
- `log.c`: Modul pentru logare.

## Descrierea Componențelor

### 1. Producer (`producer.c`)

Producătorul inițiază o conexiune TCP la server și transmite mesaje preluate dintr-un fișier JSON (`messages.json`).
- **Funcții principale**:
  - `read_and_parse_json_from_file`: Citește mesaje dintr-un fișier JSON și le salvează într-un array de mesaje.
  - `process_packets`: Procesează pachetele de mesaje și le trimite serverului.
- **Flux de execuție**:
  1. Creează un socket și se conectează la `127.0.0.1:8080`.
  2. Încarcă mesaje din `messages.json`.
  3. Trimite mesajele în format `PKT_PRODUCER_PUBLISH` către server.
  4. Loghează activitatea în `producer_log.txt`.

### 2. Consumer (`consumer.c`)

Consumatorul se conectează la server și trimite cereri pentru a primi mesaje în funcție de criterii de tip "direct" sau "topic".
- **Funcții principale**:
  - `generate_json_request_key` și `generate_json_request_topic`: Generează cereri JSON pentru mesaje specifice sau pentru topice.
  - `process_packets`: Procesează mesajele primite de la server.
- **Flux de execuție**:
  1. Creează un socket și se conectează la `127.0.0.1:8080`.
  2. Trimite cereri către server pentru anumite chei (`direct`) și topice (`topic`).
  3. Așteaptă răspunsuri și procesează mesajele primite.
  4. Loghează activitatea în `consumer_log.txt`.

### 3. Server (`server.c`)

Serverul primește mesaje de la producători și cereri de la consumatori, distribuindu-le în funcție de tipul de exchange configurat:
- **Direct Exchange**: Mesajele sunt trimise către consumatori care au cerut mesaje pentru o anumită cheie.
- **Topic Exchange**: Mesajele sunt distribuite consumatorilor care sunt abonați la un anumit topic.

- **Funcții principale**:
  - `receive_incoming_data`: Primește date de la clienți (producători sau consumatori).
  - `handle_producer_publish`: Tratează mesajele trimise de producători.
  - `handle_consumer_request`: Tratează cererile de la consumatori.
- **Flux de execuție**:
  1. Inițializează un server care ascultă pe portul `8080`.
  2. Acceptă conexiuni de la producători și consumatori.
  3. Procesează mesajele primite și le redirecționează corespunzător.
  4. Loghează activitatea în `server_log.txt`.

## Instrucțiuni de Instalare

1. **Clonare și Construire**:
   - Descarcă sursa proiectului.
   - Rulează `make` pentru a construi proiectul.

2. **Rularea Proiectului**:
   - Începe mai întâi serverul:
     ```
     ./server
     ```
   - Apoi rulează producătorii și consumatorii:
     ```
     ./producer
     ./consumer
     ```
