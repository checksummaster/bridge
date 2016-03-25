g++ -std=c++11 bridge.cpp json11.cpp -I . -I ./asio/include -o bridge.linux -static -pthread
readelf bridge.linux -all | grep NEEDED