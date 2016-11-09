#!/bin/bash

echo "backup"
mkdir -p ./moumon-bckp/home/$USER/.config/upstart
mkdir -p ./moumon-bckp/storage/ca-st-ro/bin
cp /storage/ca-st-ro/bin/moumon ./moumon-bckp/storage/ca-st-ro/bin/
cp /home/$USER/.config/upstart/moumon.conf ./moumon-bckp/home/$USER/.config/upstart/

echo "switch automount-open off"
gsettings set org.gnome.desktop.media-handling automount-open false

echo "install new moumon"
killall moumon
tar xvf moumon-pkg.tar.gz
rsync -av ./pkg/storage/ /storage/
rsync -av ./pkg/ubuntuhome/ /home/$USER/
rm -r pkg
killall moumon

echo "done"
