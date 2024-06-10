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
