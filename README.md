# REST_APIs

#Dependencies
cmake < 3.15
cpp >= c++17

Compile and Run
g++ calculator.cpp -o calculator
./calculator

Get Result Using Curl
curl -d '{"a":1, "b":2, "op":"+"}' http://127.0.0.1:18080/calculator

