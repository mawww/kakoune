declare-option -docstring "name of the client in which documentation is to be displayed" \
    str docsclient

declare-option -docstring "maximum number of search matches `:doc-search` returns from every topic" \
    int doc_max_search_results 10

declare-option -hidden range-specs doc_render_ranges
declare-option -hidden range-specs doc_links
declare-option -hidden range-specs doc_anchors
declare-option -hidden str-list    doc_search_matches

define-command -hidden -params 4 doc-render-regex %{
    evaluate-commands -draft %{ try %{
        execute-keys <percent> s %arg{1} <ret>
        execute-keys -draft s %arg{2} <ret> d
        execute-keys "%arg{3}"
        evaluate-commands %sh{
            face="$4"
            eval "set -- $kak_quoted_selections_desc"
            ranges=""
            for desc in "$@"; do ranges="$ranges '$desc|$face'"; done
            echo "update-option buffer doc_render_ranges"
            echo "set-option -add buffer doc_render_ranges $ranges"
        }
    } }
}

define-command -hidden doc-parse-links %{
    evaluate-commands -draft %{ try %{
        execute-keys <percent> s <lt><lt>(.*?),.*?<gt><gt> <ret>
        execute-keys -draft s <lt><lt>.*,|<gt><gt> <ret> d
        execute-keys H
        set-option buffer doc_links %val{timestamp}
        update-option buffer doc_render_ranges
        evaluate-commands -itersel %{
            set-option -add buffer doc_links "%val{selection_desc}|%reg{1}"
            set-option -add buffer doc_render_ranges "%val{selection_desc}|default+u"
        }
    } }
}

define-command -hidden doc-parse-anchors %{
    evaluate-commands -draft %{ try %{
        set-option buffer doc_anchors %val{timestamp}
        # Find sections as add them as imlicit anchors
        execute-keys <percent> s ^={2,}\h+([^\n]+)$ <ret>
        evaluate-commands -itersel %{
            set-option -add buffer doc_anchors "%val{selection_desc}|%sh{printf '%s' ""$kak_main_reg_1"" | tr '[A-Z ]' '[a-z-]'}"
        }

        # Parse explicit anchors and remove their text
        execute-keys <percent> s \[\[(.*?)\]\]\s* <ret>
        evaluate-commands -itersel %{
            set-option -add buffer doc_anchors "%val{selection_desc}|%reg{1}"
        }
        execute-keys d
        update-option buffer doc_anchors
    } }
}

define-command -hidden doc-jump-to-anchor -params 1 %{
    update-option buffer doc_anchors
    evaluate-commands %sh{
        anchor="$1"
        eval "set -- $kak_quoted_opt_doc_anchors"

        shift
        for range in "$@"; do
            if [ "${range#*|}" = "$anchor" ]; then
                printf '%s\n'  "select '${range%|*}'; execute-keys vv"
                exit
            fi
        done
        printf "fail No such anchor '%s'\n" "${anchor}"
    }
}

define-command -hidden doc-follow-link %{
    update-option buffer doc_links
    evaluate-commands %sh{
        eval "set -- $kak_quoted_opt_doc_links"
        for link in "$@"; do
            printf '%s\n' "$link"
        done | awk -v FS='[.,|#]' '
            BEGIN {
                l=ENVIRON["kak_cursor_line"];
                c=ENVIRON["kak_cursor_column"];
            }
            l >= $1 && c >= $2 && l <= $3 && c <= $4 {
                if (NF == 6) {
                    print "doc " $5
                    if ($6 != "") {
                        print "doc-jump-to-anchor %{" $6 "}"
                    }
                } else {
                    print "doc-jump-to-anchor %{" $5 "}"
                }
                exit
            }
        '
    }
}

define-command -params 1 -hidden doc-render %{
    execute-keys "!cat %arg{1}<ret>gg"

    doc-parse-anchors

    # Join paragraphs together
    try %{
        execute-keys -draft '%S\n{2,}|(?<lt>=\+)\n|^[^\n]+::\n|^\h*[*-]\h+<ret>' \
            <a-K>^\h*-{2,}(\n|\z)<ret> S\n\z<ret> <a-k>\n<ret> <a-j>
    }

    # Remove some line end markers
    try %{ execute-keys -draft <percent> s \h*(\+|:{2,})$ <ret> d }

    # Setup the doc_render_ranges option
    set-option buffer doc_render_ranges %val{timestamp}
    doc-render-regex \B(?<!\\)\*(?=\S)[^\n]+?(?<=\S)(?<!\\)\*\B \A|.\z 'H' default+b
    doc-render-regex \b(?<!\\)_(?=\S)[^\n]+?(?<=\S)(?<!\\)_\b \A|.\z 'H' default+i
    doc-render-regex \B(?<!\\)`(?=\S)[^\n]+?(?<=\S)`\B \A|.\z 'H' mono
    doc-render-regex ^=\h+[^\n]+ ^=\h+ '~' title
    doc-render-regex ^={2,}\h+[^\n]+ ^={2,}\h+ '' header
    doc-render-regex ^\h*-{2,}\n\h*.*?^\h*-{2,}\n ^\h*-{2,}\n '' block

    doc-parse-links

    # Remove escaping of * and `
    try %{ execute-keys -draft <percent> s \\((?=\*)|(?=`)) <ret> d }
    # Go to beginning of file
    execute-keys 'gg'

    set-option buffer readonly true
    add-highlighter buffer/ ranges doc_render_ranges
    add-highlighter buffer/ wrap -word -indent
    map buffer normal <ret> ': doc-follow-link<ret>'
}

define-command -params 0..2 \
    -shell-script-candidates %{
        if [ "$kak_token_to_complete" -eq 0 ]; then
            find -L \
                "${kak_config}/autoload/" \
                "${kak_runtime}/doc/" \
                "${kak_runtime}/rc/" \
                -type f -name "*.asciidoc" |
                sed 's,.*/,,; s/\.[^.]*$//'
        elif [ "$kak_token_to_complete" -eq 1 ]; then
            page=$(
                find -L \
                    "${kak_config}/autoload/" \
                    "${kak_runtime}/doc/" \
                    "${kak_runtime}/rc/" \
                    -type f -name "$1.asciidoc" |
                    head -1
            )
            if [ -f "${page}" ]; then
                awk '
                    /^==+ +/ { sub(/^==+ +/, ""); print }
                    /^\[\[[^\]]+\]\]/ { sub(/^\[\[/, ""); sub(/\]\].*/, ""); print }
                ' < $page | tr '[A-Z ]' '[a-z-]'
            fi
        fi
    } \
    doc -docstring %{
        doc <topic> [<keyword>]: open a buffer containing documentation about a given topic
        An optional keyword argument can be passed to the function, which will be automatically selected in the documentation

        See `:doc doc` for details.
    } %{
    evaluate-commands %sh{
        topic="doc"
        if [ $# -ge 1 ]; then
            topic="$1"
        fi
        page=$(
            find -L \
                "${kak_config}/autoload/" \
                "${kak_runtime}/doc/" \
                "${kak_runtime}/rc/" \
                -type f -name "$topic.asciidoc" |
                head -1
        )
        if [ -f "${page}" ]; then
            printf 'evaluate-commands -try-client %%opt{docsclient} %%{
                edit! -scratch "*doc-%s*"
                doc-render "%s"
            }' "$(basename "$1" .asciidoc)" "${page}"
            if [ $# -eq 2 ]; then
                printf "doc-jump-to-anchor '%s'" "$2"
            fi
        else
            printf 'fail No such doc file: %s\n' "$topic.asciidoc"
        fi
    }
}

alias global help doc

define-command -params 1.. -docstring %{
        doc-search <pattern> [topic...]: search for a pattern in the documentation

        Display a menu listing all the search matches in the given topics, or the whole documentation if unspecified (the number of search results is effected by the `doc_max_search_results` option)
    } \
    -shell-script-candidates %{
        case "${kak_token_to_complete}" in
            0) ;;
            *) ls -1 -- "${kak_runtime}"/doc/*.asciidoc | awk '{gsub("^.+/|\\..+$", ""); print}';;
        esac
    } doc-search %{
    set-option global doc_search_matches

    evaluate-commands %sh{
        readonly SEARCH_PATTERN="${1}"

        shift
        if [ $# -eq 0 ]; then
            set -- $(ls -1 -- "${kak_runtime}"/doc/*.asciidoc | awk '{gsub("^.+/|\\..+$", ""); print}')
        else
            for topic; do
                if [ ! -e "${kak_runtime}/doc/${topic}.asciidoc" ]; then
                    printf 'fail No such topic %%{%s}' "${topic}"
                    exit 1
                fi
            done
        fi

        for topic; do
            SEARCH_PATTERN_ESC=$(printf %s "${SEARCH_PATTERN}" | sed "s/'/&&/g")
            printf "doc-search-impl '%s' '%s'\n" "${topic}" "${SEARCH_PATTERN_ESC}"
        done
    }

    evaluate-commands %sh{
        kakquote() { printf "%s" "$*" | sed "s/'/''/g; 1s/^/'/; \$s/\$/'/"; }

        eval "set -- ${kak_quoted_opt_doc_search_matches}"

        while [ $# -gt 0 ]; do
            candidates="${candidates} $(kakquote "{MenuInfo}${1}{default}{\\} ${3}") 'doc ${1}; select $2'"
            shift 3
        done

        if [ -n "${candidates}" ]; then
            printf '
                set global doc_search_matches
                menu -markup --%s
            ' "${candidates}"
        fi
    }
}

define-command -hidden -params 2 doc-search-impl %{
    edit -scratch

    try %{
        doc-render "%val{runtime}/doc/%arg{1}.asciidoc"

        execute-keys gg / "%arg{2}" <ret> %opt{doc_max_search_results} N <a-n> )

        evaluate-commands -draft -itersel %{
            # Topic and selection description of the search match
            set-option -add global doc_search_matches %arg{1} %val{selection_desc}

            # Search match context
            execute-keys <a-x>
            try %{
                execute-keys <a-K>^$<ret> H
            }
            set-option -add global doc_search_matches %val{selection}
        }
    }

    delete-buffer!
}
