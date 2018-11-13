# Highlighting for common files in /etc

# Highlighters
## /etc/resolv.conf
add-highlighter shared/etc-resolv-conf group
add-highlighter shared/etc-resolv-conf/ regex ^#.*?$ 0:comment
add-highlighter shared/etc-resolv-conf/ regex ^(nameserver|server|domain|sortlist|options)[\s\t]+(.*?)$ 1:type 2:attribute

hook -group etc-resolv-conf-highlight global WinSetOption filetype=etc-resolv-conf %{ add-highlighter window/etc-resolv-conf ref etc-resolv-conf }
hook -group etc-resolv-conf-highlight global WinSetOption filetype=(?!etc-resolv-conf).* %{ remove-highlighter window/etc-resolv-conf }

## /etc/hosts
add-highlighter shared/etc-hosts group
add-highlighter shared/etc-hosts/ regex ^(.+?)[\s\t]+?(.*?)$ 1:type 2:attribute
add-highlighter shared/etc-hosts/ regex '#.*?$' 0:comment

hook -group etc-hosts-highlight global WinSetOption filetype=etc-hosts %{ add-highlighter window/etc-hosts ref etc-hosts }
hook -group etc-hosts-highlight global WinSetOption filetype=(?!etc-hosts).* %{ remove-highlighter window/etc-hosts }

## /etc/fstab
add-highlighter shared/etc-fstab group
add-highlighter shared/etc-fstab/ regex ^(\S{1,})\s+?(\S{1,})\s+?(\S{1,})\s+?(\S{1,})\s+?(\S{1,})\s+?(\S{1,})(?:\s+)?$ 1:keyword 2:value 3:type 4:string 5:attribute 6:attribute
add-highlighter shared/etc-fstab/ regex '#.*?$' 0:comment

hook -group etc-fstab-highlight global WinSetOption filetype=etc-fstab %{ add-highlighter window/etc-fstab ref etc-fstab }
hook -group etc-fstab-highlight global WinSetOption filetype=(?!etc-fstab).* %{ remove-highlighter window/etc-fstab }

## /etc/group
add-highlighter shared/etc-group group
add-highlighter shared/etc-group/ regex ^(\S+?):(\S+?)?:(\S+?)?:(\S+?)?$ 1:keyword 2:type 3:value 4:string

hook -group etc-group-highlight global WinSetOption filetype=etc-group %{ add-highlighter window/etc-group ref etc-group }
hook -group etc-group-highlight global WinSetOption filetype=(?!etc-group).* %{ remove-highlighter window/etc-group }

## /etc/gshadow
add-highlighter shared/etc-gshadow group
add-highlighter shared/etc-gshadow/ regex ^(\S+?):(\S+?)?:(\S+?)?:(\S+?)?$ 1:keyword 2:type 3:value 4:string

hook -group etc-gshadow-highlight global WinSetOption filetype=etc-gshadow %{ add-highlighter window/etc-gshadow ref etc-gshadow }
hook -group etc-gshadow-highlight global WinSetOption filetype=(?!etc-gshadow).* %{ remove-highlighter window/etc-gshadow }

## /etc/shadow
add-highlighter shared/etc-shadow group
add-highlighter shared/etc-shadow/ regex ^(\S+?):(\S+?):([0-9]+?):([0-9]+?)?:([0-9]+?)?:([0-9]+?)?:([0-9]+?)?:([0-9]+?)?:(.*?)?$ 1:keyword 2:type 3:value 4:value 5:value 6:value 7:value 8:value

hook -group etc-shadow-highlight global WinSetOption filetype=etc-shadow %{ add-highlighter window/etc-shadow ref etc-shadow }
hook -group etc-shadow-highlight global WinSetOption filetype=(?!etc-shadow).* %{ remove-highlighter window/etc-shadow }

## /etc/passwd
add-highlighter shared/etc-passwd group
add-highlighter shared/etc-passwd/ regex ^(\S+?):(\S+?):([0-9]+?):([0-9]+?):(.*?)?:(.+?):(.+?)$ 1:keyword 2:type 3:value 4:value 5:string 6:attribute 7:attribute

hook -group etc-passwd-highlight global WinSetOption filetype=etc-passwd %{ add-highlighter window/etc-passwd ref etc-passwd }
hook -group etc-passwd-highlight global WinSetOption filetype=(?!etc-passwd).* %{ remove-highlighter window/etc-passwd }
