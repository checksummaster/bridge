#!/usr/bin/env bash

# Fail on error, verbose output
#set -exo pipefail

# Figure out which ABI and SDK the device has
abi=$(adb shell getprop ro.product.cpu.abi | tr -d '\r')
sdk=$(adb shell getprop ro.build.version.sdk | tr -d '\r')
rel=$(adb shell getprop ro.build.version.release | tr -d '\r')

# PIE is only supported since SDK 16
if (($sdk >= 16)); then
  bin=minicap
  bin2=minitouch
else
  bin=minicap-nopie
  bin2=minitouch-nopie
fi

# Create a directory for our resources
dir=/data/local/tmp/minicap-devel
adb shell "mkdir $dir 2>/dev/null"

# Upload the binary
echo --------------------
echo upload binary
echo --------------------
adb push minicap/libs/$abi/$bin $dir
adb push minitouch/libs/$abi/$bin2 /data/local/tmp/


# Upload the shared library
echo --------------------
echo Upload the shared library
echo --------------------
if [ -e minicap/jni/minicap-shared/aosp/libs/android-$rel/$abi/minicap.so ]; then
  adb push minicap/jni/minicap-shared/aosp/libs/android-$rel/$abi/minicap.so $dir
else
  adb push minicap/jni/minicap-shared/aosp/libs/android-$sdk/$abi/minicap.so $dir
fi

# Run!
echo --------------------
echo run
echo --------------------
adb shell /data/local/tmp/$bin2 &
adb shell LD_LIBRARY_PATH=$dir $dir/$bin -P 1440x2560@480x800/90 &

sleep 3

#disable auto rotation
adb shell content insert --uri content://settings/system --bind name:s:accelerometer_rotation --bind value:i:0
#landscape
adb shell content insert --uri content://settings/system --bind name:s:user_rotation --bind value:i:3
#portrait
#adb shell content insert --uri content://settings/system --bind name:s:user_rotation --bind value:i:0

adb forward tcp:1717 localabstract:minicap
adb forward tcp:1111 localabstract:minitouch

adb shell ps | grep mini
adb forward --list

#toggle the screen on/off
#adb shell input keyevent 26 # Power
#adb shell input keyevent 82; # Menu

#adb shell dumpsys power | grep "mScreenOn=true" | xargs -0 test -z && adb shell input keyevent 26



#nc localhost 1111

