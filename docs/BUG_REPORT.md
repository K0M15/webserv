# Webserv — Mandatory-Teil Audit (2026-08-09, aktualisiert)

Getestet gegen `en.subject.pdf` (Version 24.0). Dies ist eine **Aktualisierung**
einer früheren Version dieses Reports (ebenfalls vom 2026-08-09) — seither wurden
mehrere Punkte im Code gefixt (Upload, Body-Size-Limit, Location-Matching,
Header-Case-Insensitivity). Alle Punkte unten wurden gegen den aktuellen
Code-Stand (`git log -1`: `cd40f0d`) neu verifiziert: entweder per `grep`/Code-Lesen,
per `g++ -std=c++98`-Kompilierversuch, oder live gegen den laufenden Server
(curl / rohe Python-Sockets, Testbefehle in Klammern).

**Fazit: Der Mandatory-Teil ist aktuell NICHT erfüllt.** C++98-Verstoß, der
`errno`-Check nach `read()` und die fehlende CGI-Ausführung sind laut Subject
Grund für eine automatische 0.

---

## ✅ Seit dem letzten Audit behoben (neu verifiziert)

- **Datei-Upload funktioniert jetzt.** `POST` auf eine Location mit
  konfiguriertem `upload_dir` schreibt die Datei und antwortet `201 Created`
  (`ConnectionManager.cpp:294-368`). „Clients must be able to upload files“ ist
  damit erfüllt — POST auf Locations *ohne* `upload_dir` liefert korrekt `403`.
- **`client_max_body_size` wird jetzt geparst und durchgesetzt.**
  `WebserverSettings::max_body_size` (Default 1 MB) wird sowohl früh gegen den
  rohen Lesepuffer (`ConnectionManager.cpp:94`) als auch gegen den geparsten
  Body (`ConnectionManager.cpp:322`) geprüft → `413` bei Überschreitung.
- **Location-Matching macht jetzt korrektes Longest-Prefix-Match.** Die neue
  Funktion `matchLocation()` (`ConnectionManager.cpp:223-243`) wählt die
  spezifischste Location mit Segment-Grenzen-Check, nicht mehr die
  alphabetisch erste — der alte Bug ist behoben.
- **Header-Lookup ist jetzt case-insensitive.** `Request::fromString`
  lowercased Header-Keys beim Parsen, `Request::getHeader()` lowercased den
  Such-Key ebenfalls (`Request.cpp:46-49`, `Request.hpp:29-43`).

---

## 🔴 Kritisch — führt laut Subject direkt zu 0 Punkten

### 1. Kein C++98 — Code kompiliert nicht mit `-std=c++98`
Verifiziert mit:
```
g++ -Wall -Wextra -Werror -std=c++98 -Iinclude -c src/Webserver.cpp
```
Schlägt sofort fehl: `<unordered_map>` erfordert C++11, dazu `noexcept`,
`nullptr` als reservierte Keywords in C++11 (`HttpServerException.hpp:12`,
`Webserver.cpp:11`). Der Code nutzt durchgehend weitere C++11/14/17-Features:
`auto`, Range-based-for mit *structured bindings* (`WebserverSettings.hpp:139`),
`std::optional` (`WebserverSettings.hpp:54,61-62`), `std::from_chars`
(`Request.cpp:58`), `= default`, In-Class-Default-Initializer,
`std::to_string`. → Praktisch ein Komplett-Rewrite, kein Patch.

### 2. Makefile nutzt `-std=c++17` statt C++98
`Makefile:4`: `CXXFLAGS = -Wall -Wextra -Werror -std=c++17 -Iinclude -g`.
Verstößt gegen „Your code must comply with the C++98 standard“.

### 3. `errno` wird nach `read()` geprüft, um Verhalten zu steuern
`ConnectionManager.cpp:83-89`:
```cpp
ssize_t n = read(fd, buf, sizeof(buf));
...
if (n < 0)
{
    if (errno == EAGAIN || errno == EWOULDBLOCK)   // <- verboten
        return;
    onClose(fd);
    return;
}
```
Subject S. 8 (rote Box): „Checking the value of errno to adjust the server
behaviour is strictly forbidden after performing a read or write operation.“

### 4. CGI ist komplett nicht implementiert
`grep` bestätigt: keine der External Functions `fork`, `execve`, `pipe`,
`dup`/`dup2`, `waitpid`, `socketpair` wird irgendwo verwendet. `cgi_extension`
wird zwar pro Location geparst (`WebserverSettings.cpp:177-179`), aber
`ConnectionManager::handleRequest` hat für POST nur den Kommentar
`// TODO: add CGI handler when merged` (`ConnectionManager.cpp:331`) — keine
Auswertung. Mandatory: „Your server should support at least one CGI.“

---

## 🟠 Mandatory-Features fehlen komplett

### 5. Mehrere `server {}`-Blöcke in einer Config-Datei funktionieren nicht
`ConfigReader::readConfigBlock()` (`ConfigReader.cpp:48-53`) setzt `res.first`
nie — bleibt immer `""`. In `readConfig()` (`ConfigReader.cpp:92`)
landet dadurch **jeder** Server-Block unter demselben Map-Key `""` in
`ConfigReader::data`; jeder neue Block überschreibt den vorherigen komplett.
`Webserver::setupListenSockets()` iteriert über `m_config.data` — mit nur
einem Eintrag wird nur der **letzte** Block der Datei tatsächlich gebunden.

**Live reproduziert (2026-08-09):** Config mit zwei Blöcken (Port 8091 +
8092, siehe `tests/sample_cfg/multi_server.config`-Stil):
```
curl http://127.0.0.1:8091/   →  Connection refused (curl exit 7)
curl http://127.0.0.1:8092/   →  200 OK, "site2"
```
Serverlog zeigt nur `Listening on 0.0.0.0:8092`. → „Your server must be able
to listen to multiple ports to deliver different content“ ist nicht erfüllt,
sobald mehr als ein `server{}`-Block in derselben Datei steht — betrifft auch
das eigene Testfile `tests/sample_cfg/multi_server.config` (2 Blöcke, Port
4000+5000).

### 6. HTTP-Redirection konfigurierbar, aber wirkungslos
`LocationConfig::redirect` wird geparst (`WebserverSettings.cpp:173-176`),
aber `grep -rn "redirect" src/ConnectionManager.cpp` findet **keinen** Treffer
— das Feld wird beim Request-Handling nirgends gelesen. Totes Config-Feld.

### 7. Erlaubte HTTP-Methoden pro Route nicht umsetzbar
`LocationConfig::methods` existiert als Feld (`WebserverSettings.hpp:55`),
aber der Location-Block-Parser in `WebserverSettings.cpp` (Zeilen 143-205)
kennt kein `methods`-Keyword — es wird nie befüllt. `handleRequest()` prüft
außerdem nie, ob die Methode für die getroffene Location erlaubt ist
(`grep` auf `->methods`/`.methods` in `ConnectionManager.cpp`: 0 Treffer).
„List of accepted HTTP methods for the route“ fehlt komplett.

### 8. Keine konfigurierbaren Error-Pages
Kein `error_page`-Directive im Parser (`grep -rn "error_page" src include`:
0 Treffer). Fehlerseiten sind ausschließlich hart codiert in
`HttpResponse::error()` (`HttpResponse.cpp:89-96`). Der eingebaute Default
erfüllt „must have default error pages if none are provided“, aber das
Config-Feature „Set up default error pages“ (Chapter IV.3) fehlt.

---

## 🟡 Reproduzierte Bugs in vorhandenen Features

### 9. Uncaught Exception bei ungültigem `Content-Length` → Verbindung hängt
`ConnectionManager::isRequestComplete()` (`ConnectionManager.cpp:169-196`)
sucht selbst (case-sensitiv!) nach `"content-length:"` im rohen Lesepuffer
und ruft `std::stoul(len_str)` **ohne try/catch** auf. Diese Methode wird
direkt aus `onReadable()` aufgerufen (`ConnectionManager.cpp:100`), außerhalb
jeder try/catch-Umgebung — im Gegensatz zu `Request::fromString()` (aufgerufen
in `handleRequest()`, dort mit try/catch), das mit `std::from_chars` bereits
sauber validiert und wirft.

**Live reproduziert** per rohem Python-Socket mit `content-length:
notanumber` (klein geschrieben, damit `isRequestComplete`s eigene
Suche greift statt der bereits robusten von `Request::fromString`):
- Server-Log: `Event loop error: stoul` (Exception erst ganz oben in
  `Webserver::run()` gefangen)
- Die betroffene Verbindung bekommt **nie** eine Antwort (Client-Timeout nach
  2s bestätigt) — verstößt gegen „A request to your server should never hang
  indefinitely“ (hängt bis zum 60s-Timeout in `checkTimeouts`)
- Andere Clients werden weiterhin bedient (zweiter Test-Client im selben Lauf
  bekam normal `200 OK`) — kein Totalausfall, aber die eine Verbindung ist
  dauerhaft blockiert.

### 10. `DELETE` meldet immer Erfolg, auch wenn das Löschen fehlschlägt
`ConnectionManager.cpp:403-405`:
```cpp
if (!std::remove(path.c_str()))
    sendResponse(conn, HttpResponse::error(500));
sendResponse(conn, HttpResponse::error(204));   // <- läuft immer, ohne else/return
```
`std::remove()` gibt `0` bei Erfolg zurück, `!0` ist `true` → bei Erfolg wird
zuerst fälschlich ein 500 aufgebaut, dann sofort von der zweiten,
unbedingten `sendResponse(...204)`-Zeile überschrieben und gesendet. Bei
echtem Fehlschlag (`remove()` gibt `-1`) ist `!(-1)` = `false` — der 500er
wird übersprungen und trotzdem `204` gesendet.

**Live reproduziert:** Datei in `chmod 555`-Verzeichnis via `DELETE`
angefragt → Server antwortet `204 No Content`, `ls` zeigt die Datei danach
weiterhin vorhanden. Löschfehler werden also stillschweigend als Erfolg
gemeldet.

### 11. `GET` auf Verzeichnis ohne trailing slash liefert leeres `200`
In `handleRequest()` (`ConnectionManager.cpp:267-270`) wird der Index nur
angehängt, wenn `url_path` mit `/` endet. Fehlt der Slash (`GET /subdir`
statt `/subdir/`), versucht der Code `std::ifstream(root + "/subdir")` zu
öffnen — das gelingt unter Linux für Verzeichnisse (`is_open() == true`),
liefert aber keinen lesbaren Inhalt.

**Live reproduziert:** `GET /subdir` → `200 OK`, `Content-Length: 0`, statt
Redirect auf `/subdir/` oder Ausliefern von `subdir/index.html`.

---

## ⚪ Kleinere Abweichungen

### 12. Kein Default-Config-Pfad
`main.cpp:20` verlangt zwingend `argc == 2` und bricht sonst ab. Subject
erlaubt alternativ einen Default-Pfad („provided as an argument … or
available in a default path“) — nicht implementiert.

### 13. Keep-Alive ist toter Code
`grep -rn "keep_alive" src/*.cpp` zeigt: `conn.keep_alive` wird nur im
Konstruktor auf `false` gesetzt und im Exception-Handler explizit auf
`false` — nirgends auf `true`. `HttpResponse::m_keep_alive` ebenso nie
`true` gesetzt. Jede Antwort schließt die Verbindung (`Connection: close`),
obwohl `onWritable()` (`ConnectionManager.cpp:129-150`) eine vollständige
State-Machine für Connection-Reuse enthält, die nie erreicht wird. Nicht
mandatory (HTTP/1.0 als Referenz reicht laut Subject), aber nutzloser toter
Pfad.

### 14. Kein Chunked-Transfer-Encoding
Weder `Request::fromString` noch `isRequestComplete` kennen
`Transfer-Encoding: chunked`. Das Subject verlangt Un-Chunking explizit für
CGI-Requests (Chapter IV.3) — da CGI (Punkt 4) ohnehin fehlt, ist dies aktuell
kein eigenständiger Blocker, muss aber mitgebaut werden, sobald CGI kommt.

### 15. Makefile ohne Header-Dependency-Tracking
Die Pattern-Rule `$(OBJ_DIR)/%.o: src/%.cpp` (`Makefile:44-45`) hat kein
`-MMD`/Dependency-Files — Header-Änderungen lösen kein Rebuild der
abhängigen `.cpp`-Dateien aus. Kein Verstoß gegen „no unnecessary
relinking“, aber ein Build-Hygiene-Problem.

---

## 📄 README.md — Chapter V Requirements nicht erfüllt

- **Erste Zeile weiterhin fehlerhaft:** `README.md:1` lautet
  `*This project has been created as part of the 42 curriculum by ,
  afelger*` — der erste Login vor dem Komma fehlt. Gefordertes Format:
  `<login1>[, <login2>[, <login3>[...]]]`.
- **Keine Sections „Description“, „Instructions“, „Resources“** als eigene
  Überschriften vorhanden. Das README hat stattdessen „Outline“,
  „Requirements“, „Configuration File“, „Readme“ (dies ist lediglich ein
  wörtliches Zitat der Subject-Anforderungen) und „Architecture“.
- Keine Beschreibung, wie/wofür AI eingesetzt wurde (explizit im
  „Resources“-Abschnitt gefordert).

---

## Zusammenfassung: Mandatory-Parts-Status

| Feature (Subject) | Status |
|---|---|
| C++98, kompiliert mit `-std=c++98` | ❌ fehlt komplett |
| Kein `errno`-Check nach read/write | ❌ verletzt |
| CGI-Ausführung | ❌ fehlt komplett |
| Datei-Upload | ✅ implementiert (POST + `upload_dir`) |
| Max. Body-Size (Config + Durchsetzung) | ✅ implementiert (413) |
| GET / POST / DELETE | ⚠️ GET (#11) & DELETE (#10) fehlerhaft, POST funktioniert |
| Mehrere Ports/Server-Blöcke | ❌ Config-Bug — nur letzter Block einer Datei aktiv |
| HTTP-Redirection (Config) | ❌ geparst, aber wirkungslos |
| Methoden-Restriktion pro Route | ❌ fehlt komplett (Parser + Handler) |
| Default/konfigurierbare Error-Pages | ⚠️ nur hart codierter Default, kein Config-Override |
| Directory Listing (on/off) | ✅ funktioniert |
| Location-Matching (Longest-Prefix) | ✅ jetzt korrekt |
| Header-Lookup case-insensitive | ✅ jetzt korrekt |
| Statisches Website-Serving | ⚠️ funktioniert grundsätzlich, aber Bug #11 |
| Single `poll()` für alles | ✅ strukturell korrekt (`PollHandler`-Singleton) |
| Non-blocking I/O | ✅ Sockets stehen auf `O_NONBLOCK` |
| Verbindung hängt nie | ❌ verletzt durch Bug #9 |
| README-Anforderungen | ❌ mehrere Punkte nicht erfüllt |
| Bonus (Cookies, Multi-CGI) | ❌ nicht bewertbar, da Mandatory nicht erfüllt |

**Empfehlung (Priorität):**
1. C++98-Migration und `errno`-Verstoß (#1–3) — beide sind explizite
   0-Punkte-Kriterien.
2. Config-Block-Bug (#5) — vermutlich der am leichtesten zu fixende Bug
   (eindeutiger Key statt `""` beim Einfügen in `ConfigReader::data`), aber
   mit großer Wirkung, da er mehrere Server/Ports komplett unbrauchbar macht.
3. CGI (#4) — komplett fehlendes Mandatory-Feature, größter Implementierungsaufwand.
4. `stoul`-Hang (#9) und `DELETE`-Bug (#10) — kleine, gezielte Fixes mit
   direkter Auswirkung auf „never hang indefinitely“ bzw. korrekte
   Status-Codes.
5. Methoden-Restriktion (#7), Redirect (#6), Error-Pages (#8) — jeweils
   überschaubare Parser- + Handler-Ergänzungen.
6. README (Chapter V) — schnell zu fixen, aber vollständiger Punkteverlust,
   wenn übersehen.
