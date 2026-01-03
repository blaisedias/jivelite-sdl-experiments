#!/bin/sh
SCRIPTPATH=$(realpath "$0")
echo "$SCRIPTPATH"

function pcp_setup {
    echo "export XDG_RUNTIME_DIR=/home/tc/.X.d"
    export XDG_RUNTIME_DIR=/home/tc/.X.d
# touch screen detection code copied from jivelite.sh on piCorePlayer
# Autodetect Touchscreen, and then mouse if no touch is found.
TOUCH=0
MOUSE=0
TMPFILE=$(mktemp)
udevadm info --export-db | grep "P: " | grep "event" | sed 's/P: //' > $TMPFILE
for LINE in $(cat $TMPFILE); do
    udevadm info --query=property --path=$LINE | grep -q TOUCH
    [ $? -eq 0 ] && eventno=$(basename $LINE);TOUCH=1;MOUSE=0
    if [ "$eventno" != "" ]; then
#        echo -e "Automatic touchscreen detection found input device on $eventno: \nDevice Info:" >> $LOG
#        udevadm info --query=all --path=${LINE%/*} >> $LOG

        # Some touchpanels need multitouch (known: MPI7003)
        udevadm info --query=all --path=${LINE%/*} | grep -q "MPI7003"
        if [ $? -eq 0 ]; then
#            echo "Detected touchscreen driver requiring multitouch, enabling driver..." >> $LOG
            echo "Detected touchscreen driver requiring multitouch, enabling driver..."
            modprobe hid_multitouch
#            sed -i 's/^# module debounce/module debounce/' /usr/local/etc/ts.conf
        fi
        break
    fi
    udevadm info --query=property --path=$LINE | grep -q MOUSE
    [ $? -eq 0 ] && eventno=$(basename $LINE);TOUCH=0;MOUSE=1
#    [ "$eventno" != "" ] && echo "Found mouse: $eventno" >> $LOG && break
    [ "$eventno" != "" ] && echo "Found mouse: $eventno" && break
done
rm -f $TMPFILE

if [ x"" != x"$eventno" -a $TOUCH -eq 1 ]; then
#    export JIVE_NOCURSOR=1
    export TSLIB_TSDEVICE=/dev/input/$eventno
    echo "export TSLIB_TSDEVICE=/dev/input/$eventno"
#    export SDL_MOUSEDRV=TSLIB
#    echo "export SDL_MOUSEDRV=TSLIB" >> $LOG
#    export SDL_MOUSEDEV=$TSLIB_TSDEVICE
#    echo "export SDL_MOUSEDEV=$TSLIB_TSDEVICE" >> $LOG
fi
}

if [ -f "/usr/local/etc/pcp/pcpversion.cfg" ]; then
    echo "Platform is piCorePlayer, setting up touch screen"
    pcp_setup
fi

./bin/sqvumeter dl ./lib/TubeD.so  dl ./lib/Chevrons.so dl ./lib/PurpleTastic.so bg ./images/wallpapers/wallhaven1.jpg vsync vu PurpleTastic2Transparent $*
