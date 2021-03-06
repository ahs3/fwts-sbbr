#!/bin/bash
#
TEST="Test table against WAET"
NAME=test-0001.sh
TMPLOG=$TMP/waet.log.$$

$FWTS --log-format="%line %owner " -w 80 --dumpfile=$FWTSTESTDIR/waet-0001/acpidump-0001.log waet - | cut -c7- | grep "^waet" > $TMPLOG
diff $TMPLOG $FWTSTESTDIR/waet-0001/waet-0001.log >> $FAILURE_LOG
ret=$?
if [ $ret -eq 0 ]; then
	echo PASSED: $TEST, $NAME
else
	echo FAILED: $TEST, $NAME
fi

rm $TMPLOG
exit $ret
