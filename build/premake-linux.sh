
# sudo apt install clang-15
# sudo apt install libc++-dev g++-12
# sudo apt install libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev libgl-dev xorg-dev
# sudo apt install libopus-dev libasound2-dev libjack-jackd2-dev jackd2
# sudo apt install libssl-dev libsodium-dev libflac-dev libflac++-dev

./premake-bin/linux/premake5 --file=premake.lua gmake2


export CC=/usr/bin/clang-15
export CPP=/usr/bin/clang-cpp-15
export CXX=/usr/bin/clang++-15
export LD=/usr/bin/ld.lld-15
export CXXFLAGS="-stdlib=libc++ -I/usr/include/c++/12 -I/usr/include/x86_64-linux-gnu/c++/12"
export LDFLAGS=-v 


cd _gen
make all config=release_x86_64
