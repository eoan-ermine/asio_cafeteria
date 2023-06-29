# asio_cafeteria

A simple demonstration of multithreaded asynchronous execution using timers and a strand with Boost.Asio

## Build

```shell
mkdir build && cd build
conan install .. -of .
cmake --preset conan-release ..
cmake --build .
```
