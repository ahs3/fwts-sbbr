#!/bin/bash
#
TEST="Test table against TPM2"
NAME=test-0001.sh
TMPLOG=$TMP/tpm2.log.$$

$FWTS --log-format="%line %owner " -w 80 --dumpfile=$FWTSTESTDIR/tpm2-0001/acpidump-0001.log tpm2 - | cut -c7- | grep "^tpm2" > $TMPLOG
diff $TMPLOG $FWTSTESTDIR/tpm2-0001/tpm2-0001.log >> $FAILURE_LOG
ret=$?
if [ $ret -eq 0 ]; then
	echo PASSED: $TEST, $NAME
else
	echo FAILED: $TEST, $NAME
fi

rm $TMPLOG
exit $ret
