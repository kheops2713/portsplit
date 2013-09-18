#/bin/bash

TITLE="PORTSPLIT"
SECTION=1
AUTHOR="KheOps <kheops@ceops.eu>"
# Fix me (should use git to retrieve the last commit date for the markdown)
DATE=2013-09-18
# Fix me (maybe use DESCRIPTION to have the version or something)
DESCRIPTION='TCP\ port\ multiplexer'

INPUT=portsplit.md
OUTPUT=portsplit.1

if ! which pandoc >/dev/null 2>&1; then
  echo pandoc not found but is needed to generate manpage from markdown file.
  exit 1
fi

pandoc -s -t man -V title="$TITLE" -V section=$SECTION -V author="$AUTHOR" -V date=$DATE -V description="$DESCRIPTION" -o $OUTPUT $INPUT
