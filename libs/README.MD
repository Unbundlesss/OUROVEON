# Sinful Prebuilt Binary Collection

OUROVEON uses a small mountain of external libraries. Many of these we build from source (some of which contain edits from the original) or are header-only.

This folder contains all the libraries we distribute as pre-built binaries for one reason or another.

A handful are either too complex to bother porting into our build framework and/or we have no designs on ever needing to modify the code of and are therefore either bundled (in the case of Windows where package management is less universal) or expected to be installed via the OS' package manager upfront.

<br>
<hr>
<br>

## Windows

Pre-built support libraries :

* OpenSSL 1.1.1m (dynamic, x64, Visual Studio 2019 16.11) using https://github.com/kiyolee/openssl1_1-win-build
* Freetype 2.11.1 (static, x64, Visual Studio 2019 16.11) built from original source https://www.freetype.org/

Middleware :

* [Superluminal performance](https://superluminal.eu/) analysis API

<br>
<hr>
<br>

## MacOS Universal

In the case of building Universal binaries for M1+Intel, our PM of choice (Homebrew) does not provide 'fat' libraries - that is, combined binaries with object code for both ARM64 and X86_64 in one unit - so there is an additional step.

We build our own fat libraries by using both the M1-native and rosetta-hosted versions of Homebrew ([approach modelled on this](https://stackoverflow.com/questions/64951024/how-can-i-run-two-isolated-installations-of-homebrew)) to fetch the required libraries and then `lipo` merge the two results together into a Universal bundle. A snapshot of these is checked in and a script is in-progress to automate the process for brave souls looking to do from-scratch builds on MacOS themselves

At time of writing, we ship the following custom-fat libraries

* OpenSSL 1.1.1
* FLAC
* OPUS
* Sodium
* Freetype (req. PNG, BZ2)
