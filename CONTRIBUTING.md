# Guide de Contribution

Merci pour votre interet envers le projet Wayland Autoclicker. Ce document rassemble les regles de developpement, les standards de code et le flux de travail a respecter pour contribuer efficacement et maintenir un niveau de qualite eleve.

---

## 1. Principes Generaux

- **Fiabilite et securite avant tout** : L'application interagit directement avec le serveur graphique Wayland et simule des entrees physiques. Tout ajout doit garantir qu'aucun clic ne peut rester coince et qu'aucun thread ne peut se bloquer indéfiniment.
- **Approche chirurgicale (KISS et YAGNI)** : Ne modifiez que le code strictement necessaire a la fonctionnalite ou au correctif demande. Evitez les abstractions prematurees ou les composants inutilises.
- **Verification par la preuve** : Toute contribution (code ou correction) doit etre accompagnee de tests unitaires ou de preuves tangibles de fonctionnement.

---

## 2. Standards de Developpement

### 2.1 Langage et Bibliothèques
- **Norme** : C++20 moderne (`std::jthread`, `std::stop_token`, `std::condition_variable_any`, lambda-expressions, `std::optional`).
- **Framework** : Qt6 (`QtCore`, `QtWidgets`, `QtDBus`).
- **Absence d'appels bloquants** : Ne jamais utiliser `sleep_for`, `msleep` ou des boucles d'attente actives dans les threads de traitement. Toutes les attentes doivent etre interruptibles via un `stop_token`.
- **RAII et securite memoire** : Gestion automatique des ressources, utilisation de gardes de portee (`QScopeGuard`) pour garantir le relachement des etats en toute circonstance.

### 2.2 Compilation et Avertissements
- Le projet compile avec `-Wall -Wextra -Wformat -Wformat-security -fstack-protector-strong`.
- **Zero avertissement** : Tout avertissement genere par le compilateur lors d'une contribution doit etre corrige avant soumission.

### 2.3 Politique Stricte sur les Emojis
Afin de preserver la clarte et la sobriete professionnelle du depot, il est **formellement interdit d'inclure des emojis** dans :
- Le code source (interfaces utilisateur, chaines de caracteres, commentaires, identifiants).
- Les messages de commit Git.
- Les titres et descriptions de Pull Requests (PR).
- La documentation (fichiers markdown, specifications, guides).

---

## 3. Conventions de Commits

Le projet adopte rigoureusement la convention **Conventional Commits**. Chaque message de commit doit respecter la structure suivante :

```text
<type>(<portee_optionnelle>): <description a l'indicatif sans point final>
```

### Types autorises
- `feat` : Ajout d'une nouvelle fonctionnalite.
- `fix` : Correction d'un bogue.
- `docs` : Modification de la documentation seule.
- `test` : Ajout ou correction de tests unitaires.
- `refactor` : Modification du code ne changeant ni le comportement ni les fonctionnalites.
- `perf` : Amelioration des performances ou de l'empreinte memoire.
- `build` : Modification du systeme de construction CMake ou des dependances.
- `ci` : Configuration de l'integration continue.
- `chore` : Taches d'entretien diverses sans impact sur le binaire produit.

### Bonnes pratiques Git
- Ecrire la description a l'imperatif ou a l'indicatif present en minuscules (ex: `feat: add support for extra mouse buttons`).
- Ne pas commiter apres chaque micro-ajustement (ex: calibration de temporisation). Regrouper les modifications une fois stabilisees.
- Ne jamais inclure de fichiers temporaires, de caches ou de fichiers generes par l'environnement de developpement.

---

## 4. Flux de Travail Git et Protection de Branche

### 4.1 Branche principale `main`
- La branche `main` represente la version de production stable et testee.
- **La branche `main` est strictement protegee** :
  - Aucun push direct n'est autorise sur `main`.
  - Toutes les modifications doivent transiter par une Pull Request (PR).
  - La suite de tests CTest doit passer a 100 % avant fusion.

### 4.2 Cycle de contribution
1. Forkez ou clonez le depot localement :
   ```bash
   git clone https://github.com/Louis27940/wayland-autoclicker.git
   cd wayland-autoclicker
   ```
2. Creez une branche thematique a partir de `main` :
   ```bash
   git checkout -b feat/nom-de-votre-fonctionnalite
   # ou
   git checkout -b fix/nom-du-correctif
   ```
3. Effectuez vos modifications en respectant les standards de code et de commit.
4. Lancez la suite de tests complete et assurez-vous de leur succes (voir section suivante).
5. Poussez votre branche sur votre depot distant et ouvrez une Pull Request vers `main`.

---

## 5. Construction et Validation des Tests

Avant de soumettre une Pull Request, verifiez en local que la compilation est exempte de tout avertissement et que l'ensemble des tests reussit.

### 5.1 Compilation avec options de durcissement
```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

### 5.2 Lancement de la suite CTest
```bash
ctest --test-dir build --output-on-failure
```

Tout comportement nouveau doit etre couvert par un test unitaire adapte dans le repertoire `tests/` (`tst_TimingPolicy`, `tst_ClickController`, `tst_DBusSignatures`, ou `tst_DebugInfo`).

---

## 6. Signalement de Bogues et Suggestions

### 6.1 Rapports de bogues
Pour signaler un dysfonctionnement, ouvrez une issue sur GitHub en fournissant :
- Une description concise et precise du probleme.
- Les etapes permettant de le reproduire.
- Votre environnement technique : version de la distribution Linux, version du compositeur Wayland / KDE Plasma, version de Qt.
- Les eventuelles traces ou messages de la console.

### 6.2 Failles de securite
Si vous identifiez une vulnerabilite ou un defaut de securite (privilege escalation, fuite de jeton, regression de permissions), merci de ne pas ouvrir d'issue publique. Contactez directement le responsable du depot ou utilisez la fonctionnalite de signalement prive de vulnerabilites de GitHub (*Private Vulnerability Reporting*).
