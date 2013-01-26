#!/bin/bash

EXPECTED_ARGS=3

if [ $# -ne $EXPECTED_ARGS ]
then
  echo "Usage: `basename $0` {arg}"
  exit $E_BADARGS
fi

mysql="mysql -u bible -pbible bible"
query="select '+OK', versetext from bibledb_kjv where bookid='%s' and chapterno='%s' and verseno='%s'";
query2=`printf "$query" $1 $2 $3`

line=$(echo $query2 | $mysql | tail -n +2)

if [[ $line == +OK* ]] ;
then
  echo $line | cut -b 5-
else
  echo "5You're doing it wrong, atheist faggot."
fi
