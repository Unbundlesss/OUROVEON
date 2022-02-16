
# sudo apt install clang-12
# sudo apt install libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev libgl-dev xorg-dev
# sudo apt install libssl-dev libasound2-dev libjack-jackd2-dev jackd2

./premake-bin/linux-armhf/premake5 --file=premake.lua --cc=clang gmake2

export CC=/usr/bin/clang-12
export CPP=/usr/bin/clang-cpp-12
export CXX=/usr/bin/clang++-12
export LD=/usr/bin/ld.lld-12

cd _gen
make clean all
