#!/bin/bash
#

MODULE=${1}

TEXT=$(cat /sys/module/hello/sections/.text)
BSS=$(cat /sys/module/hello/sections/.bss)
DATA=$(cat /sys/module/hello/sections/.data)

echo "add-symbol-file ./${MODULE}.ko ${TEXT} -s .bss ${BSS} -s .data ${DATA}"
