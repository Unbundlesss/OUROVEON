# LORE : Starting Up

## System Requirements

* Screen resolution of at least 1920 x 1080
* OpenGL 3.2
* MacOS 11+ or Windows 8.1+

<br>

## Installation

None of the **OUROVEON** apps require any specific installation. Simply unzip the build somewhere you want to keep it (or on Mac, feel free to drag it into **Applications**)

To upgrade to a new version, simply delete the previous one and perform the above step again.

<br>

### Uninstallation / Reset

To uninstall, simply delete the app (or wherever you unpacked it to).

All retained configuration files are stored in the following places:

* Windows : `%APPDATA%\OUROVEON`
* MacOS : `~/Library/Application Support/OUROVEON/`

If you wish to completely reset the app or remove any remaining data, simply delete that `OUROVEON` folder.

<br>

## Initial Setup

The *Framework Preflight* screen will be shown each time you start the app to allow you to adjust any core configuration settings before running the actual application functionality. First time you run an app on a fresh install, there are a few crucial configuration steps to take:

#### Data Storage 

The first time you run **LORE** you will be asked to choose a folder to use as your *Data Storage* path. This is where **OUROVEON** apps stash and share data. All downloaded and cached stems will be stored under here, as well as recordings and exports.

![initial setup screen](/doc/lore-081/initial-setup.png)

If the directory specified does not exist, click $\textcolor{orange}{\textsf{[Create Directory]}}$ to make it. Once it exists, you can click $\textcolor{orange}{\textsf{[Validate and Save]}}$ to accept those settings and progress.

#### Audio Engine & Authentication

The final two steps involve choosing an audio device to use for playback and then logging into Endlesss. Note that you can use some of the features of **LORE** without logging into Endlesss by toggling $\textcolor{orange}{\textsf{Continue without Authentication}}$ but this will limit your ability to sync jams.

![initial login](/doc/lore-081/initial-setup-login.png)

When you have logged in, you will have the opportunity to synchronise the local Jam Cache. First time you run, the tool will require you to do this at least once. Click the 🔄 button to begin this process.

![jam cache sync](/doc/lore-081/initial-setup-sync.png)

**This may take a few minutes.** It fetches the list of all jams you have subscribed to in Endlesss, the latest batch of public jams and optionally the current 'Collectible' jam list. For each jam it finds, it then also downloads a pile of data about the jam state to better populate the **LORE** *Jam Browser* UI with up-to-date statistics.

$\textcolor{cyan}{\textsf{IMPORTANT}}$ : because this process takes time and network bandwidth, it is not done automatically each time the application runs. You should re-sync your cache by hitting 🔄 every so often so that the data stays roughly up to date. If you have recently joined new jams and can't find them in the *Jam Browser*, ensure you've recently updated this cache.

Once everything is setup, click $\textcolor{orange}{\textsf{[Continue]}}$ to begin using **LORE**!
