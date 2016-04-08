cd minicap
./run.sh &
cd ..
cd minitouch
./run.sh &
cd ..
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

