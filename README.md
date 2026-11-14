# Wayland Autoclicker

Utilitaire anti-AFK et autoclicker natif pour Wayland sous KDE Plasma 6, concu pour fonctionner sans privilèges root, sans acces a `/dev/uinput` et sans regles `udev`.

L'application s'appuie exclusivement sur les interfaces D-Bus standardisees des portails XDG Desktop (`RemoteDesktop` et `GlobalShortcuts`), garantissant une integration native et securisee avec le compositeur Wayland.

---

## Fonctionnalites

- **Deux modes de fonctionnement** :
  - **Clic** : Pression et relachement du bouton selectionne (Gauche, Droit, Milieu) aux coordonnees actuelles du pointeur.
  - **Micro-mouvement** : Deplacement relatif alterne de +/- 1 pixel avec retour a zero net, simulant une activite sans declencher d'action dans les interfaces ou jeux.
- **Cadence et jitter configurables** :
  - Intervalle reglable en heures, minutes, secondes et millisecondes (de 200 ms a 10 minutes).
  - Jitter aleatoire dynamique (jusqu'a 50 % de l'intervalle) pour eviter les cadences strictement periodiques.
- **Securite et controles anti-emballement** :
  - Compte a rebours avant demarrage (0 a 30 secondes) pour positionner le curseur sereinement.
  - Option de minimisation automatique dans la boite a miniatures lors du demarrage.
  - Arret automatique programmable (de 1 seconde a 24 heures, ou desactive).
  - Failsafe RAII : liberation immediate et garantie de tout bouton enfonce en cas d'arret ou d'interruption.
  - Gestionnaire de signaux POSIX async-signal-safe (`SIGINT`, `SIGTERM`, `SIGHUP`) via `socketpair` pour un arret propre.
- **Raccourcis globaux natifs Wayland** :
  - Deux raccourcis XDG Desktop Portals : bascule Demarrer/Arreter (Toggle) et arret d'urgence (Stop).
  - Saisie et reassignation directes depuis l'interface ou via la configuration des raccourcis de KDE Plasma.
  - Synchronisation bidirectionnelle avec le portail et filtrage des touches de verrouillage.
- **Fenetre de diagnostic et debogage** :
  - Mesure des ressources du processus : memoire physique reelle (RSS), pic memoire (HWM), memoire virtuelle (VSIZE), nombre de threads actifs.
  - Etat detaille des sessions D-Bus des portails et jetons de restauration.
  - Statistiques d'injection en temps reel (actions envoyees, boutons relaches, deplacement net).
- **Integration systeme** :
  - Boite a miniatures (*system tray*) avec icone d'etat dynamique et menu contextuel.
  - Persistance securisee des reglages dans `~/.config/Perso/WaylandAutoclicker.conf` avec permissions strictes (0600 sur le fichier, 0700 sur le repertoire).

---

## Architecture technique

Le projet est developpe en C++20 et Qt6, decoupe en modules a responsabilites uniques :

- `PortalManager` ([src/PortalManager.hpp](file:///home/louis/Documents/Projets_Github/Perso/Autoclicker/src/PortalManager.hpp)) : Gere le cycle de vie asynchrone des sessions XDG Desktop Portals (`RemoteDesktop` et `GlobalShortcuts`), l'ecoute des signaux D-Bus, la generation securisee des jetons et implemente l'injecteur concret.
- `InputInjector` ([src/InputInjector.hpp](file:///home/louis/Documents/Projets_Github/Perso/Autoclicker/src/InputInjector.hpp)) : Interface abstraite thread-safe permettant d'isoler le controleur de clic de l'implementation D-Bus.
- `ClickController` ([src/ClickController.hpp](file:///home/louis/Documents/Projets_Github/Perso/Autoclicker/src/ClickController.hpp)) : Orchestre le worker d'arriere-plan base sur `std::jthread` et `std::stop_token`. Garanti sans `sleep` bloquant grace a `std::condition_variable_any`.
- `TimingPolicy` ([src/TimingPolicy.hpp](file:///home/louis/Documents/Projets_Github/Perso/Autoclicker/src/TimingPolicy.hpp)) : Module pur (sans dependance Qt ou D-Bus) charge du calcul des echeances absolues, du jitter et du clamping defensif.
- `DebugDialog` ([src/DebugDialog.hpp](file:///home/louis/Documents/Projets_Github/Perso/Autoclicker/src/DebugDialog.hpp)) : Boite de dialogue de diagnostic analysant l'etat interne et les metriques de `/proc/self/status`.
- `Settings` ([src/Settings.hpp](file:///home/louis/Documents/Projets_Github/Perso/Autoclicker/src/Settings.hpp)) : Enveloppe `QSettings` garantissant le durcissement des droits d'acces du fichier de configuration.
- `MainWindow` / `TrayController` : Interface graphique compacte basee sur Qt Widgets et integration dans la barre des taches de Plasma.

---

## Prerequis

- **Systeme d'exploitation** : Linux avec session Wayland (KDE Plasma 6 recommande).
- **Portails de bureau** : `xdg-desktop-portal` et `xdg-desktop-portal-kde`.
- **Compilateur** : GCC >= 10 ou Clang supportant C++20 (`std::jthread`, `std::stop_token`).
- **Outils de compilation** : CMake >= 3.21 et Ninja.
- **Bibliotheques** :
  - Qt6 (`Core`, `Widgets`, `DBus`, `Test`)
  - En-tetes systeme Linux (`linux/input-event-codes.h`)

Sous Arch Linux / Manjaro :
```bash
sudo pacman -S base-devel cmake ninja qt6-base xdg-desktop-portal xdg-desktop-portal-kde
```

Sous Fedora :
```bash
sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel xdg-desktop-portal xdg-desktop-portal-kde
```

Sous Ubuntu / Debian (24.04+) :
```bash
sudo apt install build-essential cmake ninja-build qt6-base-dev libxkbcommon-dev xdg-desktop-portal xdg-desktop-portal-kde
```

---

## Compilation et Installation

### 1. Configuration et compilation

```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

### 2. Execution de la suite de tests

```bash
ctest --test-dir build --output-on-failure
```

### 3. Installation locale (recommandee dans ~/.local)

```bash
cmake --install build --prefix ~/.local
```

Apres installation, l'application est disponible dans votre lanceur d'applications ou via la commande :
```bash
~/.local/bin/wayland-autoclicker
```

---

## Utilisation

1. **Premier lancement** :
   - Au demarrage, KDE Plasma affiche une boite de dialogue sollicitant l'acces au controle du pointeur (`RemoteDesktop`).
   - Cochez l'option pour memoriser l'autorisation. Un jeton persistant securise sera enregistre pour les sessions futures.
2. **Configuration des raccourcis** :
   - Par defaut, les raccourcis suggeres sont `F6` (Bascule Demarrer / Arreter) et `F7` (Arret d'urgence).
   - Vous pouvez modifier ces raccourcis directement dans l'interface (champs Toggle et Stop) ou en cliquant sur **KDE Config** pour ouvrir le panneau natif de Plasma 6.
3. **Lancement du clic** :
   - Configurez l'intervalle souhaite, le jitter et le compte a rebours.
   - Cliquez sur **Start** ou appuyez sur votre raccourci global.
   - En cas d'imprevu, appuyez a tout moment sur le raccourci d'arret pour interrompre l'action instantanement.
4. **Diagnostic** :
   - Cliquez sur le bouton **Debug** pour inspecter l'empreinte memoire reelle, les identifiants de session D-Bus et les compteurs d'injection.

---

## Revocation des autorisations

Si vous souhaitez reinitialiser ou revoquer les autorisations accordees au portail :
1. Ouvrez la **Configuration du systeme** de KDE Plasma.
2. Naviguez vers **Securite et confidentialite** puis **Raccourcis** / **Autorisations des applications**.
3. Recherchez `WaylandAutoclicker` ou `fr.perso.Autoclicker` et revoquez l'acces au portail RemoteDesktop.
4. Supprimez eventuellement le fichier de configuration local :
   ```bash
   rm -f ~/.config/Perso/WaylandAutoclicker.conf
   ```

---

## Contribution

Les contributions sont les bienvenues. Merci de consulter le fichier [CONTRIBUTING.md](file:///home/louis/Documents/Projets_Github/Perso/Autoclicker/CONTRIBUTING.md) pour prendre connaissance des standards de developpement, de la politique de style et du flux de validation Git.
