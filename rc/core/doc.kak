declare-option -docstring "name of the client in which documentation is to be displayed" \
    str docsclient

declare-option -hidden range-specs doc_ranges
declare-option -hidden range-specs doc_links

define-command -hidden -params 4 doc-render-regex %{
    evaluate-commands -draft %{ try %{
        execute-keys \%s %arg{1} <ret>
        execute-keys -draft s %arg{2} <ret> d
        execute-keys "%arg{3}"
        %sh{
            ranges=$(echo "$kak_selections_desc" | sed -e "s/:/|$4:/g; s/\$/|$4/")
            echo "update-option buffer doc_ranges"
            echo "set-option -add buffer doc_ranges '$ranges'"
        }
    } }
}

define-command -hidden doc-parse-links %{
    evaluate-commands -draft %{ try %{
        execute-keys \%s <lt><lt>(.*?)#,.*?<gt><gt> <ret>
        execute-keys -draft s <lt><lt>.*,|<gt><gt> <ret> d
        execute-keys H
        set-option buffer doc_links %val{timestamp}
        evaluate-commands -itersel %{
            set-option -add buffer doc_links "%val{selection_desc}|%reg{1}"
        }
    } }
}

define-command doc-jump %{
    update-option buffer doc_links
    %sh{
        printf "%s" "$kak_opt_doc_links" | awk -v RS=':' -v FS='[.,|]' '
            BEGIN {
                l=ENVIRON["kak_cursor_line"];
                c=ENVIRON["kak_cursor_column"];
            }
            l >= $1 && c >= $2 && l <= $3 && c <= $4 {
                print "doc " $5
                exit
            }
        '
    }
}

define-command -params 1 -hidden doc-render %{
    edit! -scratch *doc*
    execute-keys "!cat %arg{1}<ret>gg"

    # Join paragraphs together
    try %{ execute-keys -draft \%S \n{2,}|(?<=\+)\n|^[^\n]+::\n <ret> <a-K>^-{2,}(\n|\z)<ret> S\n\z<ret> <a-k>\n<ret> <a-j> }

    # Remove some line end markers
    try %{ execute-keys -draft \%s \h*(\+|:{2,})$ <ret> d }

    # Setup the doc_ranges option
    set-option buffer doc_ranges %val{timestamp}
    doc-render-regex \B(?<!\\)\*[^\n]+?(?<!\\)\*\B \A|.\z 'H' default+b
    doc-render-regex \b(?<!\\)_[^\n]+?(?<!\\)_\b \A|.\z 'H' default+i
    doc-render-regex \B(?<!\\)`[^\n]+?(?<!\\)`\B \A|.\z 'H' mono
    doc-render-regex ^=\h+[^\n]+ ^=\h+ '~' title
    doc-render-regex ^={2,}\h+[^\n]+ ^={2,}\h+ '' header
    doc-render-regex ^-{2,}\n.*?^-{2,}\n ^-{2,}\n '' block
    doc-parse-links

    # Remove escaping of * and `
    try %{ execute-keys -draft \%s \\((?=\*)|(?=`)) <ret> d }

    set-option buffer readonly true
    add-highlighter buffer ranges doc_ranges
    add-highlighter buffer wrap -word -indent
    map buffer normal <ret> :doc-jump<ret>
}

define-command -params 1 \
    -shell-candidates %{
        find "${kak_runtime}/doc/" -type f -name "*.asciidoc" | sed 's,.*/,,; s/\.[^/]*$//'
    } \
    doc -docstring %{doc <topic> [<keyword>]: open a buffer containing documentation about a given topic
An optional keyword argument can be passed to the function, which will be automatically selected in the documentation} %{
    %sh{
        readonly page="${kak_runtime}/doc/${1}.asciidoc"
        if [ -f "${page}" ]; then
            printf %s\\n "evaluate-commands -try-client %opt{docsclient} doc-render ${page}"
        else
            printf %s\\n "echo -markup '{Error}No such doc file: ${page}'"
        fi
    }
}

alias global help doc
