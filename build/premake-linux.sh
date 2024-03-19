
# sudo apt install clang-17 lld-17
# sudo apt install libc++-dev
# sudo apt install libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev libgl-dev xorg-dev
# sudo apt install libopus-dev libasound2-dev libjack-jackd2-dev jackd2
# sudo apt install libssl-dev libsodium-dev libflac-dev libflac++-dev

./premake-bin/linux/premake5 --file=premake.lua gmake2


export CC=/usr/bin/clang-17
export CPP=/usr/bin/clang-cpp-17
export CXX=/usr/bin/clang++-17

export CXXFLAGS=" -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -g "
export LDFLAGS="-v -fsanitize=address -fsanitize=undefined -fuse-ld=lld-17 "


cd _gen
make LORE -j 6 config=debug_x86_64
