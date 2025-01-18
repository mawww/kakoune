provide-module perf-report %{
    add-highlighter shared/perf-report group
    add-highlighter shared/perf-report/above_threshold regex '\b([5-9]|\d{2})\.\d+%' 0:red
    add-highlighter shared/perf-report/below_threshold regex '\b[0-4]\.\d+%' 0:green


    define-command -override perf-report-focus %{
        execute-keys 'xs...\d+\.\d+%<ret><a-:><a-semicolon>vtv<lt><semicolon>'
    }
}

hook -group perf-report-highlight global WinSetOption filetype=perf-report %{
    require-module perf-report
    add-highlighter window/perf-report ref perf-report
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/perf-report }

    map window normal <ret> ': perf-report-focus<ret>'
}

provide-module perf-annotate %{
    require-module gas
    add-highlighter shared/perf-annotate group
    add-highlighter shared/perf-annotate/gas ref gas
    add-highlighter shared/perf-annotate/above_threshold regex '^\h+([1-9]|\d{2})\.\d+\b' 0:red
    add-highlighter shared/perf-annotate/below_threshold regex '^\h+0\.\d+\b' 0:green
}

hook -group perf-annotate-highlight global WinSetOption filetype=perf-annotate %{
    require-module perf-annotate
    add-highlighter window/perf-annotate ref perf-annotate
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/perf-annotate }
}
