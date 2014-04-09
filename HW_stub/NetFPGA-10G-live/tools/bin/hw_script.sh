#! /bin/bash

#cd ../scripts/
#./impact_run.sh ../../projects/bir_starter/bitfiles/system.bit
#/root/netfpga/bir/HW_stub/NetFPGA-10G-live/projects/bir_stater_final_smartX/hw/smartX/best_run/system.bit  #../../projects/bir_starter/bitfiles/download.bit 
#cd ../bin/


while read line  
do 
#echo $line

#cd ../scripts/
#./impact_run.sh ../../projects/bir_starter/bitfiles/system.bit
#cd ../bin/
major=$(echo $line | cut -d"_" -f2)
minor=$(echo $line | cut -d"_" -f3)
echo $major" "$minor" test"
./nf_test.py hw --major $major --minor $minor | tee hw_results/result"_"$major"_"$minor  # --isim  > results/result"_"$major"_"$minor
sleep 2


done < list
