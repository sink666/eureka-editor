#!/bin/bash
set -e

if [ ! -d glbsp_src ]; then
  echo "Run this script from the top level."
  exit 1
fi

echo "Creating source package for Eureka..."

cd ..

src=eureka
dest=PACK-SRC

mkdir $dest

#
#  Source code
#
mkdir $dest/src
cp -av $src/src/*.[chr]* $dest/src
cp -av $src/Makefile* $dest/
# cp -av $src/src/*.ico $dest/src

mkdir $dest/glbsp_src
cp -av $src/glbsp_src/*.[chr]* $dest/glbsp_src

mkdir $dest/misc
cp -av $src/misc/*.* $dest/misc

mkdir $dest/obj_linux
mkdir $dest/obj_linux/glbsp

#
#  Data files
#
mkdir $dest/common
cp -av $src/common/*.* $dest/common || true

mkdir $dest/games
cp -av $src/games/*.* $dest/games || true

mkdir $dest/ports
cp -av $src/ports/*.* $dest/ports || true

mkdir $dest/mods
cp -av $src/mods/*.* $dest/mods || true

mkdir $dest/data/doom1_boss
mkdir $dest/data/doom2_boss

cp -av $src/data/doom1_boss/*.* $dest/data/doom1_boss
cp -av $src/data/doom2_boss/*.* $dest/data/doom2_boss

#
#  Documentation
#
cp -av $src/*.txt $dest

mkdir $dest/docs
cp -av $src/docs/*.* $dest/docs

rm $dest/docs/New_Workflow.txt
rm $dest/docs/Features.txt

#
# all done
#
echo "------------------------------------"
echo "All done."

