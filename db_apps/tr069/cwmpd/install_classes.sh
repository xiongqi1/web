#!/bin/sh
# usage: install_classes.sh srcDir destDir

EXT=lua
cd $1
for cls in `find . -type f -a -name \*.$EXT`; do
	mkdir -p `dirname $2/$cls`
	cp $cls $2/$cls
done
