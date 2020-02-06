#!/bin/bash

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test T1 meters"
TESTRESULT="ERROR"

METERS="MyWarmWater supercom587 12345678 NOKEY
      MyColdWater supercom587 11111111 NOKEY
      MoreWater   iperl       12345699 NOKEY
      WaterWater  iperl       33225544 NOKEY
      Wasser      apator162   20202020 NOKEY
      MyTapWatera apator162   21202020 NOKEY
      MyTapWaterb apator162   22202020 NOKEY
      MyTapWaterc apator162   23202020 NOKEY
      MyTapWaterd apator162   24202020 NOKEY
      MyElectricity1 amiplus  10101010 NOKEY
      Duschen     mkradio3    34333231 NOKEY
      HeatMeter   vario451    58234965 NOKEY
      HeatMeter   eurisii     88018801 NOKEY
      Tempoo      lansenth    00010203 NOKEY
      Rummet      rfmamb      11772288 NOKEY
      IzarWater   izar        21242472 NOKEY
      IzarWater2  izar        66290778 NOKEY
      HydrusWater hydrus      64646464 NOKEY
      HydrodigitWater hydrodigit 86868686 NOKEY
      Q400Water   q400        72727272 AAA896100FED12DD614DD5D46369ACDD
      Elen1       ebzwmbe     22992299 NOKEY
      Elen2       esyswm      77997799 NOKEY
      Elen3       ehzp        55995599 NOKEY"

cat simulations/simulation_t1.txt | grep '^{' > $TEST/test_expected.txt
$PROG --format=json simulations/simulation_t1.txt $METERS  > $TEST/test_output.txt
if [ "$?" == "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" == "0" ]
    then
        echo OK json: $TESTNAME
        TESTRESULT="OK"
    fi
fi

cat simulations/simulation_t1.txt | grep '^|' | sed 's/^|//' > $TEST/test_expected.txt
$PROG --format=fields simulations/simulation_t1.txt $METERS  > $TEST/test_output.txt
if [ "$?" == "0" ]
then
    cat $TEST/test_output.txt | sed 's/[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9].[0-9][0-9]$/1111-11-11 11:11.11/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" == "0" ]
    then
        echo OK fields: $TESTNAME
        TESTRESULT="OK"
    fi
fi


if [ "$TESTRESULT" = "ERROR" ]
then
    echo ERROR: $TESTNAME
    exit 1
fi
