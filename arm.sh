~/Mazda/mazda_adb/m3-toolchain-master/bin/arm-cortexa9_neon-linux-gnueabi-g++ -std=c++11 bridge.cpp json11.cpp -I . -I ./asio/include -o bridge.arm -static -pthread
readelf bridge.arm -all | grep NEEDED
