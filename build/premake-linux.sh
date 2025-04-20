
# sudo apt install clang-19 lld-19
# sudo apt install libc++-dev
# sudo apt install libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev libgl-dev xorg-dev
# sudo apt install libopus-dev libasound2-dev libjack-jackd2-dev jackd2
# sudo apt install libssl-dev libsodium-dev libflac-dev libflac++-dev libsndio-dev


# Fedora 39
# dnf install requirements
#
#       libX11-devel libXcursor-devel libXrandr-devel libXinerama-devel libXi-devel mesa-libGL-devel
#       clang lld
#       freetype-devel libXxf86vm-devel
#       pipewire-jack-audio-connection-kit-devel alsa-lib-devel
#       libopenenc-devel openssl-devel libsodim-devel flac-devel
#
# `-fuse-ld=lld` in LDFLAGS
# requires `-L/usr/lib64/pipewire-0.3/jack` added to LDFLAGS to find the jack lib
#

./premake-bin/linux/premake5 --file=premake.lua gmake2


export CC=/usr/bin/clang-19
export CPP=/usr/bin/clang-cpp-19
export CXX=/usr/bin/clang++-19

#export CXXFLAGS=" -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -g "
#export LDFLAGS="-v -fsanitize=address -fsanitize=undefined -fuse-ld=lld-19 "

export CXXFLAGS=" "
export LDFLAGS="-v -fuse-ld=lld-19 "


cd _gen
make LORE -j 6 config=debug_x86_64
