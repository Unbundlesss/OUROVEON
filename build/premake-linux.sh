
# sudo apt install clang-16
# sudo apt install libc++-dev
# sudo apt install libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev libgl-dev xorg-dev
# sudo apt install libopus-dev libasound2-dev libjack-jackd2-dev jackd2
# sudo apt install libssl-dev libsodium-dev libflac-dev libflac++-dev

./premake-bin/linux/premake5 --file=premake.lua gmake2


export CC=/usr/bin/clang-16
export CPP=/usr/bin/clang-cpp-16
export CXX=/usr/bin/clang++-16

export CXXFLAGS=" -fsanitize=address -fno-omit-frame-pointer "
export LDFLAGS="-v -fsanitize=address -fuse-ld=lld-16"


cd _gen
make LORE config=release_x86_64
