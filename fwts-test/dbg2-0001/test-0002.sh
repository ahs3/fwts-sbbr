#!/bin/bash
#
TEST="Test apcitables against invalid DBG2"
NAME=test-0001.sh
TMPLOG=$TMP/dbg2.log.$$

$FWTS --log-format="%line %owner " -w 80 --dumpfile=$FWTSTESTDIR/dbg2-0001/acpidump-0002.log dbg2 - | cut -c7- | grep "^dbg2" > $TMPLOG
diff $TMPLOG $FWTSTESTDIR/dbg2-0001/dbg2-0002.log >> $FAILURE_LOG
ret=$?
if [ $ret -eq 0 ]; then
	echo PASSED: $TEST, $NAME
else
	echo FAILED: $TEST, $NAME
fi

rm $TMPLOG
exit $ret
