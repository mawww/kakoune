declare-option str-to-str-map filetype_map
declare-option str-list       loaded_lang_files

hook global BufSetOption filetype=([^*]*) %{ evaluate-commands %sh{
    if [ -n "$kak_hook_param_capture_1" ]; then
        lang_file=$(printf '%s' "$kak_opt_filetype_map" | grep -o "'${kak_hook_param_capture_1}=\(.*\)'" | tr -d \' | cut -d '=' -f 2)
        if [ -n "$lang_file" -a -z "$(printf '%s' "$kak_opt_loaded_lang_files" | grep -o "$lang_file")" ]; then
            printf 'set-option -add global loaded_lang_files %s\n' "$lang_file"
            printf 'source %s/lang/%s\n' "$kak_runtime" "$lang_file"
        fi
    fi
} }

# c-family
set-option -add global filetype_map 'c=c-family.kak'
set-option -add global filetype_map 'cpp=c-family.kak'
set-option -add global filetype_map 'objc=c-family.kak'

hook global BufCreate .*\.(cc|cpp|cxx|C|hh|hpp|hxx|H)$ %{
    set-option buffer filetype cpp
}

hook global BufSetOption filetype=c\+\+ %{
    set-option buffer filetype cpp
}

hook global BufCreate .*\.c$ %{
    set-option buffer filetype c
}

hook global BufCreate .*\.h$ %{
    try %{
        execute-keys -draft %{%s\b::\b|\btemplate\h*<lt>|\bclass\h+\w+|\b(typename|namespace)\b|\b(public|private|protected)\h*:<ret>}
        set-option buffer filetype cpp
    } catch %{
        set-option buffer filetype c
    }
}

hook global BufCreate .*\.m %{
    set-option buffer filetype objc
}

# markdown
set-option -add global filetype_map 'markdown=markdown.kak'

hook global BufCreate .*[.](markdown|md|mkd) %{
    set-option buffer filetype markdown
}

# yaml
set-option -add global filetype_map 'yaml=yaml.kak'

hook global BufCreate .*[.](ya?ml) %{
    set-option buffer filetype yaml
}

# arch-linux
# package build description file
hook global BufCreate (.*/)?PKGBUILD %{
    set-option buffer filetype sh
}
