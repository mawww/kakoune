declare-option -docstring "name of the client in which documentation is to be displayed" \
    str docsclient

declare-option -hidden range-specs doc_render_ranges

define-command -hidden -params 4 doc-render-regex %{
    eval -draft %{ try %{
        exec \%s %arg{1} <ret>
        exec -draft s %arg{2} <ret> d
        exec "%arg{3}"
        %sh{
            ranges=$(echo "$kak_selections_desc" | sed -e "s/:/|$4:/g; s/\$/|$4/")
            echo "update-option buffer doc_render_ranges"
            echo "set-option -add buffer doc_render_ranges '$ranges'"
        }
    } }
}

define-command -params 1 -hidden doc-render %{
    edit! -scratch *doc*
    exec "!cat %arg{1}<ret>gg"

    # Join paragraphs together
    try %{ exec -draft \%S \n{2,}|(?<=\+)\n|^[^\n]+::\n <ret> <a-K>^-{2,}(\n|\z)<ret> S\n\z<ret> <a-k>\n<ret> <a-j> }

    # Remove some line end markers
    try %{ exec -draft \%s \h*(\+|:{2,})$ <ret> d }

    # Setup the doc_render_ranges option
    set-option buffer doc_render_ranges %val{timestamp}
    doc-render-regex \B(?<!\\)\*[^\n]+?(?<!\\)\*\B \A|.\z 'H' default+b
    doc-render-regex \b(?<!\\)_[^\n]+?(?<!\\)_\b \A|.\z 'H' default+i
    doc-render-regex \B(?<!\\)`[^\n]+?(?<!\\)`\B \A|.\z 'H' mono
    doc-render-regex ^=\h+[^\n]+ ^=\h+ '~' title
    doc-render-regex ^={2,}\h+[^\n]+ ^={2,}\h+ '' header
    doc-render-regex ^-{2,}\n.*?^-{2,}\n ^-{2,}\n '' block
    doc-render-regex <lt><lt>.*?<gt><gt> <lt><lt>.*,|<gt><gt> 'H' link

    # Remove escaping of * and `
    try %{ exec -draft \%s \\((?=\*)|(?=`)) <ret> d }

    set-option buffer readonly true
    add-highlighter buffer ranges doc_render_ranges
    add-highlighter buffer wrap -word -indent
}

define-command -params 1 \
    -shell-candidates %{
        find "${kak_runtime}/doc/" -type f -name "*.asciidoc" | while read l; do
            basename "${l%.*}"
        done
    } \
    doc -docstring %{doc <topic> [<keyword>]: open a buffer containing documentation about a given topic
An optional keyword argument can be passed to the function, which will be automatically selected in the documentation} %{
    %sh{
        readonly page="${kak_runtime}/doc/${1}.asciidoc"

        shift
        if [ -f "${page}" ]; then
            printf %s\\n "eval -try-client %opt{docsclient} doc-render ${page}"
        else
            printf %s\\n "echo -markup '{Error}No such doc file: ${page}'"
        fi
    }
}

alias global help doc
