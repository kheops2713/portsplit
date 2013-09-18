#/bin/bash

TITLE="PORTSPLIT"
SECTION=1
AUTHOR="KheOps <kheops@ceops.eu>"
# Fix me (maybe use DESCRIPTION to have the version or something)
DESCRIPTION='TCP\ port\ multiplexer'

INPUT=portsplit.md
OUTPUT=portsplit.1

if [ -z "$DATE" ]; then
  DATE=$(git log -- $INPUT | head -n 3 | tail -n 1 | sed 's/^Date: *//' | sed -r 's/\+[0-9]{4}//' |date -f- +%Y-%m-%d)
fi

if [ -z "$DATE" ]; then
  echo Cannot get date for latest commit of file $INPUT, please set DATE variable by hand.
  exit 1
fi

if ! which pandoc >/dev/null 2>&1; then
  echo pandoc not found but is needed to generate manpage from markdown file.
  exit 1
fi

MD5_BEFORE=$(md5sum $OUTPUT 2>/dev/null)
pandoc -s -t man -V title="$TITLE" -V section=$SECTION -V author="$AUTHOR" -V date=$DATE -V description="$DESCRIPTION" -o $OUTPUT $INPUT
MD5_AFTER=$(md5sum $OUTPUT 2>/dev/null)

if [ "$MD5_BEFORE" != "$MD5_AFTER" ]; then
  echo MANPAGE CHANGED\! Consider committing it.
fi
