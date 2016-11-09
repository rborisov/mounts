#!/bin/bash

gsettings set org.gnome.desktop.media-handling automount-open true

killall moumon
rm /storage/ca-st-ro/bin/moumon
rm /home/rborisov/.config/upstart/moumon.conf
