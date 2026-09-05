#!/bin/sh

# run in the directory where this script is located

if [ $# = 0 ]
then
	echo >&2 "usage: $0 <pofile>..."
	echo >&2 "usage: $0 --init <pofile>"
	exit 1
fi

dir=${0%/*}
cd "${dir:-.}" || exit

basedir="../kdbg/"	# root of translatable sources
project="kdbg"		# project name
if [ "$1" = --init ]
then
	init=1
	shift
fi

echo "Preparing rc files"

(
	cd "$basedir" &&
	extractrc $(ls -1 *.rc *.ui)
) > rc.cpp || exit

echo "Extracting messages"

infiles=$(mktemp) || exit
trap 'rm -f "$infiles" rc.cpp' EXIT HUP INT TERM

(
	cd "$basedir" &&
	ls -1 *.cpp *.h &&
	echo rc.cpp
) > "$infiles" &&
xgettext --from-code=UTF-8 -C -kde \
	-ci18n \
	-ki18n:1 \
	-ki18nc:1c,2 \
	-ki18np:1,2 \
	-ki18ncp:1c,2,3 \
	-ktr2i18n:1 \
	-kkli18n:1 \
	-kkli18nc:1c,2 \
	-kaliasLocale \
	-kki18n:1 \
	-kki18nc:1c,2 \
	-kki18np:1,2 \
	-kki18ncp:1c,2,3 \
	--files-from="$infiles" -D "$basedir" -D . -o "$project".pot || exit

echo "Merging translations"

for cat
do
	echo $cat
	if [ -z "$init" ]
	then
		msgmerge --add-location --backup=off -U "$cat" "$project".pot || exit
	else
		msginit --input="$project".pot --locale="${cat%.po}" --output="$cat" || exit
	fi
done

echo "Done"
