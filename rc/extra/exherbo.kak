## Repository metadata files
hook global BufCreate .*/metadata/mirrors\.conf         %{ set buffer filetype paludis-mirrors-conf }
hook global BufCreate .*/metadata/licence_groups.conf   %{ set buffer filetype exheres-0-licence-groups }
hook global BufCreate .*/metadata/options/descriptions/.*\.conf   %{ set buffer filetype exheres-0-licence-groups }
hook global BufCreate .*/metadata/.*\.conf              %{ set buffer filetype exheres-0-metadata }

## News items
hook global BufCreate .*/metadata/news/.*/.*\.txt %{ set buffer filetype glep42 }

## exheres-0, exlib
hook global BufCreate .*\.(exheres-0|exlib) %{ set buffer filetype sh }

# Paludis configurations
hook global BufCreate .*/etc/paludis(-.*)?/bashrc                               %{ set buffer filetype sh }
hook global BufCreate .*/etc/paludis(-.*)?/general(\.conf.d/.*.conf|\.conf)     %{ set buffer filetype paludis-key-value-conf }
hook global BufCreate .*/etc/paludis(-.*)?/licences(\.conf.d/.*.conf|\.conf)    %{ set buffer filetype paludis-options-conf }
hook global BufCreate .*/etc/paludis(-.*)?/mirrors(\.conf.d/.*.conf|\.conf)     %{ set buffer filetype paludis-mirrors-conf }
hook global BufCreate .*/etc/paludis(-.*)?/options(\.conf.d/.*.conf|\.conf)     %{ set buffer filetype paludis-options-conf }
hook global BufCreate .*/etc/paludis(-.*)?/output(\.conf.d/.*.conf|\.conf)      %{ set buffer filetype paludis-key-value-conf }
hook global BufCreate .*/etc/paludis(-.*)?/package_(unmask|mask)(\.conf.d/.*.conf|\.conf)     %{ set buffer filetype paludis-specs-conf }
hook global BufCreate .*/etc/paludis(-.*)?/platforms(\.conf.d/.*.conf|\.conf)   %{ set buffer filetype paludis-specs-conf }
hook global BufCreate .*/etc/paludis(-.*)?/repositories/.*\.conf                %{ set buffer filetype paludis-key-value-conf }
hook global BufCreate .*/etc/paludis(-.*)?/repository\.template                 %{ set buffer filetype paludis-key-value-conf }
hook global BufCreate .*/etc/paludis(-.*)?/repository_defaults\.conf            %{ set buffer filetype paludis-key-value-conf }
hook global BufCreate .*/etc/paludis(-.*)?/specpath\.conf                       %{ set buffer filetype paludis-key-value-conf }
hook global BufCreate .*/etc/paludis(-.*)?/suggestions(\.conf.d/.*.conf|\.conf) %{ set buffer filetype paludis-specs-conf }

# Highlighters
## exheres-0 Repository metadata files
addhl -group / group exheres-0-metadata
addhl -group /exheres-0-metadata regex ^#.*?$ 0:comment
addhl -group /exheres-0-metadata regex ^(?:[\s\t]+)?(\*)?(\S+)(?:[\s\t]+)?=(?:[\s\t]+)?(.+?)?$ 1:type 2:attribute 3:string
addhl -group /exheres-0-metadata regex ^(?:[\s\t]+)?[\S]+[\s\t]+=[\s\t]+\[.+?[\s\t]+\] 0:string
addhl -group /exheres-0-metadata regex ^(?:[\s\t]+)?(\S+)\s\[\[$ 0:type
addhl -group /exheres-0-metadata regex ^(?:[\s\t]+)?\]\]$ 0:type

hook global WinSetOption filetype=exheres-0-metadata %{ addhl ref exheres-0-metadata }
hook global WinSetOption filetype=(?!exheres-0-metadata).* %{ rmhl exheres-0-metadata }

## exheres-0 options descriptions
addhl -group / group exheres-0-options-descriptions
addhl -group /exheres-0-options-descriptions regex ^#.*?$ 0:comment
addhl -group /exheres-0-options-descriptions regex ^(?:[\s\t]+)?[\S]+[\s\t]+-[\s\t]+\[.+?[\s\t]+\] 0:string
addhl -group /exheres-0-options-descriptions regex ^(?:[\s\t]+)?(\S+)\s\[\[$ 0:type
addhl -group /exheres-0-options-descriptions regex ^(?:[\s\t]+)?\]\]$ 0:type

hook global WinSetOption filetype=exheres-0-options-descriptions %{ addhl ref exheres-0-options-descriptions }
hook global WinSetOption filetype=(?!exheres-0-options-descriptions).* %{ rmhl exheres-0-options-descriptions }

## metadata/licence_groups.conf
addhl -group / group exheres-0-licence-groups
addhl -group /exheres-0-licence-groups regex [\s\t]+(\S+(?:[\s\t]+))*$ 0:attribute
addhl -group /exheres-0-licence-groups regex ^(\S+) 0:type
addhl -group /exheres-0-licence-groups regex ^#.*?$ 0:comment

hook global WinSetOption filetype=exheres-0-licence-groups %{ addhl ref exheres-0-licence-groups }
hook global WinSetOption filetype=(?!exheres-0-licence-groups).* %{ rmhl exheres-0-licence-groups }

## Paludis configurations
### options.conf
addhl -group / group paludis-options-conf
addhl -group /paludis-options-conf regex [\s\t]+(\S+(?:[\s\t]+))*$ 0:attribute
addhl -group /paludis-options-conf regex (?::)(?:[\s\t]+)(.*?$) 1:attribute
addhl -group /paludis-options-conf regex [\s\t]+(\S+=)(\S+) 1:attribute 2:value
addhl -group /paludis-options-conf regex [\s\t](\S+:) 0:keyword
addhl -group /paludis-options-conf regex [\s\t](-\S+)(.*?) 1:red
addhl -group /paludis-options-conf regex ^(\S+/\S+) 0:type
addhl -group /paludis-options-conf regex ^#.*?$ 0:comment

hook global WinSetOption filetype=paludis-options-conf %{ addhl ref paludis-options-conf }
hook global WinSetOption filetype=(?!paludis-options-conf).* %{ rmhl paludis-options-conf }

## general.conf, repository.template
addhl -group / group paludis-key-value-conf
addhl -group /paludis-key-value-conf regex ^[\s\t]?(\S+)[\s\t+]=[\s\t+](.*?)$ 1:attribute 2:value
addhl -group /paludis-key-value-conf regex ^#.*?$ 0:comment

hook global WinSetOption filetype=paludis-key-value-conf %{ addhl ref paludis-key-value-conf }
hook global WinSetOption filetype=(?!paludis-key-value-conf).* %{ rmhl paludis-key-value-conf }

## mirrors.conf
addhl -group / group paludis-mirrors-conf
addhl -group /paludis-mirrors-conf regex ^[\s\t+]?(\S+)[\s\t+](.*?)$ 1:type 2:value
addhl -group /paludis-mirrors-conf regex ^#.*?$ 0:comment

hook global WinSetOption filetype=paludis-mirrors-conf %{ addhl ref paludis-mirrors-conf }
hook global WinSetOption filetype=(?!paludis-mirrors-conf).* %{ rmhl paludis-mirrors-conf }

## package_(unmask|mask).conf, platforms.conf
addhl -group / group paludis-specs-conf
addhl -group /paludis-specs-conf regex [\s\t]+(\S+(?:[\s\t]+))*$ 0:attribute
addhl -group /paludis-specs-conf regex ^(\S+/\S+) 0:type
addhl -group /paludis-specs-conf regex ^#.*?$ 0:comment

hook global WinSetOption filetype=paludis-specs-conf %{ addhl ref paludis-specs-conf }
hook global WinSetOption filetype=(?!paludis-specs-conf).* %{ rmhl paludis-specs-conf }

## News items (GLEP42)
addhl -group / group glep42
addhl -group /glep42 regex ^(Title|Author|Translator|Content-Type|Posted|Revision|News-Item-Format|Display-If-Installed|Display-If-Keyword|Display-If-Profile):([^\n]*(?:\n\h+[^\n]+)*)$ 1:keyword 2:attribute
addhl -group /glep42 regex <[^@>]+@.*?> 0:string
addhl -group /glep42 regex ^>.*?$ 0:comment

hook global WinSetOption filetype=glep42 %{ addhl ref glep42 }
hook global WinSetOption filetype=(?!glep42).* %{ rmhl glep42 }
