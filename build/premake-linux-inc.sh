
./premake-bin/linux/premake5 --file=premake.lua  gmake2

#export CC=/usr/bin/clang-12
#export CPP=/usr/bin/clang-cpp-12
#export CXX=/usr/bin/clang++-12
#export LD=/usr/bin/ld.lld-12

#export CXXFLAGS=-stdlib=libc++
#export LDFLAGS=-v 

cd _gen
make config=release_x86_64
