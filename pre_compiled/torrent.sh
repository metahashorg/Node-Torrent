#!/bin/bash

workdir=/opt/mh-torrent
log=torrent.log
conf=config.conf

function startTorrent() {

    status
    if [ $res -eq 0 ]
    then
        echo Service Torrent already running, pid $pid
    else
        ulimit -c unlimited
        if [ -d $workdir ]
        then
           cd $workdir

           if [ ! -f $conf ]
           then
               echo "No config file $conf found, exiting."
               exit 2
           fi
           echo Starting Service Torrent
           ./torrent $conf > $log &
        else
           echo "Working directory $workdir does not exists"
           exit 2;
        fi
    fi


}


function stopTorrent() {


    status
    if [ $res -ne 0 ]
       then
               echo Service Torrent not running
    else

        echo Stopping service torrent, pid $pid....
        kill $pid
        sleep 4
        status

        if [ $res -eq 0 ]
        then
            echo Stop failed. Torrent still alive, pid $pid. Please check manually
        fi

    fi
}

function status() {

    pid=`pidof torrent`
    res=$?
}


case "$1" in
start)
    startTorrent
    ;;

stop)
    stopTorrent
    ;;

restart)
    stopTorrent
    startTorrent
    ;;

status)
     status

    if [ $res -ne 0 ]
       then
               echo -e "Service Torrent \e[31mnot running\e[0m"
       else
               echo -e "Service Torrent is \e[32mrunning\e[0m. Pid $pid"
       fi
     ;;

 *)
     echo "Usage: $0 start|stop|status|restart"
     exit 1
    ;;
esac
