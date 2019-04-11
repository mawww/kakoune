## Repository metadata files
hook global BufCreate .*/metadata/mirrors\.conf         %{ set-option buffer filetype paludis-mirrors-conf }
hook global BufCreate .*/metadata/licence_groups.conf   %{ set-option buffer filetype exheres-0-licence-groups }
hook global BufCreate .*/metadata/options/descriptions/.*\.conf   %{ set-option buffer filetype exheres-0-licence-groups }
hook global BufCreate .*/metadata/.*\.conf              %{ set-option buffer filetype exheres-0-metadata }

## News items
hook global BufCreate .*/metadata/news/.*/.*\.txt %{ set-option buffer filetype glep42 }

## exheres-0, exlib
hook global BufCreate .*\.(exheres-0|exlib) %{ set-option buffer filetype sh }

# Paludis configurations
hook global BufCreate .*/etc/paludis(-.*)?/bashrc                               %{ set-option buffer filetype sh }
hook global BufCreate .*/etc/paludis(-.*)?/general(\.conf.d/.*.conf|\.conf)     %{ set-option buffer filetype paludis-key-value-conf }
hook global BufCreate .*/etc/paludis(-.*)?/licences(\.conf.d/.*.conf|\.conf)    %{ set-option buffer filetype paludis-options-conf }
hook global BufCreate .*/etc/paludis(-.*)?/mirrors(\.conf.d/.*.conf|\.conf)     %{ set-option buffer filetype paludis-mirrors-conf }
hook global BufCreate .*/etc/paludis(-.*)?/options(\.conf.d/.*.conf|\.conf)     %{ set-option buffer filetype paludis-options-conf }
hook global BufCreate .*/etc/paludis(-.*)?/output(\.conf.d/.*.conf|\.conf)      %{ set-option buffer filetype paludis-key-value-conf }
hook global BufCreate .*/etc/paludis(-.*)?/package_(unmask|mask)(\.conf.d/.*.conf|\.conf)     %{ set-option buffer filetype paludis-specs-conf }
hook global BufCreate .*/etc/paludis(-.*)?/platforms(\.conf.d/.*.conf|\.conf)   %{ set-option buffer filetype paludis-specs-conf }
hook global BufCreate .*/etc/paludis(-.*)?/repositories/.*\.conf                %{ set-option buffer filetype paludis-key-value-conf }
hook global BufCreate .*/etc/paludis(-.*)?/repository\.template                 %{ set-option buffer filetype paludis-key-value-conf }
hook global BufCreate .*/etc/paludis(-.*)?/repository_defaults\.conf            %{ set-option buffer filetype paludis-key-value-conf }
hook global BufCreate .*/etc/paludis(-.*)?/specpath\.conf                       %{ set-option buffer filetype paludis-key-value-conf }
hook global BufCreate .*/etc/paludis(-.*)?/suggestions(\.conf.d/.*.conf|\.conf) %{ set-option buffer filetype paludis-specs-conf }

hook global WinSetOption filetype=exheres-0-(metadata|options-descriptions|licence-groups) %{
    require-module exheres
}

hook -group exheres-0-metadata-highlight global WinSetOption filetype=exheres-0-metadata %{
    add-highlighter window/exheres-0-metadata ref exheres-0-metadata
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/exheres-0-metadata }
}

hook -group exheres-0-options-descriptions-highlight global WinSetOption filetype=exheres-0-options-descriptions %{
    add-highlighter window/exheres-0-options-descriptions ref exheres-0-options-descriptions
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/exheres-0-options-descriptions }
}

hook -group exheres-0-licence-groups-highlight global WinSetOption filetype=exheres-0-licence-groups %{
    add-highlighter window/exheres-0-licence-groups ref exheres-0-licence-groups
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/exheres-0-licence-groups }
}

provide-module exheres %{
# Highlighters
## exheres-0 Repository metadata files
add-highlighter shared/exheres-0-metadata group
add-highlighter shared/exheres-0-metadata/ regex ^#.*?$ 0:comment
add-highlighter shared/exheres-0-metadata/ regex ^(?:[\s\t]+)?(\*)?(\S+)(?:[\s\t]+)?=(?:[\s\t]+)?(.+?)?$ 1:type 2:attribute 3:string
add-highlighter shared/exheres-0-metadata/ regex ^(?:[\s\t]+)?[\S]+[\s\t]+=[\s\t]+\[.+?[\s\t]+\] 0:string
add-highlighter shared/exheres-0-metadata/ regex ^(?:[\s\t]+)?(\S+)\s\[\[$ 0:type
add-highlighter shared/exheres-0-metadata/ regex ^(?:[\s\t]+)?\]\]$ 0:type

## exheres-0 options descriptions
add-highlighter shared/exheres-0-options-descriptions group
add-highlighter shared/exheres-0-options-descriptions/ regex ^#.*?$ 0:comment
add-highlighter shared/exheres-0-options-descriptions/ regex ^(?:[\s\t]+)?[\S]+[\s\t]+-[\s\t]+\[.+?[\s\t]+\] 0:string
add-highlighter shared/exheres-0-options-descriptions/ regex ^(?:[\s\t]+)?(\S+)\s\[\[$ 0:type
add-highlighter shared/exheres-0-options-descriptions/ regex ^(?:[\s\t]+)?\]\]$ 0:type

## metadata/licence_groups.conf
add-highlighter shared/exheres-0-licence-groups group
add-highlighter shared/exheres-0-licence-groups/ regex [\s\t]+(\S+(?:[\s\t]+))*$ 0:attribute
add-highlighter shared/exheres-0-licence-groups/ regex ^(\S+) 0:type
add-highlighter shared/exheres-0-licence-groups/ regex ^#.*?$ 0:comment
}

hook global WinSetOption filetype=paludis-(key-value|options|mirrors|specs)-conf %{
    require-module paludis
}

hook -group paludis-options-conf-highlight global WinSetOption filetype=paludis-options-conf %{
    add-highlighter window/paludis-options-conf ref paludis-options-conf
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/paludis-options-conf }
}

hook -group paludis-key-value-conf-highlight global WinSetOption filetype=paludis-key-value-conf %{
    add-highlighter window/paludis-key-value-conf ref paludis-key-value-conf
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/paludis-key-value-conf }
}

hook -group paludis-mirrors-conf-highlight global WinSetOption filetype=paludis-mirrors-conf %{
    add-highlighter window/paludis-mirrors-conf ref paludis-mirrors-conf
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/paludis-mirrors-conf }
}

hook -group paludis-specs-conf-highlight global WinSetOption filetype=paludis-specs-conf %{
    add-highlighter window/paludis-specs-conf ref paludis-specs-conf
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/paludis-specs-conf }
}
provide-module paludis %{
## Paludis configurations
### options.conf
add-highlighter shared/paludis-options-conf group
add-highlighter shared/paludis-options-conf/ regex [\s\t]+(\S+(?:[\s\t]+))*$ 0:attribute
add-highlighter shared/paludis-options-conf/ regex (?::)(?:[\s\t]+)(.*?$) 1:attribute
add-highlighter shared/paludis-options-conf/ regex [\s\t]+(\S+=)(\S+) 1:attribute 2:value
add-highlighter shared/paludis-options-conf/ regex [\s\t](\S+:) 0:keyword
add-highlighter shared/paludis-options-conf/ regex [\s\t](-\S+)(.*?) 1:red
add-highlighter shared/paludis-options-conf/ regex ^(\S+/\S+) 0:type
add-highlighter shared/paludis-options-conf/ regex ^#.*?$ 0:comment

## general.conf, repository.template
add-highlighter shared/paludis-key-value-conf group
add-highlighter shared/paludis-key-value-conf/ regex ^[\s\t]?(\S+)[\s\t+]=[\s\t+](.*?)$ 1:attribute 2:value
add-highlighter shared/paludis-key-value-conf/ regex ^#.*?$ 0:comment

## mirrors.conf
add-highlighter shared/paludis-mirrors-conf group
add-highlighter shared/paludis-mirrors-conf/ regex ^[\s\t+]?(\S+)[\s\t+](.*?)$ 1:type 2:value
add-highlighter shared/paludis-mirrors-conf/ regex ^#.*?$ 0:comment

## package_(unmask|mask).conf, platforms.conf
add-highlighter shared/paludis-specs-conf group
add-highlighter shared/paludis-specs-conf/ regex [\s\t]+(\S+(?:[\s\t]+))*$ 0:attribute
add-highlighter shared/paludis-specs-conf/ regex ^(\S+/\S+) 0:type
add-highlighter shared/paludis-specs-conf/ regex ^#.*?$ 0:comment
}

hook global WinSetOption filetype=glep42 %{
    require-module glep42
}

hook -group glep42-highlight global WinSetOption filetype=glep42 %{
    add-highlighter window/glep42 ref glep42
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/glep42 }
}

provide-module glep42 %{
## News items (GLEP42)
add-highlighter shared/glep42 group
add-highlighter shared/glep42/ regex ^(Title|Author|Translator|Content-Type|Posted|Revision|News-Item-Format|Display-If-Installed|Display-If-Keyword|Display-If-Profile):([^\n]*(?:\n\h+[^\n]+)*)$ 1:keyword 2:attribute
add-highlighter shared/glep42/ regex <[^@>]+@.*?> 0:string
add-highlighter shared/glep42/ regex ^>.*?$ 0:comment
}
