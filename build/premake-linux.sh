
# sudo apt install libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev libgl-dev xorg-dev
# sudo apt install libopus-dev libasound2-dev libjack-jackd2-dev jackd2
# sudo apt install libssl-dev libsodium-dev

./premake-bin/linux/premake5 --file=premake.lua gmake2


# sudo apt install clang-12
#export CC=/usr/bin/clang-12
#export CPP=/usr/bin/clang-cpp-12
#export CXX=/usr/bin/clang++-12
#export LD=/usr/bin/ld.lld-12
#export CXXFLAGS=-stdlib=libc++


cd _gen
make clean all config=release_x86_64
