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
add-highlighter -group / group exheres-0-metadata
add-highlighter -group /exheres-0-metadata regex ^#.*?$ 0:comment
add-highlighter -group /exheres-0-metadata regex ^(?:[\s\t]+)?(\*)?(\S+)(?:[\s\t]+)?=(?:[\s\t]+)?(.+?)?$ 1:type 2:attribute 3:string
add-highlighter -group /exheres-0-metadata regex ^(?:[\s\t]+)?[\S]+[\s\t]+=[\s\t]+\[.+?[\s\t]+\] 0:string
add-highlighter -group /exheres-0-metadata regex ^(?:[\s\t]+)?(\S+)\s\[\[$ 0:type
add-highlighter -group /exheres-0-metadata regex ^(?:[\s\t]+)?\]\]$ 0:type

hook -group exheres-0-metadata-highlight global WinSetOption filetype=exheres-0-metadata %{ add-highlighter ref exheres-0-metadata }
hook -group exheres-0-metadata-highlight global WinSetOption filetype=(?!exheres-0-metadata).* %{ remove-highlighter exheres-0-metadata }

## exheres-0 options descriptions
add-highlighter -group / group exheres-0-options-descriptions
add-highlighter -group /exheres-0-options-descriptions regex ^#.*?$ 0:comment
add-highlighter -group /exheres-0-options-descriptions regex ^(?:[\s\t]+)?[\S]+[\s\t]+-[\s\t]+\[.+?[\s\t]+\] 0:string
add-highlighter -group /exheres-0-options-descriptions regex ^(?:[\s\t]+)?(\S+)\s\[\[$ 0:type
add-highlighter -group /exheres-0-options-descriptions regex ^(?:[\s\t]+)?\]\]$ 0:type

hook -group exheres-0-options-descriptions-highlight global WinSetOption filetype=exheres-0-options-descriptions %{ add-highlighter ref exheres-0-options-descriptions }
hook -group exheres-0-options-descriptions-highlight global WinSetOption filetype=(?!exheres-0-options-descriptions).* %{ remove-highlighter exheres-0-options-descriptions }

## metadata/licence_groups.conf
add-highlighter -group / group exheres-0-licence-groups
add-highlighter -group /exheres-0-licence-groups regex [\s\t]+(\S+(?:[\s\t]+))*$ 0:attribute
add-highlighter -group /exheres-0-licence-groups regex ^(\S+) 0:type
add-highlighter -group /exheres-0-licence-groups regex ^#.*?$ 0:comment

hook -group exheres-0-licence-groups-highlight global WinSetOption filetype=exheres-0-licence-groups %{ add-highlighter ref exheres-0-licence-groups }
hook -group exheres-0-licence-groups-highlight global WinSetOption filetype=(?!exheres-0-licence-groups).* %{ remove-highlighter exheres-0-licence-groups }

## Paludis configurations
### options.conf
add-highlighter -group / group paludis-options-conf
add-highlighter -group /paludis-options-conf regex [\s\t]+(\S+(?:[\s\t]+))*$ 0:attribute
add-highlighter -group /paludis-options-conf regex (?::)(?:[\s\t]+)(.*?$) 1:attribute
add-highlighter -group /paludis-options-conf regex [\s\t]+(\S+=)(\S+) 1:attribute 2:value
add-highlighter -group /paludis-options-conf regex [\s\t](\S+:) 0:keyword
add-highlighter -group /paludis-options-conf regex [\s\t](-\S+)(.*?) 1:red
add-highlighter -group /paludis-options-conf regex ^(\S+/\S+) 0:type
add-highlighter -group /paludis-options-conf regex ^#.*?$ 0:comment

hook -group paludis-options-conf-highlight global WinSetOption filetype=paludis-options-conf %{ add-highlighter ref paludis-options-conf }
hook -group paludis-options-conf-highlight global WinSetOption filetype=(?!paludis-options-conf).* %{ remove-highlighter paludis-options-conf }

## general.conf, repository.template
add-highlighter -group / group paludis-key-value-conf
add-highlighter -group /paludis-key-value-conf regex ^[\s\t]?(\S+)[\s\t+]=[\s\t+](.*?)$ 1:attribute 2:value
add-highlighter -group /paludis-key-value-conf regex ^#.*?$ 0:comment

hook -group paludis-key-value-conf-highlight global WinSetOption filetype=paludis-key-value-conf %{ add-highlighter ref paludis-key-value-conf }
hook -group paludis-key-value-conf-highlight global WinSetOption filetype=(?!paludis-key-value-conf).* %{ remove-highlighter paludis-key-value-conf }

## mirrors.conf
add-highlighter -group / group paludis-mirrors-conf
add-highlighter -group /paludis-mirrors-conf regex ^[\s\t+]?(\S+)[\s\t+](.*?)$ 1:type 2:value
add-highlighter -group /paludis-mirrors-conf regex ^#.*?$ 0:comment

hook -group paludis-mirrors-conf-highlight global WinSetOption filetype=paludis-mirrors-conf %{ add-highlighter ref paludis-mirrors-conf }
hook -group paludis-mirrors-conf-highlight global WinSetOption filetype=(?!paludis-mirrors-conf).* %{ remove-highlighter paludis-mirrors-conf }

## package_(unmask|mask).conf, platforms.conf
add-highlighter -group / group paludis-specs-conf
add-highlighter -group /paludis-specs-conf regex [\s\t]+(\S+(?:[\s\t]+))*$ 0:attribute
add-highlighter -group /paludis-specs-conf regex ^(\S+/\S+) 0:type
add-highlighter -group /paludis-specs-conf regex ^#.*?$ 0:comment

hook -group paludis-specs-conf-highlight global WinSetOption filetype=paludis-specs-conf %{ add-highlighter ref paludis-specs-conf }
hook -group paludis-specs-conf-highlight global WinSetOption filetype=(?!paludis-specs-conf).* %{ remove-highlighter paludis-specs-conf }

## News items (GLEP42)
add-highlighter -group / group glep42
add-highlighter -group /glep42 regex ^(Title|Author|Translator|Content-Type|Posted|Revision|News-Item-Format|Display-If-Installed|Display-If-Keyword|Display-If-Profile):([^\n]*(?:\n\h+[^\n]+)*)$ 1:keyword 2:attribute
add-highlighter -group /glep42 regex <[^@>]+@.*?> 0:string
add-highlighter -group /glep42 regex ^>.*?$ 0:comment

hook -group glep42-highlight global WinSetOption filetype=glep42 %{ add-highlighter ref glep42 }
hook -group glep42-highlight global WinSetOption filetype=(?!glep42).* %{ remove-highlighter glep42 }
