# Highlighting for common files in /etc
hook global BufCreate .*/etc/(hosts|networks|services)  %{ set buffer filetype etc-hosts }
hook global BufCreate .*/etc/resolv.conf                %{ set buffer filetype etc-resolv-conf }
hook global BufCreate .*/etc/shadow                     %{ set buffer filetype etc-shadow }
hook global BufCreate .*/etc/passwd                     %{ set buffer filetype etc-passwd }
hook global BufCreate .*/etc/gshadow                    %{ set buffer filetype etc-gshadow }
hook global BufCreate .*/etc/group                      %{ set buffer filetype etc-group }
hook global BufCreate .*/etc/(fs|m)tab                  %{ set buffer filetype etc-fstab }
hook global BufCreate .*/etc/environment                %{ set buffer filetype sh }
hook global BufCreate .*/etc/env.d/.*                   %{ set buffer filetype sh }
hook global BufCreate .*/etc/profile(\.(csh|env))?      %{ set buffer filetype sh }
hook global BufCreate .*/etc/profile\.d/.*              %{ set buffer filetype sh }

# Highlighters
## /etc/resolv.conf
add-highlighter -group / group etc-resolv-conf
add-highlighter -group /etc-resolv-conf regex ^#.*?$ 0:comment
add-highlighter -group /etc-resolv-conf regex ^(nameserver|server|domain|sortlist|options)[\s\t]+(.*?)$ 1:type 2:attribute

hook -group etc-resolv-conf-highlight global WinSetOption filetype=etc-resolv-conf %{ add-highlighter ref etc-resolv-conf }
hook -group etc-resolv-conf-highlight global WinSetOption filetype=(?!etc-resolv-conf).* %{ remove-highlighter etc-resolv-conf }

## /etc/hosts
add-highlighter -group / group etc-hosts
add-highlighter -group /etc-hosts regex ^(.+?)[\s\t]+?(.*?)$ 1:type 2:attribute
add-highlighter -group /etc-hosts regex \#.*?$ 0:comment

hook -group etc-hosts-highlight global WinSetOption filetype=etc-hosts %{ add-highlighter ref etc-hosts }
hook -group etc-hosts-highlight global WinSetOption filetype=(?!etc-hosts).* %{ remove-highlighter etc-hosts }

## /etc/fstab
add-highlighter -group / group etc-fstab
add-highlighter -group /etc-fstab regex ^(\S{1,})\s+?(\S{1,})\s+?(\S{1,})\s+?(\S{1,})\s+?(\S{1,})\s+?(\S{1,})(?:\s+)?$ 1:keyword 2:value 3:type 4:string 5:attribute 6:attribute
add-highlighter -group /etc-fstab regex \#.*?$ 0:comment

hook -group etc-fstab-highlight global WinSetOption filetype=etc-fstab %{ add-highlighter ref etc-fstab }
hook -group etc-fstab-highlight global WinSetOption filetype=(?!etc-fstab).* %{ remove-highlighter etc-fstab }

## /etc/group
add-highlighter -group / group etc-group
add-highlighter -group /etc-group regex ^(\S+?):(\S+?)?:(\S+?)?:(\S+?)?$ 1:keyword 2:type 3:value 4:string

hook -group etc-group-highlight global WinSetOption filetype=etc-group %{ add-highlighter ref etc-group }
hook -group etc-group-highlight global WinSetOption filetype=(?!etc-group).* %{ remove-highlighter etc-group }

## /etc/gshadow
add-highlighter -group / group etc-gshadow
add-highlighter -group /etc-gshadow regex ^(\S+?):(\S+?)?:(\S+?)?:(\S+?)?$ 1:keyword 2:type 3:value 4:string

hook -group etc-gshadow-highlight global WinSetOption filetype=etc-gshadow %{ add-highlighter ref etc-gshadow }
hook -group etc-gshadow-highlight global WinSetOption filetype=(?!etc-gshadow).* %{ remove-highlighter etc-gshadow }

## /etc/shadow
add-highlighter -group / group etc-shadow
add-highlighter -group /etc-shadow regex ^(\S+?):(\S+?):([0-9]+?):([0-9]+?)?:([0-9]+?)?:([0-9]+?)?:([0-9]+?)?:([0-9]+?)?:(.*?)?$ 1:keyword 2:type 3:value 4:value 5:value 6:value 7:value 8:value

hook -group etc-shadow-highlight global WinSetOption filetype=etc-shadow %{ add-highlighter ref etc-shadow }
hook -group etc-shadow-highlight global WinSetOption filetype=(?!etc-shadow).* %{ remove-highlighter etc-shadow }

## /etc/passwd
add-highlighter -group / group etc-passwd
add-highlighter -group /etc-passwd regex ^(\S+?):(\S+?):([0-9]+?):([0-9]+?):(.*?)?:(.+?):(.+?)$ 1:keyword 2:type 3:value 4:value 5:string 6:attribute 7:attribute

hook -group etc-passwd-highlight global WinSetOption filetype=etc-passwd %{ add-highlighter ref etc-passwd }
hook -group etc-passwd-highlight global WinSetOption filetype=(?!etc-passwd).* %{ remove-highlighter etc-passwd }

