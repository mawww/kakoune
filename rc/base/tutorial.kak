decl str tutorial_source %val{source}
decl str tutorial_dir
decl str tutorial_lesson "000-introduction.asciidoc"

def tutorial -params .. -docstring %{ Run the kakoune tutorial } -allow-override %{
    %sh{
        [ $# -gt 0 ] && tutlang="$1" || tutlang="en"
        printf %s\\n "set global tutorial_dir $(dirname ${kak_opt_tutorial_source})/../../doc/tutorial/$tutlang"
        printf %s\\n tutorial-load
    }
}

def -hidden tutorial-load -params 0..1 -allow-override %{
    %sh{
        bufname="*${kak_opt_tutorial_lesson}*"
        if [ $(printf %s\\n "${kak_buflist}" | grep "${bufname}") ]; then
            printf %s\\n "buffer ${bufname}"
        else
            printf %s\\n "edit -scratch ${bufname}"
            printf %s\\n "tutorial-reload $@"
        fi
    }
}

def tutorial-reload -params 0..1 -docstring %{ Reload current tutorial page } %{
    %sh{
        nav=true
        if [ "$1" = "-no-nav" ]; then
            nav=false
        fi
        printf %s\\n "exec -draft -client ${kak_client} \%d"
        if $nav ; then
            printf %s\\n "exec -draft -client ${kak_client} |cat<space>${kak_opt_tutorial_dir}/meta-nav.asciidoc<ret>\;"
        fi
        printf %s\\n "exec -draft -client ${kak_client} ge|cat<space>${kak_opt_tutorial_dir}/${kak_opt_tutorial_lesson}<ret>\;"
        printf %s\\n "exec -client ${kak_client} gg"
        printf %s\\n "set buffer filetype markdown"
        printf %s\\n "map buffer user l :tutorial-next<ret>"
        printf %s\\n "map buffer user h :tutorial-prev<ret>"
        printf %s\\n "map buffer user r :tutorial-reload<ret>"
    }
}

def tutorial-next -allow-override %{
    %sh{
        next=$(ls -1 "${kak_opt_tutorial_dir}" | grep -v "meta-" | awk  "/${kak_opt_tutorial_lesson}/ {getline; print}")
        printf %s\\n  "set global tutorial_lesson $next"
    }
    tutorial-load
}

def tutorial-prev -allow-override %{
    %sh{
        next=$( ls -1 "${kak_opt_tutorial_dir}" | grep -v "meta-" | awk -v lesson="${kak_opt_tutorial_lesson}" '$0 == lesson { print last } { last = $0 }')
        printf %s\\n  "set global tutorial_lesson $next"
    }
    tutorial-load
}

def help -allow-override -docstring %{ Show help... or not } %{
    set global tutorial_lesson "meta-help.asciidoc"
    tutorial
    tutorial-reload -no-nav
    set global tutorial_lesson "000-introduction.asciidoc"
}
