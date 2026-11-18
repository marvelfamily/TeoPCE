# Guide utilisateur TeoPCE

[Accueil du projet](../README.md) · [Visite en images](TOUR.md)

**Guides :** [English](USER-GUIDE.md) · [Deutsch](USER-GUIDE-DE.md) · Français · [Español](USER-GUIDE-ES.md) · [Italiano](USER-GUIDE-IT.md) · [Українська](USER-GUIDE-UK.md)

L’interface de l’application est actuellement en anglais. Ce guide traduit les instructions ; les noms des boutons correspondent à ceux de l’application.

Pour **Catalog Manager 0.2.0-rc6** et **TeoPCE 0.9.15**. Créez votre propre catalogue à partir de vos jeux : la collection de l’auteur, un abonnement à une IA ou une clé API ne sont pas nécessaires.

![Bibliothèque de jeux sur la console affichée en grille de jaquettes](images/console-grid.png)

*Aperçu de l’interface console dans un émulateur ; collection d’exemple sur ordinateur. [Origine des captures](SCREENSHOTS.md).*

## Avant de commencer

Utilisez une **PC Engine / CoreGrafx avec Turbo EverDrive Pro**, une carte SD qui fonctionne déjà dans le menu natif EverDrive et, pour les jeux CD, votre propre BIOS CD fonctionnel. Suivez les instructions de Krikzz concernant la cartouche, le menu et le firmware ; TeoPCE n’installe ni ne met à jour le firmware.

Sauvegardez la carte SD, notamment les sauvegardes de `edturbo` et l’état existant dans `00TEOPCE/user`. La sauvegarde automatique du gestionnaire couvre **TeoPCE.pce et 00TEOPCE**, pas toute la carte. Conservez le système de fichiers fonctionnel : l’installation de TeoPCE ne nécessite pas de reformatage.

Éteignez la console avant de retirer la carte. Utilisez un lecteur de cartes sur ordinateur ; le débogage USB de la cartouche n’est pas nécessaire.

## Télécharger et ouvrir

Téléchargez le ZIP de votre plateforme depuis la [version RC6](https://github.com/marvelfamily/TeoPCE/releases/tag/v0.2.0-rc6) et extrayez-le entièrement. L’archive source générée automatiquement par GitHub est destinée au développement ; ce n’est pas une application autonome.

Si RC4 est déjà installé, sauvegardez la carte et remplacez le fichier `TeoPCE.pce` à la racine par [la nouvelle version](https://github.com/marvelfamily/TeoPCE/releases/download/v0.2.0-rc6/TeoPCE.pce). Conservez `00TEOPCE`, vos jeux et tout journal de récupération existant. Si le dossier `edturbo/gamedata` manque, créez-le et laissez-le vide. Éjectez la carte et lancez TeoPCE. Utilisez le gestionnaire RC6 pour la prochaine mise à jour du catalogue.

**Mac :** ouvrez `TeoPCE Catalog Manager.app`. Ce téléchargement est destiné à Apple Silicon, signé ad hoc et non notarié par Apple. Si le premier lancement est bloqué, vérifiez le téléchargement et sa somme de contrôle, puis utilisez, si proposé, **Réglages Système → Confidentialité et sécurité → Ouvrir quand même**. Ne désactivez pas Gatekeeper globalement. Il n’existe pas de version empaquetée pour Mac Intel.

**Windows :** exécutez `TeoPCE Catalog Manager.exe` après extraction, pas la copie affichée à l’intérieur du ZIP. L’application n’est pas signée ; Windows peut donc afficher un avertissement d’application inconnue. Vérifiez la source et la somme de contrôle avant de décider de l’exécuter. RC4 a été testé par émulation x64 sur Windows 11 ARM64 ; une compilation Windows ultérieure hébergée sur GitHub a également réussi sa vérification de démarrage. Un test manuel complet du binaire distribué sur Windows Intel/AMD natif reste à effectuer.

Les applications empaquetées ne nécessitent pas d’installation séparée de Python. Pour vérifier un ZIP, comparez son empreinte complète avec `SHA256SUMS.txt` de la même version :

```sh
# macOS Terminal
shasum -a 256 TeoPCE-Manager-0.2.0-rc6-macOS-arm64.zip
```

```powershell
# Windows PowerShell
Get-FileHash .\TeoPCE-Manager-0.2.0-rc6-Windows-x64.zip -Algorithm SHA256
```

## Sélectionner et préparer

Dans **Your card**, utilisez **Choose card…** et sélectionnez la **racine de la carte SD contenant edturbo**, et non `edturbo` lui-même. Si une carte a été détectée automatiquement, vérifiez le chemin affiché.

Cliquez sur **Prepare collection**. Le gestionnaire recherche les jeux, calcule leurs empreintes, vérifie les références aux pistes CD, associe les métadonnées et les cheats, puis télécharge les images disponibles. Les grandes collections CD prennent du temps, car toutes les pistes sont lues. **Activity** montre le travail réellement effectué ; laissez la carte connectée.

La préparation crée un paquet distinct sur l’ordinateur et n’installe rien sur la carte SD. Consultez **View skipped games & warnings** avant l’installation.

### Organisation des fichiers et formats

Les jeux peuvent rester dans leurs dossiers. Exemple : ce nommage n’est pas obligatoire.

```text
SD root/
├── edturbo/               données EverDrive natives, BIOS et sauvegardes
├── HuCard/Example.pce
├── CD/Example/
│   ├── Example.cue
│   ├── Track 01.bin
│   └── Track 02.bin
├── TeoPCE.pce             ajouté par l’installation
└── 00TEOPCE/              ajouté par l’installation
    ├── data/              catalogue, images, détails et attributions
    ├── cheats/            paquets par jeu
    └── user/              paramètres et sélection des cheats
```

- Utilisez `.pce` ou `.cue` accompagné de **toutes** les pistes référencées ; noms des fichiers et casse doivent correspondre au CUE.
- ZIP, 7z, CHD, CCD et M3U ne sont pas acceptés directement. Extrayez ou convertissez d’abord vos propres médias.
- `MODE1/2048` nécessite une conversion en secteurs bruts vérifiée séparément. Le gestionnaire le signale sans réécrire l’image disque.
- Les chemins relatifs des jeux doivent utiliser des caractères ASCII imprimables et compter moins de 128 octets. Au maximum 1 200 candidats au catalogue sont pris en charge.
- Les fichiers SuperGrafx peuvent être reconnus, mais sont masqués sur CoreGrafx ; leur lancement n’est pas pris en charge dans cette version.
- Différentes révisions HuCard ayant le même nom de base peuvent entrer en collision dans le stockage natif des sauvegardes ; les collisions signalées nécessitent un renommage réfléchi.
- Les dossiers cachés ou commençant par un trait de soulignement, ainsi que `edturbo` et `00TEOPCE`, sont exclus de l’analyse normale des jeux.

## Vérifier et modifier

![Collection dans le Catalog Manager avec un jeu sélectionné et ses détails](images/manager-collection.png)

**Collection** propose une recherche, des compteurs de couverture et **Missing covers**. `Cmd+F` / `Ctrl+F` place le curseur dans la recherche ; `Esc` l’efface. Sélectionnez un jeu pour consulter **Game card**, **Edit** ou **Cheats**.

Dans Edit, modifiez le titre, l’année, la catégorie ou la description, ou choisissez une jaquette avec **Choose cover…** (PNG/JPG/JPEG). Gardez l’image choisie disponible pour la préparation. Laissez l’année vide si elle est inconnue. Utilisez des textes anglais concis : le jeu de caractères limité de la console normalise les caractères non ASCII et peut les supprimer.

Cliquez sur **Save edits → Refresh collection**, vérifiez à nouveau, puis installez. Une modification seule ne met pas la carte à jour ; l’installation reste désactivée tant que le résultat préparé n’est pas à jour.

### Que trouve-t-on en ligne ?

| Contenu | Fonctionnement |
| --- | --- |
| Faits et cheats | Instantanés inclus et figés ; aucun compte nécessaire |
| Jaquettes/captures d’écran | Images disponibles de Libretro Thumbnails téléchargées et mises en cache localement |
| Descriptions rédigées | Fournies dans Edit ou depuis votre propre référence préparée via la CLI |
| Images manquantes | Clairement signalées ; ajoutez votre propre jaquette si vous le souhaitez |
| Travail hors ligne | Données incluses, cache et images locales uniquement ; les images indisponibles restent manquantes |

La couverture n’est pas complète pour toutes les versions. Les faits ne sont pas inventés. Les jeux sont lus localement et ne sont pas envoyés ; les requêtes d’images communiquent le titre demandé à l’hôte qui les fournit. Consultez le journal d’activité et le rapport pour les sources et les images manquantes.

## Installer et lancer

Choisissez **Install on card**, confirmez la demande et laissez la carte SD connectée jusqu’à l’apparition de **Installed and verified**. L’installateur vérifie les changements des jeux ou de l’état de l’application, vérifie une sauvegarde sur l’ordinateur, prépare la nouvelle application et vérifie l’ensemble installé. En cas d’échec détecté, il tente un retour arrière ; après une déconnexion ou une coupure de courant, vérifiez malgré tout la carte et la sauvegarde.

Éjectez la carte en toute sécurité dans le système d’exploitation, insérez-la dans la console éteinte, démarrez le menu EverDrive et lancez **TeoPCE.pce** depuis la racine. La ROM et `00TEOPCE` doivent provenir de la même génération préparée. Ne mélangez pas des fichiers de catalogue individuels et n’utilisez pas le catalogue généré par une autre personne.

## Commandes sur la console

Le bas de l’écran affiche les commandes disponibles pour l’écran actuel.

| Écran | Commandes |
| --- | --- |
| Liste des jeux | Croix directionnelle : sélectionner/changer de page ; **I** détails ; **SELECT** recherche/filtres ; **RUN** jouer |
| Détails | **I** image ; **II** retour ; **SELECT** menu du jeu ; **RUN** jouer ; haut/bas texte ; gauche/droite jeu |
| Menu du jeu | Démarrer, favori, cheats, informations de fichier, retour à EverDrive |
| Cheats | **I** activer/désactiver ou instructions ; **SELECT** options ; **II** annuler ; gauche/droite page ; **RUN** enregistrer et revenir |

### Parcourir, rechercher et filtrer

Dans la bibliothèque, appuyez sur **SELECT** pour ouvrir **FIND AND FILTERS**. Haut/bas sélectionne une option ; gauche/droite la modifie. **I** applique les paramètres ou ouvre le sélecteur de titre, de genre ou de lettre pour les lignes correspondantes. **II** annule. **RUN** efface les choix de filtres ; appliquez-les pour revenir à tous les jeux.

| Option | Choix possibles |
| --- | --- |
| Title prefix | Saisir le début d’un titre avec le clavier à l’écran, jusqu’à 12 caractères ; RUN lance la recherche |
| Library | Tous les jeux, favoris, récemment ouverts |
| Category | Sorties originales, traductions, Homebrew, Museum, Tools / Tests, jeux non licenciés ou toutes les catégories |
| Media | HuCard, CD-ROM ou les deux |
| Genre | Sélection dans une liste paginée, par exemple Action, Adventure, Platform, Puzzle, RPG ou Shoot'em Up |
| Letter | A–Z, titres commençant par un chiffre ou un symbole via `#`, ou toutes les lettres |
| Region/lang | Japon, États-Unis, patch anglais, Other ou toutes les régions/langues |
| Cheats | Tous les jeux ou ceux comportant des cheats sélectionnables |
| View | Liste avec images ou grille de six jaquettes |
| Theme | Neon, Ember, Ice, Classic |

Les filtres se combinent. Choisissez par exemple **Original releases + HuCard + Action**. La bibliothèque ordinaire est classée de A à Z ; Recently opened suit l’ordre de l’historique. Cette version ne propose pas de tri par année ou éditeur. Les filtres ne modifient que la liste affichée ; ils ne déplacent aucun fichier. Dans une bibliothèque filtrée, **II** efface les filtres.

Le titre sélectionné apparaît au-dessus de la liste ou de la grille, avec retour à la ligne pour les noms longs. Les détails indiquent le support, la région, le genre, l’année si elle est connue, ainsi que la description et les informations disponibles sur le développeur et l’éditeur. **I** bascule entre jaquette et capture du jeu lorsqu’elles sont disponibles ; haut/bas fait défiler le texte et gauche/droite change de jeu.

Ajoutez ou retirez un favori via **Details → SELECT → Add/Remove favorite**. Jusqu’à 64 favoris et huit jeux récemment ouverts sont conservés. L’historique enregistre les fiches de jeux ouvertes, pas une durée de jeu mesurée. Au redémarrage, TeoPCE sélectionne l’entrée disponible la plus récente de l’historique dans la bibliothèque non filtrée. Les préférences de vue et de thème sont également enregistrées.

De courts indicateurs décrivent le filtre actuel : `FAV` favoris, `OPEN` récents, `B*` titres commençant par B, `R:JP` Japon, `R:US` États-Unis, `L:EN` patch anglais et `CH` cheats sélectionnables. La zone du titre indique aussi le support et la région du jeu sélectionné ; `*` marque un favori.

**Details → SELECT** ouvre également les actions du jeu : démarrer, favoris, cheats, informations de fichier et retour à EverDrive. Les informations de fichier permettent d’identifier l’entrée ou le chemin sélectionné du catalogue lors d’un signalement.

## Cheats

Le gestionnaire associe automatiquement les paquets. Son onglet Cheats présente les groupes ; l’activation se fait **sur la console** :

1. Jeu → **I** détails → **SELECT** menu du jeu → **CHEATS**.
2. Sélectionnez un groupe, appuyez sur **I** et lisez les éventuelles instructions.
3. **RUN** enregistre la sélection et revient en arrière. Lancez le jeu.
4. **SELECT → All cheats off** désactive immédiatement tous les groupes pour ce jeu et enregistre ce choix. Il n’est pas nécessaire d’appuyer à nouveau sur RUN.

Les sélections enregistrées persistent ; le menu indique les codes actifs. **II** annule les modifications de sélection non enregistrées.

### Correspondance, limites et HELD

La bibliothèque incluse contient **1 177 groupes complets pour 347 versions exactes**. La correspondance HuCard utilise l’empreinte du fichier complet. Pour les CD, elle comprend les octets exacts du CUE et toutes les pistes référencées, y compris audio. Les autres révisions, traductions ou CUE recréés peuvent ne pas correspondre. Ne renommez pas un paquet pour forcer sa compatibilité.

**53 associations CD nécessitent aussi le chemin relatif enregistré**, car les règles de démarrage compilées en dépendent. Le gestionnaire affiche ce chemin lorsqu’il diffère. Déplacer un jeu installé peut affecter l’état sauvegardé : utilisez une nouvelle installation avec sauvegarde ou une migration délibérée.

Certaines combinaisons présentent des protections de démarrage CD incompatibles et sont bloquées. La cartouche dispose également d’un budget de 16 emplacements ; un groupe peut en utiliser plusieurs et reste indivisible. Réduisez la sélection si nécessaire.

**HELD signifie problèmes connus ou données non prises en charge**, et non simplement « non testé sur console ». Masquées par défaut, ces entrées peuvent être consultées via **Show held codes**. Les entrées admissibles retenues par une règle proposent **Accept risk** avec une explication ; les codes malformés ou non pris en charge ne peuvent pas être débloqués. Les contrôles de combinaison restent applicables. Masquer HELD ne désactive pas les codes actifs.

Les instructions peuvent exiger une action à l’écran titre, un personnage ou un mode. Un démarrage réussi ne prouve pas l’effet dans tous les niveaux. En cas d’erreur BIOS avec un code, effacez les codes, comparez avec le lancement natif et signalez le groupe et la révision exacts.

**Import extra cheats…** accepte les bibliothèques JSON TeoPCE version 1, pas les fichiers arbitraires d’émulateur. Les imports explicites ont priorité sur les contenus inclus correspondants ; lors des mises à jour, les paquets et sélections déjà installés ont priorité sur les deux. Voir [CHEAT-LIBRARY.md](CHEAT-LIBRARY.md).

### Sauvegardes et IGM

L’IGM HuCard natif ainsi que la sauvegarde et le chargement rapides ont été essayés sur le matériel du propriétaire. L’IGM revient au **menu de Krikzz** ; relancez TeoPCE depuis celui-ci.

Dans la configuration CD testée, il n’y a **ni IGM natif ni état de sauvegarde rapide**. La prise en charge normale des sauvegardes par le jeu ou le BIOS est distincte. TeoPCE n’ajoute pas de superposition en jeu pour activer ou désactiver les codes CD.

## Mises à jour, stockage et restauration

Après ajout de jeux : **Prepare / Refresh collection → vérifier → Install on card**. Les profils et sélections existants sont conservés lorsque leur identité reste valide. Relancez la préparation après avoir joué si l’état de l’application a changé.

Supprimer, renommer ou remplacer des jeux installés ne constitue pas une migration transparente. Si la bibliothèque précédente ne correspond plus, conservez la sauvegarde et résolvez la migration ; ne supprimez pas les profils pour contourner la vérification. Une nouvelle carte peut être préparée indépendamment. La mise à jour de TeoPCE ne met pas à jour le firmware EverDrive.

| Sous le dossier personnel de l’ordinateur | Utilité |
| --- | --- |
| `TeoPCE/prepared/` | Générations préparées et `REPORT.json` |
| `TeoPCE/metadata/` | Modifications de l’éditeur, associées au chemin de montage de la carte |
| `TeoPCE/backups/installation-…/` | ROM/données précédentes ; `RESULT.json` après installation réussie |
| `.cache/teopce-library/` | Cache de téléchargement |

Conservez les modifications et sauvegardes dont vous avez besoin. Un nouveau chemin de montage ou une nouvelle lettre de lecteur Windows peut affecter la recherche des modifications. Les anciennes préparations et le cache peuvent être supprimés lorsque le gestionnaire est fermé ; supprimer le cache nécessite de nouveaux téléchargements.

Pour restaurer : fermez le gestionnaire, copiez ailleurs les éléments actuels `TeoPCE.pce` et `00TEOPCE`, puis **remplacez les deux** par la paire issue d’une même sauvegarde. Ne fusionnez pas les générations. Ne touchez pas aux jeux ni à `edturbo`. Une sauvegarde de première installation peut ne contenir aucun des deux éléments de l’application et ne peut pas restaurer une version précédente qui n’a jamais existé. Éjectez la carte en toute sécurité après la copie.

## Dépannage

| Symptôme | Étape suivante |
| --- | --- |
| Carte introuvable | Choisir sa racine manuellement ; vérifier qu’elle est montée et contient `edturbo` |
| Jeux ignorés | Lire le rapport ; vérifier format, références CUE, chemins ASCII, dossiers exclus et MODE1/2048 |
| Échec aussi dans le menu natif | Examiner image/BIOS/carte ; TeoPCE ne répare pas les médias |
| Seul le lancement par TeoPCE échoue | Noter titre/chemin, console/firmware, erreur exacte et état avec tous les cheats désactivés |
| Aucun code compatible | Le contenu exact ou le chemin requis ne correspond pas ; le lancement normal reste possible |
| Installation désactivée | Enregistrer les modifications et actualiser ; la préparation doit être terminée pour la carte choisie |
| Carte/application modifiée | Refaire la préparation ; ne pas installer une ancienne génération sur un état plus récent |
| Année/image manquante | Essayer la préparation en ligne ou fournir vos propres données ; une année inconnue peut rester vide |
| Installation interrompue | Conserver sauvegardes et fichiers temporaires de préparation, vérifier la carte ; ne jamais supprimer un verrou pendant qu’un installateur fonctionne |

Utilisez le [formulaire de signalement](https://github.com/marvelfamily/TeoPCE/issues/new/choose) en indiquant les versions, les étapes, le texte de l’erreur et la comparaison avec le lancement natif. Masquez les chemins absolus personnels. Ne joignez ni jeux, ni BIOS, ni sauvegardes, ni images complètes de cartes.

Les vérifications manuelles, de plateforme et matérielles encore en attente pour RC6 figurent dans [RELEASE-STATUS.md](../RELEASE-STATUS.md). Les attributions et conditions de redistribution figurent dans [THIRD-PARTY.md](../THIRD-PARTY.md) ; les cartes préparées conservent les attributions des métadonnées et cheats sous `00TEOPCE/data`.
