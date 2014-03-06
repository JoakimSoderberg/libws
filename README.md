libws (alpha)
=============

[![Build Status](https://travis-ci.org/JoakimSoderberg/libws.png?branch=master)](https://travis-ci.org/JoakimSoderberg/libws) [![Coverage Status](https://coveralls.io/repos/JoakimSoderberg/libws/badge.png)](https://coveralls.io/r/JoakimSoderberg/libws)

libws is a multi-platform, non-blocking C websocket client library based on [Libevent][libevent].

Project aim
-----------
The aim of this project is to create a non-blocking portable websocket client library in C. Currently there are no plans to implement the server part of the protocol. There are lots

Some design goals:

- Full [RFC6455][rfc6455 compatability.
- CMake build system with system introspection.
- Intuitive and stable API.
- Message based, frame based and streaming API.
- Full unit test suite.
- Both WS and WSS support.
- Should pass the entire [Autobahn Test Suite][autobahn].
- Support for Windows, Linux and OSX, as well as other Unix OS versions.

Building
========

To build the project using CMake.


Test suite
==========



Regression tests
----------------

libws has both a set of regression tests that can be run using CTest. If valgrind is detected, these tests are by default run using valgrind to detect any memory leaks (this can be turned off by setting `cmake -DLIBWS_WITH_MEMCHECK=OFF ..`).

*Unix*

```bash
$ mkdir build && cd build
$ cmake ..
$ ctest # Run all tests, use --help for extra settings
$ bin/libws_tests # Get a menu and run a specific test manually.
```

*Windows*

From the git bash console:

```bash
$ git submodule update --init deps/libevent # Make sure we have Libevent.
$ ./build_deps.sh
$ mkdir build && cd build
$ cmake ..
$ 
```

Autobahn Test Suite
-------------------

[The Autobahn Test Suite][autobahn] is a set of tests that verifies that a websocket endpoint is behaving according to the websocket standard. Libws implements a client that can run towards the Autobahn fuzzingserver.

Currently libws passes all tests (excluding the extension tests, which are optional).

To build and run these tests:

```bash
$ mkdir build && cd build
$ cmak
```


[libevent]: http://libevent.org/
[autobahn]: http://autobahn.ws/testsuite/
[rfc6455]: https://tools.ietf.org/html/rfc6455

