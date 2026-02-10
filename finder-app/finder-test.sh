#!/bin/sh
# Tester script for assignment 1 and assignment 2
# Author: Siddhant Jajoo

set -e
set -u

NUMFILES=10
WRITESTR=AELD_IS_FUN
WRITEDIR=/tmp/aeld-data
username=$(whoami)

if [ $# -lt 2 ]
then
	echo "Using default value ${WRITESTR} for string to write"
	if [ $# -lt 1 ]
	then
		echo "Using default value ${NUMFILES} for number of files to write"
	else
		WRITESTR=$1
	fi	
else
	NUMFILES=$1
	WRITESTR=$2
fi

MATCHSTR="The number of files are ${NUMFILES} and the number of matching lines are ${NUMFILES}"

echo "Writing ${NUMFILES} files containing string ${WRITESTR} to ${WRITEDIR}"

rm -rf "${WRITEDIR}"

if which writer > /dev/null; then
	WRITER_COMMAND=writer
else
	WRITER_COMMAND=./writer
fi	

if which finder.sh > /dev/null; then
	FINDER_COMMAND=finder.sh
else
	FINDER_COMMAND=./finder.sh
fi

# create $WRITEDIR if not assignment1
if [ -d "/etc/finder-app/conf/" ]; then
    CONF_DIR="/etc/finder-app/conf"
else
    CONF_DIR="./conf"
fi

if [ -f "${CONF_DIR}/username.txt" ]; then
    username=$(cat "${CONF_DIR}/username.txt")
fi

mkdir -p "$WRITEDIR"
if [ ! -d "$WRITEDIR" ]; then
    echo "$WRITEDIR could not be created"
    exit 1
fi

# echo "Removing the old writer utility and compiling as a native application"
# make clean
# make

for i in $( seq 1 $NUMFILES)
do
	 "$WRITER_COMMAND" "$WRITEDIR/${username}$i.txt" "$WRITESTR"
done

OUTPUTSTRING=$($FINDER_COMMAND "$WRITEDIR" "$WRITESTR")
echo "$OUTPUTSTRING" > /tmp/assignment4-result.txt

# remove temporary directories
rm -rf /tmp/aeld-data

set +e
echo ${OUTPUTSTRING} | grep "${MATCHSTR}"
if [ $? -eq 0 ]; then
	echo "success"
	exit 0
else
	echo "failed: expected  ${MATCHSTR} in ${OUTPUTSTRING} but instead found"
	exit 1
fi
