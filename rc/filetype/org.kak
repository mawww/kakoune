# https://orgmode.org/ - your life in plain text
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾


# Faces
# ‾‾‾‾‾

set-face global org_todo     red+b
set-face global org_done     green+b
set-face global org_priority value

# Options
# ‾‾‾‾‾‾‾

# Org mode supports different priorities and those can be customized either by
# using `#+TODO' and `#+PRIORITIES:' or by settings. These are default values
# that will be used in `dynregex' highlighters. We also need to update these
# items when opening file.
declare-option -docstring "Org Mode todo markers. You can customize this option directly, following the format, or by using `#+TODO:' in your document:
    document format: #+TODO: state1 state2 ... stateN done
    manual format:   '(state1|state2|...|stateN)|(done)'
Colors for TODO items can be customized with `org_todo' and `org_done' faces.
" \
regex org_todo "(TODO)|(DONE)"

declare-option -docstring "Org Mode priorities. You can customize this option directly, or by using `#+PRIORITIES:' in your document. Please make sure that the highest priority is earlier in the alphabet than the lowest priority:
    document format: #+PRIORITIES: A C B
    manual format:   'A|C|B'
Colors for priority items can be customized with `org_priority' face." \
regex org_priority "A|C|B"

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.]org %{
    set-option buffer filetype org
    # Update `org_todo_items' and `org_priority_items' when opening file
    evaluate-commands -save-regs '"/' %{
        try %{
            execute-keys -draft '/(?i)#\+TODO:[^\n]+<ret><a-h>f:l<a-l>y: set-option buffer org_todo_items %reg{dquote}<ret>'
            set-option buffer org_todo_items %sh{ printf "%s\n" " ${kak_opt_org_todo_items}" | sed -E "s/\s+/|/g;s/^\|//;s/(.*)\|(\w+)$/(\1)|(\2)/" }
        }
        try %{
            execute-keys -draft '/(?i)#\+PRIORITIES:[^\n]+<ret><a-h>f:l<a-l>s\h*\w(\s+)?(\w)?(\s+)?(\w)?\s<ret>y: set-option buffer org_priority_items %reg{dquote}<ret>'
            set-option buffer org_priority_items %sh{ printf "%s\n" "${kak_opt_org_priority_items}" | sed -E "s/ /|/g" }
        }
    }
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/org regions
add-highlighter shared/org/inline default-region regions
add-highlighter shared/org/inline/text default-region group

evaluate-commands %sh{
    languages="arch-linux asciidoc c cabal clojure
               cmake coffee cpp css cucumber d dart diff
               dockerfile elixir elm emacs-lisp etc exherbo
               fish gas git go haml haskell hbs html i3 ini
               java javascript json julia justrc kickstart
               latex lisp lua mail makefile markdown mercurial
               moon nim objc ocaml org perl php pony protobuf
               pug python ragel restructuredtext ruby rust sass
               scala scheme scss sh sql swift systemd taskpaper
               toml troff tupfile void-linux yaml"

    for lang in ${languages}; do
        printf "%s\n" "add-highlighter shared/org/${lang} region '#\+(?i)BEGIN_SRC(?I)\h+${lang}\b' '(?i)#\+END_SRC' regions"
        printf "%s\n" "add-highlighter shared/org/${lang}/ default-region fill meta"
        case ${lang} in
            # here we need to declare all workarounds for Org-mode supported languages
            kak)        ref="kakrc"   ;;
            emacs-lisp) ref="lisp"    ;;
            *)          ref="${lang}" ;;
        esac
        printf "%s\n" "add-highlighter shared/org/${lang}/inner region \A#\+(?i)BEGIN_SRC(?I)\h+[^\n]+\K '(?i)(?=#\+END_SRC)' ref ${ref}"
    done
}

# No-language SRC block
add-highlighter shared/org/src_block region ^(\h*)(?i)#\+BEGIN_SRC(\h+|\n) ^(\h*)(?i)#\+END_SRC\h*$ fill meta

# Comment
add-highlighter shared/org/comment region (^|\h)\K#[^+] $ fill comment

# Table
add-highlighter shared/org/table region ^\h*[|][^+] $ fill string

# Various blocks
evaluate-commands %sh{
    blocks="EXAMPLE QUOTE EXPORT CENTER VERSE"
    for block in ${blocks}; do
        printf "%s\n" "add-highlighter shared/org/${block} region '(?i)#\+BEGIN_${block}\b' '(?i)#\+END_${block}\b' regions"
        printf "%s\n" "add-highlighter shared/org/${block}/ default-region fill meta"
        printf "%s\n" "add-highlighter shared/org/${block}/inner region \A#\+(?i)BEGIN_${block}\b\K '(?i)(?=#\+END_${block})' fill mono"
    done
}


# Small example block
add-highlighter shared/org/inline/text/example regex ^(\h*)[:]\h+[^\n]* 0:meta

# Unordered list items start with `-', `+', or `*'
add-highlighter shared/org/inline/text/unordered-lists regex ^(?:\h*)([-+])\h+ 1:bullet

# But `*' list must be indented with at least single space, if not it is treated as a heading
add-highlighter shared/org/inline/text/star-list regex ^(?:\h+)([*])\h+ 1:bullet

# Ordered list items start with a numeral followed by either a period or a right parenthesis, such as `1.' or `1)'
add-highlighter shared/org/inline/text/ordered-lists regex ^(?:\h*)(\d+[.)])\h+ 1:bullet

# Headings. Also includes highlighting groups for TODO and PRIORITIES
# format: STARS TODO PRIORITY TEXT TAGS
add-highlighter shared/org/inline/text/heading       dynregex   '^(?:[*]{1}|[*]{5}|[*]{9})\h+(?:(?:%opt{org_todo})\h+)?(\[#(?:%opt{org_priority})\])?[^\n]*(:[^:\n]+?:)?\n' 0:header        1:org_todo 2:org_done 3:org_priority 4:module
add-highlighter shared/org/inline/text/section       dynregex  '^(?:[*]{2}|[*]{6}|[*]{10})\h+(?:(?:%opt{org_todo})\h+)?(\[#(?:%opt{org_priority})\])?[^\n]*(:[^:\n]+?:)?\n' 0:section       1:org_todo 2:org_done 3:org_priority 4:module
add-highlighter shared/org/inline/text/subsection    dynregex  '^(?:[*]{3}|[*]{7}|[*]{11})\h+(?:(?:%opt{org_todo})\h+)?(\[#(?:%opt{org_priority})\])?[^\n]*(:[^:\n]+?:)?\n' 0:subsection    1:org_todo 2:org_done 3:org_priority 4:module
add-highlighter shared/org/inline/text/subsubsection dynregex '^(?:[*]{4}|[*]{8}|[*]{12,})\h+(?:(?:%opt{org_todo})\h+)?(\[#(?:%opt{org_priority})\])?[^\n]*(:[^:\n]+?:)?\n' 0:subsubsection 1:org_todo 2:org_done 3:org_priority 4:module

# Options
add-highlighter shared/org/inline/text/option     regex "(?i)#\+[a-z]\w*\b[^\n]*"          0:module
add-highlighter shared/org/inline/text/title      regex "(?i)#\+title:([^\n]+)"            0:module 1:title
add-highlighter shared/org/inline/text/requisites regex "(?i)#\+(?:author|email):([^\n]+)" 0:module 1:keyword

# Timestamps
## YYYY-MM-DD DAYNAME
declare-option regex org_date '\d\d\d\d-\d\d-\d\d\h+[^\s-+>\]\d]+'
## H:MM
declare-option regex org_time '([0-2])?[0-9]:[0-5][0-9]'
## (.+|++) or (-|--) digit (hour|day|week|month|year)
declare-option regex org_repeater_or_delay '([.+][+]|[-]{1,2})\h+\d\h+[hdwmy]'
## DATE TIME REPEATER-OR-DELAY
declare-option regex org_timestamp "%opt{org_date}(\h+%opt{org_time}(-%opt{org_time})?(\h+%opt{org_repeater_or_delay})?)?"

add-highlighter shared/org/inline/text/timestamp_active   dynregex "<%opt{org_timestamp}(--%opt{org_timestamp})?>"   0:keyword
add-highlighter shared/org/inline/text/timestamp_inactive dynregex "\[%opt{org_timestamp}(--%opt{org_timestamp})?\]" 0:comment

# Markup
## FORMAT: PRE MARKER CONTENTS MARKER POST
## PRE: whitespace `(`, `{`, `'`, `"` or beginning of the line
## MARKER: `/`, `+`, `=`, `~`, `_`, or `$`
## CONTENTS FORMAT: BORDER BODY BORDER
## BORDER: any non-whitespace, and not `,` `'` or `"`
## BODY: any character, can't be longer than tho lines
## POST: a whitespace character, `-`, `.`, `,`, `:`, `!`, `?`, `'`, `)`, `}`
add-highlighter shared/org/inline/text/italic         regex "(^|[\h({'i""])([/][^\h,'""][^\n]*?\n?[^\n]*[^\h,'""][/])[\s.,:!?')}]" 2:italic
add-highlighter shared/org/inline/text/strikethrough  regex "(^|[\h({'i""])([+][^\h,'""][^\n]*?\n?[^\n]*[^\h,'""][+])[\s.,:!?')}]" 2:strikethrough
add-highlighter shared/org/inline/text/verbatim       regex "(^|[\h({'i""])([=][^\h,'""][^\n]*?\n?[^\n]*[^\h,'""][=])[\s.,:!?')}]" 2:meta
add-highlighter shared/org/inline/text/code           regex "(^|[\h({'i""])([~][^\h,'""][^\n]*?\n?[^\n]*[^\h,'""][~])[\s.,:!?')}]" 2:mono
add-highlighter shared/org/inline/text/inline-math    regex "(^|[\h({'i""])([$][^\h,'""][^\n]*?\n?[^\n]*[^\h,'""][$])[\s.,:!?')}]" 2:mono
add-highlighter shared/org/inline/text/underlined     regex "(^|[\h({'i""])([_][^\h,'""][^\n]*?\n?[^\n]*[^\h,'""][_])[\s.,:!?')}]" 2:underline

## bold is kinda tricky because we need to HL everything but headings, so it's split up on several regexps
add-highlighter shared/org/inline/text/bold regex "(?:^|[\h({'i""])([*][^\h,'""*][^\n]*?(\n{1})?[^\n]*?[*])\W|([*]{3,})\n|\h([*]{3})[\s.,:!?')}]" 1:bold

add-highlighter shared/org/inline/text/link     regex "(?:^|\h)([\[]{2}[^\n]*?[\]]{2})\W" 0:link
add-highlighter shared/org/inline/text/drawer   regex "^\h*([:][^\s][^\n]*?[^\s]*?[:])\W" 1:keyword

# LaTeX
add-highlighter shared/org/LaTeX region '\\begin\{[A-Za-z0-9*]+\}' '\\end\{[A-Za-z0-9*]+\}' fill string

# LaTeX Math
add-highlighter shared/org/math1 region '[$]{2}' '[$]{2}' fill mono
add-highlighter shared/org/math2 region '\\\['   '\\\]'   fill mono
add-highlighter shared/org/math3 region '\\\('   '\\\)'   fill mono

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden org-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # follow list item indentation and amount of leading whitespaces
        try %{ execute-keys -draft 'k<a-x>s\S<ret><space><a-?>^\h*([-+]|\h+[*]|\d[.)])<ret><a-x>s^\h*([+-]|\h+[*][^*]|\d[.)])\h*<ret>y/^\n<ret>PxHr<space>' }
        try %{ execute-keys -draft -itersel 'k<a-x>s\h+$<ret>d' }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group org-highlight global WinSetOption filetype=org %{
    add-highlighter window/org ref org
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/org }
}

hook global WinSetOption filetype=org %{
    hook window InsertChar \n -group org-indent org-indent-on-new-line
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window org-.+ }
}
