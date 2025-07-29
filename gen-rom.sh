#!/bin/bash

EMU="$(dirname $0)"/external/umac_multidrive

if [ $# != 5 ]; then
  echo "usage: $0 <rom-in> <rom-out.h> <mem-size> <disp-width> <disp-height>" >& 2
  exit 1
fi

ROM_IN="$1"
ROM_OUT_H="$2"
MEMSIZE=$3
DISP_WIDTH=$4
DISP_HEIGHT=$5

echo "Patching rom with MEMSIZE=$MEMSIZE DISP_WIDTH=$DISP_WIDTH DISP_HEIGHT=$DISP_HEIGHT" 
make -C "$EMU" clean
make -C "$EMU" MEMSIZE=$MEMSIZE DISP_WIDTH=$DISP_WIDTH DISP_HEIGHT=$DISP_HEIGHT
"$EMU"/main -r "$ROM_IN" -W rom.bin
xxd -i < rom.bin > "$ROM_OUT_H"
