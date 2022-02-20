#!/bin/bash
DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

 WIP // THIS IS JUST NOTES // DONT ACTUALLY RUN THIS

# rosetta required to run x86_64 stuff non M1
softwareupdate --install-rosetta

# install homebrew in x86_64 mode
arch --x86_64 /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install.sh)"

# install all the x86_64 libraries
arch --x86_64 /usr/local/Homebrew/bin/brew install flac
arch --x86_64 /usr/local/Homebrew/bin/brew install libsodium
arch --x86_64 /usr/local/Homebrew/bin/brew install openssl@1.1
arch --x86_64 /usr/local/Homebrew/bin/brew install freetype
arch --x86_64 /usr/local/Homebrew/bin/brew install opus
arch --x86_64 /usr/local/Homebrew/bin/brew install bzip2


# fuse libs into fat lumps

lipo /usr/local/lib/libFLAC.a /opt/homebrew/lib/libFLAC.a-create -output ./universal-fat/libFLAC.a
lipo /usr/local/lib/libFLAC++.a /opt/homebrew/lib/libFLAC++.a -create -output ./universal-fat/libFLAC++.a
lipo /usr/local/lib/libogg.a /opt/homebrew/lib/libogg.a -create -output ./universal-fat/libogg.a
lipo /usr/local/lib/libopus.a /opt/homebrew/lib/libopus.a -create -output ./universal-fat/libopus.a

lipo /usr/local/lib/libfreetype.a /opt/homebrew/lib/libfreetype.a -create -output ./universal-fat/libfreetype.a
lipo /usr/local/lib/libsodium.a /opt/homebrew/lib/libsodium.a -create -output ./universal-fat/libsodium.a

lipo /usr/local/Cellar/openssl@1.1/1.1.1m/lib/libcrypto.a /opt/homebrew/Cellar/openssl@1.1/1.1.1l_1/lib/libcrypto.a -create -output ./universal-fat/libcrypto.a
lipo /usr/local/Cellar/openssl@1.1/1.1.1m/lib/libssl.a /opt/homebrew/Cellar/openssl@1.1/1.1.1l_1/lib/libssl.a -create -output ./universal-fat/libssl.a
lipo /usr/local/lib/libpng.a /opt/homebrew/lib/libpng.a -create -output ./universal-fat/libpng.a
lipo /usr/local/Cellar/bzip2/1.0.8/lib/libbz2.a /opt/homebrew/Cellar/bzip2/1.0.8/lib/libbz2.a -create -output ./universal-fat/libbz2.a