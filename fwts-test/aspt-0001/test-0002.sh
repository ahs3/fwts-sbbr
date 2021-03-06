#!/bin/bash
#
TEST="Test apcitables against invalid ASPT"
NAME=test-0001.sh
TMPLOG=$TMP/aspt.log.$$

$FWTS --log-format="%line %owner " -w 80 --dumpfile=$FWTSTESTDIR/aspt-0001/acpidump-0002.log aspt - | cut -c7- | grep "^aspt" > $TMPLOG
diff $TMPLOG $FWTSTESTDIR/aspt-0001/aspt-0002.log >> $FAILURE_LOG
ret=$?
if [ $ret -eq 0 ]; then
	echo PASSED: $TEST, $NAME
else
	echo FAILED: $TEST, $NAME
fi

rm $TMPLOG
exit $ret
