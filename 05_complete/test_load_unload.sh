#!/bin/bash
for i in {1..50}
do
        sh mobchar_load.sh
        sleep 0.1
        sh mobchar_unload.sh
        echo "Unload $i times"
done