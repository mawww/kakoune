decl str jumpclient

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global WinCreate [*]debug[*] %{
    set buffer filetype debug
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / multi_region -default code debug \
    comment \Q*** \Q*** ''

addhl -group /debug/comment fill comment

addhl -group /debug/code regex ^(pid|session) 0:keyword
addhl -group /debug/code regex ^([^:]+):(\d+):(\d+):([^\n]+) 1:default 2:value 3:value 4:error

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _debug_jump %{
    exec xs^([^:]+):(\d+):(\d+)<ret>
    eval -try-client %opt(jumpclient) edit %reg(1) %reg(2) %reg(3)
    try %(focus %opt(jumpclient))
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=debug %{
    addhl ref debug

    map window normal <ret> :_debug_jump<ret>
}

hook global WinSetOption filetype=(?!debug).* %{
    rmhl debug
}
