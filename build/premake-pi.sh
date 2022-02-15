
# sudo apt install clang-12 libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev libgl-dev xorg-dev


./premake-bin/pi/premake5 --file=premake.lua --cc=clang gmake2

export CC=/usr/bin/clang-12
export CPP=/usr/bin/clang-cpp-12
export CXX=/usr/bin/clang++-12
export LD=/usr/bin/ld.lld-12
cd _sln
make clean all
