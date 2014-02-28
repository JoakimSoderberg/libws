#!/bin/bash

echo "Running Autobahn test client script $LIBWS_METHOD"

if [ x$LIBWS_METHOD == xautobahn ]; then
  wstest -m fuzzingserver &
  FUZZ_PID=$!
  sleep 5
  mkdir build 
  cd build
  cmake $CMAKE_ARGS ..
  cmake --build .
  bin/autobahntest --config ../test/autobahn/libws.cfg
  kill -9 $FUZZ_PID
fi

