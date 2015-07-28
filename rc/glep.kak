hook global BufCreate .*/[0-9][0-9][0-9][0-9]-[0-3][0-9]-[0-9][0-9]-.*\.[a-z][a-z]\.txt %{
    set buffer filetype glep42
}

addhl -group / group glep42
addhl -group /glep42 regex ^(Title|Author|Translator|Content-Type|Posted|Revision|News-Item-Format|Display-If-Installed|Display-If-Keyword|Display-If-Profile):([^\n]*(?:\n\h+[^\n]+)*)$ 1:keyword 2:attribute
addhl -group /glep42 regex <[^@>]+@.*?> 0:string
addhl -group /glep42 regex ^>.*?$ 0:comment

hook global WinSetOption filetype=glep42 %{ addhl ref glep42 }
hook global WinSetOption filetype=(?!glep42).* %{ rmhl glep42 }
