# Spécification et Conception Technique : Autoclicker / Anti-AFK Wayland (KDE Plasma 6) v2

## 1. Contexte et Objectifs

L'objectif est de développer un utilitaire d'autoclick et d'anti-AFK natif pour Wayland sous KDE Plasma 6 (Linux).
L'application ne requiert aucun privilège root, aucun accès direct à `/dev/uinput` et aucune règle `udev`. Toute l'injection d'événements pointeur ainsi que la capture de raccourcis clavier globaux s'effectuent via les portails XDG (`org.freedesktop.portal.RemoteDesktop` et `org.freedesktop.portal.GlobalShortcuts`) au travers de D-Bus.

L'interface graphique s'inspire directement de l'ergonomie compacte d'OP Auto Clicker tout en intégrant rigoureusement les impératifs de sécurité et les retours d'état propres à Wayland.

---

## 2. Stack Technique et Dépendances

- **Langage** : C++20 (`std::jthread`, `std::stop_token`, `std::condition_variable_any`, `std::atomic`, `std::optional`). Compilateur de référence : GCC 10+ ou Clang avec libstdc++.
- **Système de build** : CMake (>= 3.21) avec Ninja et `AUTOMOC` activé.
- **Framework GUI & IPC** : Qt6 (`Core`, `Widgets`, `DBus`).
- **Headers système** : `<linux/input-event-codes.h>` (`BTN_LEFT`, `BTN_RIGHT`, `BTN_MIDDLE`), `<sys/socket.h>`, `<unistd.h>`, `<csignal>`.
- **Framework de test** : Qt Test (`Qt6::Test`), intégré à CTest.

---

## 3. Architecture et Découpage des Composants

```
+-------------------------------------------------------------+
|                         MainWindow                          |
|  - Layout compact (style OP Auto Clicker)                   |
|  - Saisie Intervalle (H, M, S, Ms)                          |
|  - Options (Bouton, Mode, Jitter)                           |
|  - Options avancées (Arrêt auto, Compte à rebours, Tray)    |
|  - Statuts Wayland (Session, Raccourcis, Run)               |
+------------------------------+------------------------------+
                               | Qt Signals / Slots
                               v
+-------------------------------------------------------------+
|                       ClickController                       |
|  - Worker std::jthread                                      |
|  - Atomics (intervalle, jitter, bouton, mode, arrêt auto)   |
|  - std::condition_variable_any + std::stop_token            |
|  - Failsafe RAII (mémorisation du bouton enfoncé)           |
|  - Compteur d'échecs consécutifs                            |
+------------------------------+------------------------------+
                               | Appelle
                               v
+-------------------------------------------------------------+
|                 <<interface>> InputInjector                 |
|  + virtual bool sendButton(uint32_t button, bool pressed)   |
|  + virtual bool sendMotion(double dx, double dy)            |
+------------------------------+------------------------------+
                               ^
                               | Implémente
+------------------------------+------------------------------+
|                        PortalManager                        |
|  - Thread principal Qt                                      |
|  - Session RemoteDesktop (Types=2u, PersistMode=2u)         |
|  - Session GlobalShortcuts (Toggle & Stop-Only)             |
|  - Machine à états Request/Response & handle_token          |
|  - Gestion du restore_token à usage unique                  |
|  - Envoi D-Bus thread-safe via QDBusConnection::sessionBus  |
+-------------------------------------------------------------+
```

### 3.1 `TimingPolicy` (Module pur C++20)
- Aucun en-tête Qt ni D-Bus.
- Calcul des bornes et clamp défensif par cycle :
  - Intervalle : [200 ms, 600 000 ms], défaut 1000 ms.
  - Jitter : [0 ms, 50% de l'intervalle effectif], défaut 80 ms.
  - Durée de pression : tirée aléatoirement dans `[borne_basse, borne_haute]`, avec `borne_haute = min(35 ms, 35% de l'intervalle)` et `borne_basse = min(20 ms, borne_haute)`.
  - Calcul de l'échéance sur `std::chrono::steady_clock` avec jitter non-cumulatif.
  - Vérification de l'arrêt automatique : comparaison entre le temps total écoulé depuis le début du run et la limite configurée (0 = illimité). La mise à jour à la volée ne réinitialise pas le temps écoulé.

### 3.2 `InputInjector` (Interface abstraite)
```cpp
class InputInjector {
public:
    virtual ~InputInjector() = default;
    virtual bool sendButton(uint32_t button, bool pressed) = 0;
    virtual bool sendMotion(double dx, double dy) = 0;
};
```

### 3.3 `ClickController` (Orchestration & Worker)
- Ne dépend d'aucun en-tête D-Bus.
- Possède un `std::jthread` qui exécute la boucle de clics/mouvements.
- Synchronisation non bloquante : `std::condition_variable_any` conditionnée par `stop_token`. Tout appel à `request_stop()` réveille instantanément le worker, même au milieu d'un cycle ou d'une temporisation.
- **Garde RAII** (`QScopeGuard` ou classe dédiée) :
  - En mode Clic : mémorise explicitement le bouton envoyé lors du `Press`. Au `Release` (ou en sortie de scope / interruption), le relâchement de ce bouton précis est systématiquement émis.
  - En mode Micro-mouvement : si le worker s'interrompt après le premier pas de +1 pixel, un mouvement compensatoire de -1 pixel est émis pour garantir un déplacement net rigoureusement nul.
- Compteur d'échecs consécutifs d'injection (seuil = 3) provoquant l'arrêt automatique sécurisé et la notification de l'interface.

### 3.4 `PortalManager` (Gestion des Portails XDG)
- Vit sur le thread principal.
- Implémente l'interface `InputInjector`.
- Établit la session `org.freedesktop.portal.RemoteDesktop` :
  - `CreateSession(session_handle_token)` -> écoute `Response` sur le chemin calculé `/org/freedesktop/portal/desktop/request/...`.
  - `SelectDevices(types = 2u)` (souris/pointeur uniquement).
  - `Start(restore_token)` -> validation que le pointeur est bien accordé, extraction du nouveau `restore_token` et réécriture immédiate dans `QSettings`.
- Établit la session `org.freedesktop.portal.GlobalShortcuts` :
  - Deux raccourcis : Raccourci bascule (Start/Stop) et Raccourci d'arrêt seul (idempotent).
  - Écoute du signal `Activated(action_id)`.
- Écoute du signal `Session.Closed` : arrêt immédiat du run et passage de l'UI en état "session fermée".

---

## 4. Interface Utilisateur Qt6 & Tray

### 4.1 Disposition inspirée d'OP Auto Clicker
1. **Click Interval** : 4 `QSpinBox` alignés horizontalement : `Hours`, `Minutes`, `Seconds`, `Milliseconds`. L'intervalle total calculé est borné dynamiquement entre 200 ms et 600 000 ms.
2. **Options** :
   - `Mouse Button` : `QComboBox` (`Left` / `Right` / `Middle`).
   - `Mode` : `QComboBox` (`Click` / `Micro-movement`).
   - `Jitter (+/-)` : `QSpinBox` (ms), borné à max 50% de l'intervalle.
3. **More Options / Safety** :
   - `Auto-stop` : `QSpinBox` (durée en secondes, 0 = illimité).
   - `Countdown` : `QSpinBox` (0 à 30 secondes, défaut 3 s).
   - `Minimize to tray on start` : `QCheckBox`.
4. **Commandes d'Action** :
   - Boutons larges `Start` et `Stop` (avec indication visuelle des raccourcis actifs).
5. **Bandeau de Diagnostic et Statuts (Bas de fenêtre)** :
   - Statut RemoteDesktop : Pastille et texte (`Inactive`, `Pending`, `Active`, `Refused`, `Closed`, `Error`).
   - Statut Raccourcis : Raccourcis effectifs ou mention "Inhibé / Indisponible".
   - Statut du Run : État courant, compte à rebours restant, temps restant avant arrêt automatique, dernier motif d'arrêt.

### 4.2 `TrayController`
- Icône présente dans la zone de notification KDE avec deux états visuels (inactif / actif).
- Menu contextuel : "Ouvrir", "Arrêter", "Quitter".

---

## 5. Règles de Fiabilité, Signaux POSIX et Arrêt Ordonné

### 5.1 Gestion des Signaux POSIX (Async-Signal-Safe)
- Un gestionnaire `sigaction` intercepte `SIGINT`, `SIGTERM` et `SIGHUP`.
- Le gestionnaire sauvegarde et restaure `errno`.
- Aucune fonction Qt ni D-Bus n'est appelée dans le gestionnaire : il se contente d'écrire un octet dans un `socketpair(AF_UNIX, SOCK_STREAM, SOCK_CLOEXEC | SOCK_NONBLOCK)`.
- Côté Qt, un `QSocketNotifier` lit cet octet sur le thread principal et déclenche l'arrêt propre de l'application.

### 5.2 Séquence d'Arrêt Ordonné
1. Demande d'arrêt du worker via `request_stop()`.
2. Jonction du thread (`jthread::join()`), assurant l'exécution du failsafe RAII.
3. Vidage des messages D-Bus en attente (`QDBusConnection::sessionBus().send(...)`).
4. Fermeture explicite des sessions RemoteDesktop et GlobalShortcuts (`Session.Close`).
5. Sauvegarde des préférences utilisateur via `QSettings`.
6. Terminaison propre de `QApplication`.

---

## 6. Structure du Répertoire du Projet

```
.
|-- CMakeLists.txt
|-- data/
|   |-- fr.perso.Autoclicker.desktop
|   |-- icons/
|       |-- 48x48/fr.perso.Autoclicker.png
|-- src/
|   |-- main.cpp
|   |-- InputInjector.hpp
|   |-- TimingPolicy.hpp
|   |-- TimingPolicy.cpp
|   |-- ClickController.hpp
|   |-- ClickController.cpp
|   |-- PortalManager.hpp
|   |-- PortalManager.cpp
|   |-- Settings.hpp
|   |-- Settings.cpp
|   |-- TrayController.hpp
|   |-- TrayController.cpp
|   |-- MainWindow.hpp
|   |-- MainWindow.cpp
|-- tests/
|   |-- CMakeLists.txt
|   |-- tst_TimingPolicy.cpp
|   |-- tst_ClickController.cpp
|   |-- tst_DBusSignatures.cpp
|   |-- FakeInputInjector.hpp
```

---

## 7. Stratégie de Tests Automatisés

- **`tst_TimingPolicy`** :
  - Respect strict des bornes d'intervalle et de jitter.
  - Plage de durée de pression (20 à 35 ms) pour différents intervalles (200 ms, 1000 ms, etc.).
  - Comportement d'arrêt automatique en cours d'exécution.
- **`tst_ClickController`** (avec `FakeInputInjector`) :
  - Émission ordonnée de Press puis Release.
  - Changement de bouton en cours de pression : vérification que le Release relâche bien le bouton mémorisé.
  - Arrêt anticipé en plein milieu d'une attente : réactivité instantanée (< 50 ms).
  - Mode micro-mouvement : déplacement compensatoire de retour si arrêt entre les deux mouvements (somme nette = 0).
  - Compteur d'échecs d'injection : arrêt automatique au 3e échec.
- **`tst_DBusSignatures`** :
  - Validation des signatures de méthodes et types QtDBus pour les appels aux méthodes `NotifyPointerButton` (chemin d'objet, options `a{sv}`, bouton `i`, état `u`) et `NotifyPointerMotionRelative` (chemin d'objet, options `a{sv}`, dx `d`, dy `d`).
