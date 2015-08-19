## Repository metadata files
hook global BufCreate .*/metadata/.*\.conf %{
    set buffer filetype exheres-0-metadata
}

## News items
hook global BufCreate .*/metadata/news/.*/.*\.txt %{
    set buffer filetype glep42
}

## exheres-0, exlib
hook global BufCreate .*\.(exheres-0|exlib) %{
    set buffer filetype sh
}

# Paludis configurations
hook global BufCreate .*paludis/.*\.conf %{
    set buffer filetype paludis-conf
}

# Highlighters
## exheres-0 Repository metadata files
addhl -group / group exheres-0-metadata
addhl -group /exheres-0-metadata regex ^#.*?$ 0:comment
addhl -group /exheres-0-metadata regex ^(?:[\s\t]+)?(\*)?(summary|description|homepage|owner|status|preferred_(?:gid|uid)|gecos|shell|(?:extra|primary)_groups|masters|author|date|token|description)(?:[\s\t]+)?=(?:[\s\t]+)?(.+?)?$ 1:type 2:attribute 3:string
addhl -group /exheres-0-metadata regex ^(?:[\s\t]+)?[\S]+[\s\t]+=[\s\t]+\[.+?[\s\t]+\] 0:string
addhl -group /exheres-0-metadata regex ^(?:[\s\t]+)?(\S+)\s\[\[$ 0:type
addhl -group /exheres-0-metadata regex ^(?:[\s\t]+)?\]\]$ 0:type

hook global WinSetOption filetype=exheres-0-metadata %{ addhl ref exheres-0-metadata }
hook global WinSetOption filetype=(?!exheres-0-metadata).* %{ rmhl exheres-0-metadata }

## Paludis configurations
addhl -group / group paludis-conf
addhl -group /paludis-conf regex [\s\t]+(\S+(?:[\s\t]+))*$ 0:attribute
addhl -group /paludis-conf regex (?::)(?:[\s\t]+)(.*?$) 1:attribute
addhl -group /paludis-conf regex [\s\t]+(\S+\=)(.+?[\s\t]) 1:attribute 2:value
addhl -group /paludis-conf regex [\s\t](\S+:) 0:keyword
addhl -group /paludis-conf regex [\s\t](-\S+)(.*?) 1:red
addhl -group /paludis-conf regex ^(\S+/\S+) 0:type
addhl -group /paludis-conf regex ^#.*?$ 0:comment

hook global WinSetOption filetype=paludis-conf %{ addhl ref paludis-conf }
hook global WinSetOption filetype=(?!paludis-conf).* %{ rmhl paludis-conf }

## News items (GLEP42)
addhl -group / group glep42
addhl -group /glep42 regex ^(Title|Author|Translator|Content-Type|Posted|Revision|News-Item-Format|Display-If-Installed|Display-If-Keyword|Display-If-Profile):([^\n]*(?:\n\h+[^\n]+)*)$ 1:keyword 2:attribute
addhl -group /glep42 regex <[^@>]+@.*?> 0:string
addhl -group /glep42 regex ^>.*?$ 0:comment

hook global WinSetOption filetype=glep42 %{ addhl ref glep42 }
hook global WinSetOption filetype=(?!glep42).* %{ rmhl glep42 }
