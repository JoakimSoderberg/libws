
md build
cd build
cmake -DLIBWS_WITH_AUTOBAHN=ON ..
cmake --build .
ctest
bin\Debug\autobahntest.exe --config ..\test\autobahn\libws.cfg
