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
addhl -group / group etc-resolv-conf
addhl -group /etc-resolv-conf regex ^#.*?$ 0:comment
addhl -group /etc-resolv-conf regex ^(nameserver|server|domain|sortlist|options)[\s\t]+(.*?)$ 1:type 2:attribute

hook global WinSetOption filetype=etc-resolv-conf       %{ addhl ref etc-resolv-conf }
hook global WinSetOption filetype=(?!etc-resolv-conf).* %{ rmhl etc-resolv-conf }

## /etc/hosts
addhl -group / group etc-hosts
addhl -group /etc-hosts regex ^(.+?)[\s\t]+?(.*?)$ 1:type 2:attribute
addhl -group /etc-hosts regex \#.*?$ 0:comment

hook global WinSetOption filetype=etc-hosts             %{ addhl ref etc-hosts }
hook global WinSetOption filetype=(?!etc-hosts).*       %{ rmhl etc-hosts }

## /etc/fstab
addhl -group / group etc-fstab
addhl -group /etc-fstab regex \#.*?$ 0:comment
addhl -group /etc-fstab regex ^(\S+)\s+(\S+)\s+(\S+)\s+(?:(\S+,?)+)\s+([0-9])\s+([0-9]) 1:keyword 2:value 3:type 4:string 5:attribute 6:attribute

hook global WinSetOption filetype=etc-fstab             %{ addhl ref etc-fstab }
hook global WinSetOption filetype=(?!etc-fstab).*       %{ rmhl etc-fstab }

## /etc/group
addhl -group / group etc-group
addhl -group /etc-group regex ^(\S+?):(\S+?)?:(\S+?)?:(\S+?)?$ 1:keyword 2:type 3:value 4:string

hook global WinSetOption filetype=etc-group             %{ addhl ref etc-group }
hook global WinSetOption filetype=(?!etc-group).*       %{ rmhl etc-group }

## /etc/gshadow
addhl -group / group etc-gshadow
addhl -group /etc-gshadow regex ^(\S+?):(\S+?)?:(\S+?)?:(\S+?)?$ 1:keyword 2:type 3:value 4:string

hook global WinSetOption filetype=etc-gshadow           %{ addhl ref etc-gshadow }
hook global WinSetOption filetype=(?!etc-gshadow).*     %{ rmhl etc-gshadow }

## /etc/shadow
addhl -group / group etc-shadow
addhl -group /etc-shadow regex ^(\S+?):(\S+?):([0-9]+?):([0-9]+?)?:([0-9]+?)?:([0-9]+?)?:([0-9]+?)?:([0-9]+?)?:(.*?)?$ 1:keyword 2:type 3:value 4:value 5:value 6:value 7:value 8:value

hook global WinSetOption filetype=etc-shadow            %{ addhl ref etc-shadow }
hook global WinSetOption filetype=(?!etc-shadow).*      %{ rmhl etc-shadow }

## /etc/passwd
addhl -group / group etc-passwd
addhl -group /etc-passwd regex ^(\S+?):(\S+?):([0-9]+?):([0-9]+?):(.*?)?:(.+?):(.+?)$ 1:keyword 2:type 3:value 4:value 5:string 6:attribute 7:attribute

hook global WinSetOption filetype=etc-passwd            %{ addhl ref etc-passwd }
hook global WinSetOption filetype=(?!etc-passwd).*      %{ rmhl etc-passwd }

