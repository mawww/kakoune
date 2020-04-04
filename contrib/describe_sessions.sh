#!/bin/sh
##
## describe_sessions.sh for kakoune
## by lenormf
##

readonly KAK_SCRIPT='
    {
        echo

        printf "Session: %s\n" "${kak_session}"
        printf "Current working directory: %s\n" "${PWD}"

        eval set -- "${kak_buflist}"
        printf "Buffers (%d):\n" $#
        for bufname in "$@"; do
            printf "\t%s\n" "${bufname}"
        done

        eval set -- "${kak_client_list}"
        printf "Clients (%d):\n" $#
        for clientname in "$@"; do
            printf "\t%s\n" "${clientname}"
        done
    } >>{{outfile}}

    rm -rf {{sentinel}}
'

main() {
    outfile=$(mktemp "${TMPDIR:-/tmp}"/kak-describe_sessions.XXXXXXXX)
    nb_sessions=0
    nb_dead_sessions=0
    nb_suspended_sessions=0

    if ! command -v socat >/dev/null 2>&1; then
        echo "Unmet dependency: socat" >&2
        exit 1
    fi

    script=$(printf 'nop %%sh{ %s }' "${KAK_SCRIPT}" | sed "s,{{outfile}},\"${outfile}\",g")

    sessions_dir="${TMPDIR:-/tmp}/kakoune/${USER}"
    if [ -n "${XDG_RUNTIME_DIR}" ]; then
        sessions_dir="${XDG_RUNTIME_DIR}/kakoune"
    fi

    for session in "${sessions_dir}"/*; do
        name_session="${session##*/}"

        if printf '' | socat - UNIX-CONNECT:"${session}",connect-timeout=1 2>/dev/null; then
            sentinel=$(mktemp -d "${TMPDIR:-/tmp}"/kak-sentinel.XXXXXXXX)
            script_session=$(printf %s "${script}" | sed "s,{{sentinel}},\"${sentinel}\",g")

            if ! printf %s "${script_session}" | kak -p "${name_session}"; then
                printf '\nSession "%s" dead\n' "${name_session}" >> "${outfile}"
                nb_dead_sessions=$((nb_dead_sessions + 1))
                continue
            fi

            wait_limit=2
            while ! mkdir "${sentinel}" 2>/dev/null && [ "${wait_limit}" -gt 0 ]; do
                wait_limit=$((wait_limit - 1))
                sleep 1
            done

            rm -rf "${sentinel}"

            if [ "${wait_limit}" -gt 0 ]; then
                nb_sessions=$((nb_sessions + 1))
            else
                printf '\nSession "%s" suspended\n' "${name_session}" >> "${outfile}"
                nb_suspended_sessions=$((nb_suspended_sessions + 1))
            fi
        else
            nb_dead_sessions=$((nb_dead_sessions + 1))
        fi
    done

    printf 'Running sessions: %d\nSuspended sessions: %d\nDead sessions: %d\n' \
        "${nb_sessions}" "${nb_suspended_sessions}" "${nb_dead_sessions}"
    cat "${outfile}"

    rm -f "${outfile}"
}

main "$@"
