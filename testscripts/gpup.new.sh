#!/bin/sh
date
../conf/deviceid.sh ../conf/gpup_single.conf
echo 'gpup';sudo ../../../bin/rvs -c ../conf/gpup_single.conf -d 3; date 
