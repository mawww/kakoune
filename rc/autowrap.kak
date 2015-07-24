decl int autowrap_column 80

def autowrap-enable %{
    hook -group autowrap window InsertChar [^\n] %{ try %{ exec -draft "<a-h><a-k>.{%opt{autowrap_column},}<ret><a-;>bi<ret><esc>xX<a-k>[^\n]+\n[^\n]<ret>kx;d" } }
}

def autowrap-disable %{
    rmhooks window autowrap
}
