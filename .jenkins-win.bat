cd deps
cd libevent
md build
cd build
cmake -DEVENT__DISABLE_TESTS=ON -DEVENT__DISABLE_REGRESS=ON -DEVENT__DISABLE_BENCHMARK=ON .. 
cd ..
cd ..
cd ..

md build
cd build
cmake -DLibevent_DIR=deps/libevent/build/  -DLIBWS_WITH_AUTOBAHN=ON ..
cmake --build .
ctest
bin\Debug\autobahntest.exe --config ..\test\autobahn\libws.cfg
