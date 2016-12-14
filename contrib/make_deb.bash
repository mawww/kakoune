#! /usr/bin/env bash
##
## make_deb.bash for kakoune
## by lenormf
##
## Dependencies: build-essential, devscripts, debmake, asciidoc
## Guidelines for making binary packages: https://www.debian.org/doc/debian-policy/ch-binary.html

set -e

readonly DEFAULT_PACKAGE_NAME="kakoune"
readonly DEFAULT_PACKAGE_TYPE="bin"
readonly DEFAULT_PACKAGE_DEPENDENCIES=( "libboost-all-dev (>= 1.50)" "libncursesw5-dev (>= 5.3)" )
readonly DEFAULT_PACKAGE_HOMEPAGE="http://kakoune.org/"
readonly DEFAULT_PACKAGE_SOURCE="https://github.com/mawww/kakoune"
readonly DEFAULT_PACKAGE_LICENSE="UNLICENSE"
readonly DEFAULT_DESCRIPTION_ONELINE="mawww's experiment for a better code editor"
readonly DEFAULT_DESCRIPTION_DETAILS=$(cat << EOF
Vim inspired — Faster as in less keystrokes — Multiple selections — Orthogonal design

Kakoune is a code editor heavily inspired by Vim, as such most of its commands are similar to vi’s ones, and it shares Vi’s "keystrokes as a text editing language" model.
Kakoune can operate in two modes, normal and insertion. In insertion mode, keys are directly inserted into the current buffer. In normal mode, keys are used to manipulate the current selection and to enter insertion mode.
Kakoune has a strong focus on interactivity, most commands provide immediate and incremental results, while still being competitive (as in keystroke count) with Vim.
Kakoune works on selections, which are oriented, inclusive range of characters, selections have an anchor and a cursor character. Most commands move both of them, except when extending selection where the anchor character stays fixed and the cursor one moves around.
EOF
)

function usage {
    echo "Usage: $1 [-e email] [-f full name]"
    exit
}

## Replace the content of a field (<name>: <value>), add it if non-existent
function replace_field {
    local field_name="$1"
    local path_file="$2"
    local field_value="$3"

    grep -q "^${field_name}: " "${path_file}" \
        && sed -r -i "s,^(${field_name}:).+\$,\1 ${field_value}," "${path_file}" \
        || echo "${field_name}: ${field_value}" >> "${path_file}"
}

## Append a value to a field
function append_field {
    local field_name="$1"
    local path_file="$2"
    local field_value="$3"

    grep -q "^${field_name}: " "${path_file}" \
        && sed -r -i "s#^(${field_name}:)\s*(.+)\$#\1 \2, ${field_value}#" "${path_file}" \
        || echo "${field_name}: ${field_value}" >> "${path_file}"
}

function main {
    readonly PATH_DIR_CURRENT="${PWD}"
    local maintainer_email="${DEBEMAIL}"
    local maintainer_fullname="${DEBFULLNAME}"

    while getopts he:f: o; do
        case "${o}" in
            e) maintainer_email="${OPTARG}";;
            f) maintainer_fullname="${OPTARG}";;
            *) usage "$0";;
        esac
    done

    if [ -z "${maintainer_email}" ]; then
        echo "No maintainer email detected, set one using the '-e' flag"
        exit
    elif [ -z "${maintainer_fullname}" ]; then
        echo "No maintainer full name detected, set one using the '-f' flag"
        exit
    fi

## os.getlogin() does not always work, e.g. in docker
    export DEBEMAIL="${maintainer_email}"
    export DEBFULLNAME="${maintainer_fullname}"

    readonly PATH_KAKOUNE=$(readlink -e $(dirname $(readlink -f "$0"))/..)
    readonly PATH_DIR_TMP=$(mktemp -d)
## TODO: assign the proper kakoune version whenever possible
    readonly VERSION_KAKOUNE=$(git show --pretty=%ci | egrep -om 1 '[0-9]{4}-[0-9]{2}-[0-9]{2}' | sed 's/-/./g').$(git show --pretty=%h | sed -n 1p)
    readonly DIR_KAKOUNE="kakoune-${VERSION_KAKOUNE}"
    readonly PATH_DIR_WORK="${PATH_DIR_TMP}/${DIR_KAKOUNE}"

    echo "Detected path to the kakoune project: ${PATH_KAKOUNE}"
    echo "Path to the temporary directory: ${PATH_DIR_TMP}"
    echo "Path to the work directory: ${PATH_DIR_WORK}"
    echo "Version of the package: ${VERSION_KAKOUNE}"

    echo "Copying the source over to the work directory"
    cp -r "${PATH_KAKOUNE}/src" "${PATH_DIR_WORK}"

    echo "Copying the tests over to the temporary directory"
    cp -r "${PATH_KAKOUNE}/test" "${PATH_DIR_TMP}"

    echo "Copying additional directories over to the temporary directory"
    cp -r "${PATH_KAKOUNE}/"{share,rc,colors,doc} "${PATH_DIR_TMP}"

    echo "Creating a symlink in the source code to allow tests to run"
    ln -s "${PATH_DIR_WORK}" "${PATH_DIR_TMP}/src"

    echo "Copying the license file in the source directory"
    fmt -w 72 < "${PATH_KAKOUNE}/UNLICENSE" > "${PATH_DIR_WORK}/LICENSE"

    echo "Changing directory to the temporary one"
    cd "${PATH_DIR_TMP}"

## FIXME: make a patch
    echo "Deactivating the debug mode in the makefile"
    sed -i 's/debug ?= yes/debug ?= no/' "${DIR_KAKOUNE}/Makefile"

## FIXME: make a patch
    echo "Disabling copying the README file to the doc directory"
    sed -r -i 's,(install.*\.\./README\.asciidoc.+),#\1,' "${DIR_KAKOUNE}/Makefile"

## FIXME: make a patch
    echo "Setting the prefix of the installation procedure"
    sed -r -i 's,(PREFIX \?=) .+,\1 /usr,' "${DIR_KAKOUNE}/Makefile"

## FIXME: make a patch
    echo "Removing debug symbols from the compilation flags"
    sed -r -i 's,^(CXXFLAGS.+)-g\b,\1,' "${DIR_KAKOUNE}/Makefile"

    echo "Creating a tar archive of the code"
    tar cf "${DIR_KAKOUNE}.tar" "${DIR_KAKOUNE}"

    echo "Compressing the tar archive of the code"
    gzip "${PATH_DIR_WORK}.tar"

    echo "Changing directory to the work one"
    cd "${PATH_DIR_WORK}"

    echo "Package maintainer info: ${maintainer_fullname} (${maintainer_email})"
    echo "Path to the license file: ${PATH_LICENSE}"

    echo "Initializing package creation"
    debmake -p "${DEFAULT_PACKAGE_NAME}" \
        -b "${DEFAULT_PACKAGE_NAME}:${DEFAULT_PACKAGE_TYPE}" \
        -e "${maintainer_email}" \
        -f "${maintainer_fullname}"

    echo "Adding the homepage to the control file"
    replace_field Homepage debian/control "${DEFAULT_PACKAGE_HOMEPAGE}"

    echo "Adding the dependencies to the control file"
    local list_dependencies=""
    for dep in "${DEFAULT_PACKAGE_DEPENDENCIES[@]}"; do
        test -n "${list_dependencies}" && list_dependencies="${list_dependencies}, "
        list_dependencies="${list_dependencies}${dep}"
    done
    echo "List of dependencies that will be added: ${list_dependencies}"
    sed -r -i "s/^(Depends: .+)\$/\1, ${list_dependencies}/" debian/control

## FIXME: follow these guidelines https://www.debian.org/doc/debian-policy/ch-binary.html#s-descriptions
    echo "Adding a description to the control file"
    replace_field Description debian/control "${DEFAULT_DESCRIPTION_ONELINE}"
    ## Remove the last two lines of the control files, containing a default description text generated by debmake
    sed -i -n -e :a -e '1,2!{P;N;D;};N;ba' debian/control
    fmt -w 120 <<< "${DEFAULT_DESCRIPTION_DETAILS}" | sed -e 's/ +/ /g' -e 's/^$/./g' -e 's/^/ /g' >> debian/control

    echo "Adding the version of the package to the control file"
    echo "Version: ${VERSION_KAKOUNE}" >> debian/control
    replace_field Version debian/control "${VERSION_KAKOUNE}"

    echo "Assigning a section to the control file"
    replace_field Section debian/control editors

    echo "Adding asciidoc to the list of build dependencies"
    append_field Build-Depends debian/control 'asciidoc'

    echo "Adding package recommendations"
    replace_field Suggests debian/control 'tmux, x11-utils, xdotool, clang, exuberant-ctags'

## TODO: generate a changelog
    echo -e "kakoune (${VERSION_KAKOUNE}) stable; urgency=low\n\n  * Initial release\n\n -- ${maintainer_fullname} <${maintainer_email}>  $(date -R)" > debian/changelog

    echo "Modifying the source in the copyright file"
    replace_field Source debian/copyright "${DEFAULT_PACKAGE_SOURCE}"

    echo "Modifying the license in the copyright file"
    replace_field License debian/copyright "${DEFAULT_PACKAGE_LICENSE}"

    echo "Generating an NROFF man page"
    a2x --no-xmllint -f manpage "${PATH_DIR_TMP}/doc/kak.1.txt"

    echo "Copying the generated manpage into the build directory"
    mv "${PATH_DIR_TMP}/doc/kak.1" debian/kak.1.ex

    echo "Building the package"
    debuild -eCXXFLAGS="" -i -us -uc -b

    readonly PATH_PACKAGE_CREATED=$(find "${PATH_DIR_TMP}" -type f -iname 'kakoune_*.deb' | sed -n 1p)
    if [ -z "${PATH_PACKAGE_CREATED}" ]; then
        echo "WARNING: no package was created, check the logs at ${PATH_DIR_TMP}"
        exit 1
    fi

    echo "Path of the package created: ${PATH_PACKAGE_CREATED} (copying to the current directory)"
    cp "${PATH_PACKAGE_CREATED}" "${PATH_DIR_CURRENT}"

    rm -rf "${PATH_DIR_TMP}"
}

main "$@"
