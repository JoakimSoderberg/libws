#!/bin/sh

cd deps
cd libevent
mkdir build
cd build
cmake -DEVENT__DISABLE_TESTS=ON -DEVENT__DISABLE_REGRESS=ON -DEVENT__DISABLE_BENCHMARK=ON ..
cmake --build . --config Release
cmake --build . --config Debug
cd ..
cd ..
cd ..


