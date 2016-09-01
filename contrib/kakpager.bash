#! /usr/bin/env bash
##
## kakpager.bash by lenormf
## Instead of using `kak` directly in a shell pipeline to use the editor as a
## pager, use this script instead
## Examples:
##   man -P /path/to/kakpager.bash kak
##   cat /etc/passwd | /path/to/kakpager.bash
##

set -e

readonly PATH_TMP=$(mktemp)

rm -f "${PATH_TMP}"
mkfifo "${PATH_TMP}"
trap "rm -f '${PATH_TMP}'" EXIT ERR

(cat > "${PATH_TMP}" && exit &)

kak "${PATH_TMP}"

exit $?
