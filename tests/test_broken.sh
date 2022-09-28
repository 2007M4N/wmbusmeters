#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test broken telegrams"
TESTRESULT="ERROR"

cat > $TEST/test_expected.txt <<EOF
(dvparser) warning: unexpected end of data
(dvparser) found new format "046D036E51706CE1F14302FF2C0259D40902FD66A000" with hash 48a9, remembering!
(dvparser) warning: unexpected end of data
(dvparser) warning: unexpected end of data
(dvparser) found new format "046D0406041301FD17426C4406840106840206840306840406840506840606840706840806840906C1337F47A64E0C062364" with hash b934, remembering!
(meter) while processing field extractors ignoring dventry 0C06 at offset 129 matching since field total was already matched against dventry 0406 at offset 25 !
(dvparser) found new format "046D0406041301FD17426C4406840106840206840306840406840506840606840706840806840906585D65E6958F6B5E93DBA60CD99D06EB27D97106000000840F060003620501" with hash 6c76, remembering!
(meter) while processing field extractors ignoring dventry 0000 at offset 146 matching since field total was already matched against dventry 0406 at offset 25 !
(meter) while processing field extractors ignoring dventry 0600 at offset 151 matching since field total was already matched against dventry 0406 at offset 25 !
(meter) while processing field extractors ignoring dventry 0501 at offset 166 matching since field total was already matched against dventry 0406 at offset 25 !
EOF

$PROG --format=fields --selectfields=id,current_consumption_hca,device_date_time --debug simulations/simulation_broken.txt \
      HCA auto 27293981 NOKEY \
      HEAT sensostar 22256495 859A9D0F5DC2BAD679644E4FB6F9CE29 \
      2>&1 | grep 'ignoring\|dvparser' > $TEST/test_output.txt

if [ "$?" = "0" ]
then
    diff $TEST/test_expected.txt $TEST/test_output.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    fi
else
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
fi
