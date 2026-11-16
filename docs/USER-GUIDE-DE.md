# TeoPCE-Benutzerhandbuch

[Projektstartseite](../README.md) · [Visueller Rundgang](TOUR.md)

**Handbücher:** [English](USER-GUIDE.md) · Deutsch · [Français](USER-GUIDE-FR.md) · [Español](USER-GUIDE-ES.md) · [Italiano](USER-GUIDE-IT.md) · [Українська](USER-GUIDE-UK.md)

Die Programmoberfläche ist derzeit auf Englisch. Dieses Handbuch übersetzt die Anleitung; die Schaltflächen heißen wie im Programm.

Für **Catalog Manager 0.2.0-rc4** und **TeoPCE 0.9.13**. Erstelle deinen eigenen Katalog aus deinen eigenen Spielen: Weder die Sammlung des Autors noch ein KI-Abonnement oder ein API-Schlüssel sind erforderlich.

![Die Spielebibliothek auf der Konsole in der Cover-Rasteransicht](images/console-grid.png)

*Konsolenoberfläche als Vorschau im Emulator; Beispielsammlung im Desktop-Manager. [Herkunft der Screenshots](SCREENSHOTS.md).*

## Vorbereitungen

Verwende ein **PC Engine / CoreGrafx + Turbo EverDrive Pro** mit einer SD-Karte, die bereits im nativen EverDrive-Menü funktioniert, und für CD-Spiele dein eigenes funktionierendes CD-BIOS. Befolge Krikzz' Anweisungen zu Modul, Menü und Firmware; TeoPCE installiert oder aktualisiert keine Firmware.

Sichere die SD-Karte, insbesondere die Spielstände in `edturbo` und den vorhandenen Zustand in `00TEOPCE/user`. Die automatische Sicherung des Managers umfasst **TeoPCE.pce und 00TEOPCE**, nicht die gesamte Karte. Behalte das funktionierende Dateisystem bei: Die Installation von TeoPCE erfordert keine Neuformatierung.

Schalte die Konsole aus, bevor du die Karte entnimmst. Verwende ein Kartenlesegerät am Computer; USB-Debugging am Modul ist nicht nötig.

## Herunterladen und öffnen

Lade die ZIP-Datei für deine Plattform aus dem [RC4-Release](https://github.com/marvelfamily/TeoPCE/releases/tag/v0.2.0-rc4) herunter und entpacke sie vollständig. Das automatisch erzeugte Quellcode-Archiv von GitHub ist für die Entwicklung gedacht und keine eigenständig lauffähige Anwendung.

**Mac:** Öffne `TeoPCE Catalog Manager.app`. Dieser Download ist für Apple Silicon, ad hoc signiert und nicht von Apple notarisiert. Wird der erste Start blockiert, prüfe Download und Prüfsumme und verwende anschließend, falls angeboten, Apples **Systemeinstellungen → Datenschutz & Sicherheit → Dennoch öffnen**. Deaktiviere Gatekeeper nicht global. Es gibt keinen gepackten Build für Intel-Macs.

**Windows:** Starte die entpackte `TeoPCE Catalog Manager.exe`, nicht die Datei innerhalb der ZIP-Ansicht. Sie ist nicht signiert; Windows kann deshalb vor einer unbekannten App warnen. Prüfe Quelle und Prüfsumme, bevor du über den Start entscheidest. RC4 wurde unter Windows 11 ARM64 mit x64-Emulation getestet; ein späterer, auf GitHub gehosteter Windows-Build bestand ebenfalls seine Startprüfung. Ein vollständiger manueller Test der verteilten Binärdatei unter nativem Intel-/AMD-Windows steht noch aus.

Die gepackten Apps benötigen keine separate Python-Installation. Vergleiche zur Prüfung einer ZIP-Datei deren vollständigen Hashwert mit `SHA256SUMS.txt` aus demselben Release:

```sh
# macOS Terminal
shasum -a 256 TeoPCE-Manager-0.2.0-rc4-macOS-arm64.zip
```

```powershell
# Windows PowerShell
Get-FileHash .\TeoPCE-Manager-0.2.0-rc4-Windows-x64.zip -Algorithm SHA256
```

## Auswählen und vorbereiten

Wähle unter **Your card** mit **Choose card…** das **Stammverzeichnis der SD-Karte, das edturbo enthält**, nicht `edturbo` selbst. Wurde eine Karte automatisch erkannt, prüfe den angezeigten Pfad.

Klicke auf **Prepare collection**. Der Manager durchsucht Spiele, berechnet Hashwerte, prüft CD-Track-Verweise, gleicht Metadaten und Cheats ab und lädt verfügbare Bilder herunter. Große CD-Sammlungen benötigen Zeit, da alle Tracks gelesen werden. **Activity** zeigt die tatsächlichen Arbeitsschritte; lasse die Karte verbunden.

Die Vorbereitung erstellt ein separates Paket auf dem Computer und installiert nichts auf der SD-Karte. Prüfe vor der Installation **View skipped games & warnings**.

### Dateiaufbau und Formate

Spiele können in ihren Ordnern bleiben. Beispiel, kein vorgeschriebenes Namensschema:

```text
SD root/
├── edturbo/               native EverDrive-Daten, BIOS und Spielstände
├── HuCard/Example.pce
├── CD/Example/
│   ├── Example.cue
│   ├── Track 01.bin
│   └── Track 02.bin
├── TeoPCE.pce             durch die Installation hinzugefügt
└── 00TEOPCE/              durch die Installation hinzugefügt
    ├── data/              Katalog, Bilder, Details und Quellenangaben
    ├── cheats/            Pakete pro Spiel
    └── user/              Einstellungen und Cheat-Auswahl
```

- Verwende `.pce` oder `.cue` zusammen mit **allen** referenzierten Tracks; Dateinamen und Groß-/Kleinschreibung müssen mit der CUE-Datei übereinstimmen.
- ZIP, 7z, CHD, CCD und M3U werden nicht direkt eingelesen. Entpacke oder konvertiere deine eigenen Medien zuerst.
- `MODE1/2048` benötigt eine gesondert überprüfte Konvertierung in Rohsektoren. Der Manager meldet dies, ohne das Disc-Image umzuschreiben.
- Relative Spielpfade müssen aus druckbaren ASCII-Zeichen bestehen und kürzer als 128 Byte sein. Es werden höchstens 1.200 Katalogkandidaten unterstützt.
- SuperGrafx-Dateien können erkannt werden, sind auf CoreGrafx jedoch ausgeblendet; ihr Start wird in dieser Version nicht unterstützt.
- Verschiedene HuCard-Revisionen mit demselben Basisnamen können bei der nativen Speicherung von Spielständen kollidieren; gemeldete Kollisionen erfordern eine gezielte Umbenennung.
- Versteckte und mit Unterstrich beginnende Ordner sowie `edturbo` und `00TEOPCE` sind von der normalen Spielsuche ausgeschlossen.

## Prüfen und bearbeiten

![Die Sammlung im Catalog Manager mit Spielauswahl und Details](images/manager-collection.png)

**Collection** bietet eine Suche, Zahlen zur Abdeckung und **Missing covers**. `Cmd+F` / `Ctrl+F` setzt den Fokus auf die Suche; `Esc` leert sie. Wähle ein Spiel, um **Game card**, **Edit** oder **Cheats** anzusehen.

Ändere unter Edit Titel, Jahr, Kategorie oder Beschreibung oder wähle mit **Choose cover…** ein Cover (PNG/JPG/JPEG). Halte das gewählte Bild für die Vorbereitung verfügbar. Lasse ein unbekanntes Jahr leer. Verwende kurze englische Texte: Der begrenzte Zeichensatz der Konsole normalisiert Nicht-ASCII-Zeichen und kann sie weglassen.

Klicke auf **Save edits → Refresh collection**, prüfe alles erneut und installiere dann. Eine Bearbeitung allein aktualisiert die Karte nicht; die Installation bleibt deaktiviert, solange die vorbereitete Ausgabe nicht aktuell ist.

### Was wird online abgerufen?

| Inhalt | Verhalten |
| --- | --- |
| Fakten und Cheats | Enthaltene, festgelegte Datenstände; kein Konto nötig |
| Cover/Screenshots | Verfügbare Bilder von Libretro Thumbnails werden heruntergeladen und lokal zwischengespeichert |
| Beschreibungstexte | In Edit eingegeben oder über die CLI aus einer eigenen vorbereiteten Referenz übernommen |
| Fehlende Bilder | Klar gekennzeichnet; bei Bedarf eigenes Cover hinzufügen |
| Offline arbeiten | Nur mitgelieferte Daten, Cache und lokale Bilder; nicht verfügbare Bilder fehlen weiterhin |

Nicht jede Veröffentlichung ist vollständig abgedeckt. Fakten werden nicht erfunden. Spiele werden lokal gelesen und nicht hochgeladen; Bildanfragen nennen dem ausliefernden Host den angefragten Titel. Informationen zu Quellen und fehlenden Bildern stehen im Aktivitätsprotokoll und im Bericht.

## Installieren und starten

Wähle **Install on card**, bestätige die Nachfrage und lasse die SD-Karte verbunden, bis **Installed and verified** erscheint. Der Installer prüft, ob sich Spiele oder Anwendungszustand geändert haben, verifiziert eine Sicherung auf dem Computer, bereitet die neue Anwendung zur Installation vor und überprüft den installierten Satz. Bei erkannten Fehlern wird ein Rollback versucht; nach Abziehen oder Stromausfall müssen Karte und Sicherung dennoch geprüft werden.

Wirf die Karte im Betriebssystem sicher aus, stecke sie in die ausgeschaltete Konsole, starte das EverDrive-Menü und öffne **TeoPCE.pce** aus dem Stammverzeichnis. ROM und `00TEOPCE` müssen aus derselben vorbereiteten Generation stammen. Mische keine einzelnen Katalogdateien und verwende keinen von einer anderen Person erzeugten Katalog.

## Konsolensteuerung

Die Fußzeile zeigt die Steuerung für den aktuellen Bildschirm.

| Bildschirm | Steuerung |
| --- | --- |
| Spieleliste | Steuerkreuz auswählen/blättern; **I** Details; **SELECT** Suche/Filter; **RUN** spielen |
| Details | **I** Bild; **II** zurück; **SELECT** Spielmenü; **RUN** spielen; hoch/runter Text; links/rechts Spiel |
| Spielmenü | Start, Favorit, Cheats, Dateiinformationen, zurück zu EverDrive |
| Cheats | **I** umschalten/Anleitung; **SELECT** Optionen; **II** abbrechen; links/rechts Seite; **RUN** speichern und zurück |

### Durchsuchen, Suche und Filter

Öffne in der Bibliothek mit **SELECT** **FIND AND FILTERS**. Hoch/runter wählt eine Option; links/rechts ändert sie. **I** übernimmt die Einstellungen oder öffnet bei den entsprechenden Zeilen die Titel-, Genre- oder Buchstabenauswahl. **II** bricht ab. **RUN** setzt die Filterauswahl zurück; übernimm sie, um wieder alle Spiele anzuzeigen.

| Option | Auswahl |
| --- | --- |
| Title prefix | Titelanfang über die Bildschirmtastatur eingeben, bis zu 12 Zeichen; RUN sucht |
| Library | Alle Spiele, Favoriten, zuletzt geöffnet |
| Category | Originalveröffentlichungen, Übersetzungen, Homebrew, Museum, Tools / Tests, nicht lizenzierte oder alle Kategorien |
| Media | HuCard, CD-ROM oder beides |
| Genre | Auswahl aus einer seitenweise angezeigten Liste, etwa Action, Adventure, Platform, Puzzle, RPG oder Shoot'em Up |
| Letter | A–Z, mit Zahlen/Symbolen beginnende Titel über `#` oder alle Buchstaben |
| Region/lang | Japan, USA, englischer Patch, Other oder alle |
| Cheats | Beliebige Spiele oder Spiele mit auswählbaren Cheats |
| View | Liste mit Bildern oder Raster mit sechs Covern |
| Theme | Neon, Ember, Ice, Classic |

Filter werden kombiniert. Wähle beispielsweise **Original releases + HuCard + Action**. Die normale Bibliothek ist A–Z sortiert; Recently opened verwendet die Reihenfolge des Verlaufs. Eine Sortierung nach Jahr oder Publisher gibt es in dieser Version nicht. Filter ändern nur die angezeigte Liste; sie verschieben keine Dateien. **II** in einer gefilterten Bibliothek setzt die Filter zurück.

Der ausgewählte Titel steht über der Liste bzw. dem Raster und wird bei längeren Namen umgebrochen. Die Details zeigen Medium, Region, Genre, Jahr, sofern bekannt, sowie verfügbare Beschreibung und Angaben zu Entwickler und Publisher. **I** wechselt, sofern vorhanden, zwischen Cover und Spiel-Screenshot; hoch/runter scrollt den Text, links/rechts wechselt das Spiel.

Füge einen Favoriten über **Details → SELECT → Add/Remove favorite** hinzu oder entferne ihn dort. Bis zu 64 Favoriten und acht zuletzt geöffnete Spiele bleiben gespeichert. Der Verlauf erfasst geöffnete Spielkarten, keine gemessene Spielzeit. Beim Neustart wählt TeoPCE den neuesten verfügbaren Verlaufseintrag in der ungefilterten Bibliothek. Ansichts- und Theme-Einstellungen werden ebenfalls gespeichert.

Kurze Tags beschreiben den aktuellen Filter: `FAV` Favoriten, `OPEN` zuletzt geöffnet, `B*` Titel mit B am Anfang, `R:JP` Japan, `R:US` USA, `L:EN` englischer Patch und `CH` auswählbare Cheats. Der Titelbereich nennt auch Medium und Region des ausgewählten Spiels; `*` markiert einen Favoriten.

**Details → SELECT** öffnet außerdem Spielaktionen: Start, Favoriten, Cheats, Dateiinformationen und Rückkehr zu EverDrive. Die Dateiinformationen helfen, den ausgewählten Katalogeintrag bzw. Pfad bei einer Problemmeldung zu identifizieren.

## Cheats

Der Manager gleicht Pakete automatisch ab. Sein Cheats-Tab zeigt Gruppen in der Vorschau; aktiviert wird **auf der Konsole**:

1. Spiel → **I** Details → **SELECT** Spielmenü → **CHEATS**.
2. Gruppe markieren, **I** drücken und vorhandene Anweisungen lesen.
3. **RUN** speichert die Auswahl und kehrt zurück. Starte das Spiel.
4. **SELECT → All cheats off** deaktiviert und speichert sofort alle Gruppen für dieses Spiel. Ein zusätzliches RUN ist nicht nötig.

Gespeicherte Auswahlen bleiben erhalten; das Menü zeigt aktive Codes an. **II** verwirft ungespeicherte Änderungen der Auswahl.

### Zuordnung, Grenzen und HELD

Die mitgelieferte Bibliothek enthält **1.177 vollständige Gruppen für 347 exakte Veröffentlichungen**. Der HuCard-Abgleich verwendet den vollständigen Datei-Hash. Der CD-Abgleich umfasst die exakten CUE-Bytes und alle referenzierten Tracks einschließlich Audio. Andere Revisionen, Übersetzungen oder neu erstellte CUE-Dateien passen möglicherweise nicht. Benenne kein Paket um, um Kompatibilität zu erzwingen.

**53 CD-Zuordnungen benötigen zusätzlich den erfassten relativen Pfad**, da kompilierte Startrichtlinien davon abhängen. Der Manager zeigt diesen Pfad an, wenn er abweicht. Das Verschieben eines installierten Spiels kann gespeicherten Zustand beeinflussen: Verwende eine gesicherte neue Einrichtung oder eine gezielte Migration.

Einige Kombinationen haben inkompatible CD-Startschutzmechanismen und werden blockiert. Das Modul bietet außerdem ein Budget von 16 Slots; eine Gruppe kann mehrere Slots belegen und bleibt als Ganzes erhalten. Reduziere bei Bedarf die Auswahl.

**HELD bedeutet bekannte Probleme oder nicht unterstützte Daten**, nicht bloß „nicht auf der Konsole getestet“. Diese standardmäßig ausgeblendeten Einträge lassen sich über **Show held codes** ansehen. Geeignete, aufgrund von Richtlinien zurückgehaltene Einträge bieten **Accept risk** mit einer Erklärung; fehlerhafte oder nicht unterstützte Codes lassen sich nicht freischalten. Kombinationsprüfungen gelten weiterhin. Das Ausblenden von HELD deaktiviert keine aktiven Codes.

Anweisungen können eine Aktion auf dem Titelbildschirm, eine bestimmte Figur oder einen Modus verlangen. Ein erfolgreicher Start beweist nicht die Wirkung in jedem Level. Bei einem BIOS-Fehler mit einem Code: Codes löschen, mit dem nativen Start vergleichen und die genaue Gruppe/Revision melden.

**Import extra cheats…** akzeptiert TeoPCE-JSON-Bibliotheken der Version 1, keine beliebigen Emulator-Dateien. Explizite Importe haben Vorrang vor passenden mitgelieferten Inhalten; bereits installierte Pakete/Auswahlen haben bei Aktualisierungen Vorrang vor beiden. Siehe [CHEAT-LIBRARY.md](CHEAT-LIBRARY.md).

### Spielstände und IGM

Das native HuCard-IGM sowie Quick Save/Load wurden auf der Hardware des Besitzers erprobt. IGM führt zurück zu **Krikzz's menu**; starte TeoPCE von dort erneut.

Im getesteten CD-Aufbau gibt es **kein natives IGM und keinen Quick Save State**. Normale Speicherunterstützung durch Spiel oder BIOS ist davon getrennt. TeoPCE fügt kein Overlay im Spiel hinzu, um CD-Codes umzuschalten.

## Updates, Speicher und Wiederherstellung

Nach dem Hinzufügen von Spielen: **Prepare / Refresh collection → prüfen → Install on card**. Vorhandene Profile/Auswahlen bleiben erhalten, wenn ihre Identität weiterhin gültig ist. Bereite nach dem Spielen erneut vor, falls sich der Anwendungszustand geändert hat.

Das Löschen, Umbenennen oder Ersetzen installierter Spiele ist keine transparente Migration. Falls die vorherige Bibliothek nicht mehr übereinstimmt, behalte die Sicherung und löse die Migration; lösche keine Profile, um die Prüfung zu umgehen. Eine neue Karte kann unabhängig vorbereitet werden. Ein TeoPCE-Update aktualisiert nicht die EverDrive-Firmware.

| Unter dem Home-Ordner des Computers | Zweck |
| --- | --- |
| `TeoPCE/prepared/` | Vorbereitete Generationen und `REPORT.json` |
| `TeoPCE/metadata/` | Editor-Änderungen, dem Einhängepfad der Karte zugeordnet |
| `TeoPCE/backups/installation-…/` | Vorheriges ROM/Daten; `RESULT.json` nach erfolgreicher Installation |
| `.cache/teopce-library/` | Download-Cache |

Bewahre benötigte Änderungen und Sicherungen auf. Ein neuer Einhängepfad oder Windows-Laufwerksbuchstabe kann das Auffinden der Änderungen beeinflussen. Alte vorbereitete Ausgaben und der Cache können bei geschlossenem Manager entfernt werden; nach Entfernen des Caches sind neue Downloads nötig.

Zur Wiederherstellung: Schließe den Manager, kopiere die aktuelle `TeoPCE.pce` und `00TEOPCE` an einen anderen Ort und **ersetze dann beide** durch das Paar aus einer einzigen Sicherung. Führe keine Generationen zusammen. Lasse Spiele und `edturbo` unverändert. Eine Sicherung der Erstinstallation kann keines der beiden Anwendungselemente enthalten und keine vorherige Version wiederherstellen, die nie existiert hat. Wirf die Karte nach dem Kopieren sicher aus.

## Fehlerbehebung

| Symptom | Nächster Schritt |
| --- | --- |
| Karte nicht gefunden | Stammverzeichnis manuell wählen; prüfen, ob die Karte eingehängt ist und `edturbo` enthält |
| Spiele übersprungen | Bericht lesen; Format, CUE-Verweise, ASCII-Pfade, ausgeschlossene Ordner und MODE1/2048 prüfen |
| Auch im nativen Menü fehlgeschlagen | Image/BIOS/Karte untersuchen; TeoPCE repariert keine Medien |
| Nur TeoPCE-Start schlägt fehl | Titel/Pfad, Konsole/Firmware, genauen Fehler und Zustand mit ausgeschalteten Cheats festhalten |
| Keine kompatiblen Codes | Exakter Inhalt oder benötigter Pfad stimmt nicht überein; normaler Start bleibt möglich |
| Installation deaktiviert | Änderungen speichern und aktualisieren; Vorbereitung muss für die gewählte Karte abgeschlossen sein |
| Karte/App geändert | Erneut vorbereiten; keine alte Generation über einen neueren Zustand installieren |
| Jahr/Bild fehlt | Online-Vorbereitung versuchen oder eigene Angaben liefern; unbekanntes Jahr darf leer bleiben |
| Installation unterbrochen | Sicherungen und Staging-Dateien behalten, Karte prüfen; niemals eine Sperrdatei löschen, während ein Installer läuft |

Nutze das [Issue-Formular](https://github.com/marvelfamily/TeoPCE/issues/new/choose) mit Versionen, Schritten, Fehlertext und Vergleich zum nativen Start. Schwärze persönliche absolute Pfade. Hänge keine Spiele, BIOS-Dateien, Spielstände oder Karten-Dumps an.

Die noch ausstehenden manuellen, Plattform- und Hardwareprüfungen für RC4 stehen in [RELEASE-STATUS.md](../RELEASE-STATUS.md). Quellenangaben und Weiterverteilungsbedingungen stehen in [THIRD-PARTY.md](../THIRD-PARTY.md); vorbereitete Karten behalten Metadaten- und Cheat-Quellenangaben unter `00TEOPCE/data`.
