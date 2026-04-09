#!/bin/sh
# Tester script for assignment 1 and assignment 2
# Author: Siddhant Jajoo
# A4: prefer /etc/finder-app/conf and PATH on target; fall back to ../conf + script dir locally.

set -e
set -u

APP_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)

NUMFILES=10
WRITESTR=AELD_IS_FUN
WRITEDIR=/tmp/aeld-data

if [ -f /etc/finder-app/conf/username.txt ]; then
	CONF_DIR=/etc/finder-app/conf
else
	CONF_DIR="${APP_DIR}/../conf"
fi
username=$(cat "${CONF_DIR}/username.txt")

if [ $# -lt 3 ]
then
	echo "Using default value ${WRITESTR} for string to write"
	if [ $# -lt 1 ]
	then
		echo "Using default value ${NUMFILES} for number of files to write"
	else
		NUMFILES=$1
	fi	
else
	NUMFILES=$1
	WRITESTR=$2
	WRITEDIR=/tmp/aeld-data/$3
fi

MATCHSTR="The number of files are ${NUMFILES} and the number of matching lines are ${NUMFILES}"

echo "Writing ${NUMFILES} files containing string ${WRITESTR} to ${WRITEDIR}"

rm -rf "${WRITEDIR}"

assignment=$(cat "${CONF_DIR}/assignment.txt")

if [ "$assignment" != 'assignment1' ]
then
	mkdir -p "$WRITEDIR"

	if [ -d "$WRITEDIR" ]
	then
		echo "$WRITEDIR created"
	else
		exit 1
	fi
fi

if [ "$assignment" = "assignment1" ]
then
	if command -v writer.sh >/dev/null 2>&1; then
		WRITER=writer.sh
	else
		WRITER="${APP_DIR}/writer.sh"
	fi
else
	if command -v writer >/dev/null 2>&1; then
		WRITER=writer
	else
		WRITER="${APP_DIR}/writer"
	fi
fi

if command -v finder.sh >/dev/null 2>&1; then
	FINDER=finder.sh
else
	FINDER="${APP_DIR}/finder.sh"
fi

for i in $( seq 1 $NUMFILES)
do
	$WRITER "$WRITEDIR/${username}$i.txt" "$WRITESTR"
done

OUTPUTSTRING=$($FINDER "$WRITEDIR" "$WRITESTR")
printf '%s\n' "${OUTPUTSTRING}" > /tmp/assignment4-result.txt

rm -rf /tmp/aeld-data

set +e
echo "${OUTPUTSTRING}" | grep "${MATCHSTR}"
if [ $? -eq 0 ]; then
	echo "success"
	exit 0
else
	echo "failed: expected  ${MATCHSTR} in ${OUTPUTSTRING} but instead found"
	exit 1
fi
