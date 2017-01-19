decl str tutorial_source %val{source}
decl str tutorial_dir
decl str tutorial_lesson "000-introduction.md"

def tutorial -params .. -docstring %{ Run the kakoune tutorial } -allow-override %{
    %sh{
        [ $# -gt 0 ] && tutlang="$1" || tutlang="en"
        echo "set global tutorial_dir $(dirname ${kak_opt_tutorial_source})/$tutlang"
        echo tutorial-load
    }
}

def -hidden tutorial-load -params 0..1 -allow-override %{
    %sh{
        bufname="*${kak_opt_tutorial_lesson}*"
        if [ $(echo "${kak_buflist}" | grep "${bufname}") ]; then
            echo "buffer ${bufname}"
        else
            echo "edit -scratch ${bufname}"
            echo "tutorial-reload $@"
        fi
    }
}

def tutorial-reload -params 0..1 -docstring %{ Reload current tutorial page } %{
    %sh{
        nav=true
        if [ "$1" = "-no-nav" ]; then
            nav=false
        fi
        echo "exec -draft -client ${kak_client} \%d"
        if $nav ; then
            echo "exec -draft -client ${kak_client} |cat<space>${kak_opt_tutorial_dir}/meta-nav.md<ret>\;"
        fi
        echo "exec -draft -client ${kak_client} ge|cat<space>${kak_opt_tutorial_dir}/${kak_opt_tutorial_lesson}<ret>\;"
        echo "exec -client ${kak_client} gg"
        # echo "exec -draft -client ${kak_client} ggO\`<lt>esc<gt>,h\`<space>previous<space>page<esc> Qa<space><esc><esc>35q anext<space>page<space>\`<lt>esc<gt>,l\`<esc> o<esc>"
        echo "set buffer filetype markdown"
        echo "map buffer user l :tutorial-next<ret>"
        echo "map buffer user h :tutorial-prev<ret>"
        echo "map buffer user r :tutorial-reload<ret>"
    }
}

def tutorial-next -allow-override %{
    %sh{
        next=$(ls "${kak_opt_tutorial_dir}" | grep -v "meta-" | grep -A 1 "${kak_opt_tutorial_lesson}" | tail -1)
        echo "set global tutorial_lesson $next"
    }
    tutorial-load
}

def tutorial-prev -allow-override %{
    %sh{
        next=$(ls "${kak_opt_tutorial_dir}" | grep -v "meta-" | grep -B 1 "${kak_opt_tutorial_lesson}" | head -1)
        echo "set global tutorial_lesson $next"
    }
    tutorial-load
}

def help -allow-override -docstring %{ Show help... or not } %{
    set global tutorial_lesson "meta-help.md"
    tutorial
    tutorial-reload -no-nav
    set global tutorial_lesson "000-introduction.md"
}
