#!/bin/bash

killall moumon
tar xvf moumon-pkg.tar.gz
#cp -r pkg/storage/* /storage/
#cp -r pkg/ubuntuhome/* /home/rborisov/
rsync -av pkg/storage/ /storage/
rsync -av pkg/ubuntuhome/ /home/rborisov/
rm -r pkg
killall moumon
