#! /usr/bin/env bash

# run in the directory where this script is located
dir=${0%/*}
cd "${dir:-.}" || exit

basedir="../kdbg/"	# root of translatable sources
project="kdbg"		# project name

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
	-kI18N_NOOP:1 \
	-kI18N_NOOP2:1c,2 \
	-kaliasLocale \
	-kki18n:1 \
	-kki18nc:1c,2 \
	-kki18np:1,2 \
	-kki18ncp:1c,2,3 \
	--files-from="$infiles" -D "$basedir" -D . -o "$project".pot || exit

echo "Merging translations"

for cat in *.po; do
	echo $cat
	msgmerge -o "$cat.new" "$cat" "$project".pot &&
	mv "$cat.new" "$cat" || exit
done

echo "Restoring POT-Creation-Date headers from Git"

restore_po_creation_date_from_git() {
    local filename="${1}"
    local original="$(git show "HEAD:./${filename}" 2>/dev/null | grep -nF POT-Creation-Date:)"
    if [[ -z ${original} ]]; then
        echo "[*] Skipped unversioned file \"${filename}\"."
        return 0
    fi
    local line_number="${original%%:*}"
    local line_content="${original#*:}"
    {
        sed -n "1,$(( line_number - 1 ))p" "${filename}"
        echo "${line_content}"
        sed -n "$(( line_number + 1 )),\$p" "${filename}"
    } | sponge "${filename}"
    echo "[+] Restored POT-Creation-Date for file \"${filename}\"."
}

for i in $(git ls-files \*.po \*.pot); do
    restore_po_creation_date_from_git "${i}"
done

echo "Done"
