hook global BufCreate .*\.ks %{
    set buffer filetype kickstart
}

add-highlighter -group / regions -default code kickstart \
    comment (^|\h)\K\# $ '' \
    double_string '"' (?<!\\)(\\\\)*" '' \
    single_string "'" (?<!\\)(\\\\)*' '' \
    packages '^\h*\K%packages\b' '^\h*\K%end\b' '' \
    shell '^\h*\K%(pre|pre-install|post)\b' '^\h*\K%end\b' ''

add-highlighter -group /kickstart/code regex "^\h*\b(auth|authconfig|autopart|autostep|bootloader|btrfs|clearpart|cmdline|device|dmraid|driverdisk|fcoe|firewall|firstboot|group|graphical|halt|ignoredisk|install|cdrom|harddrive|liveimg|nfs|url|iscsi|iscsiname|keyboard|lang|logvol|logging|mediacheck|monitor|multipath|network|part|partition|poweroff|raid|realm|reboot|repo|rescue|rootpw|selinux|services|shutdown|sshkey|sshpw|skipx|text|timezone|updates|upgrade|user|vnc|volgroup|xconfig|zerombr|zfcp)\b" 1:keyword
add-highlighter -group /kickstart/code regex '(--[\w-]+=? ?)([^-"\n][^\h\n]*)?' 1:attribute 2:string
add-highlighter -group /kickstart/code regex '%(include|ksappend)\b' 0:keyword

add-highlighter -group /kickstart/comment fill comment
add-highlighter -group /kickstart/single_string fill string
add-highlighter -group /kickstart/double_string fill string
add-highlighter -group /kickstart/packages regex "^\h*[\w-]*" 0:value
add-highlighter -group /kickstart/packages regex "#[^\n]*" 0:comment
add-highlighter -group /kickstart/packages regex "^\h*@\^?[\h\w-]*" 0:attribute
add-highlighter -group /kickstart/packages regex '\`\h*\K%packages\b' 0:type
add-highlighter -group /kickstart/packages regex '^\h*%end\b' 0:type
add-highlighter -group /kickstart/shell regex '\`\h*\K%(pre-install|pre|post)\b' 0:type
add-highlighter -group /kickstart/shell regex '^\h*%end\b' 0:type
add-highlighter -group /kickstart/shell ref sh


hook -group kickstart-highlight global WinSetOption filetype=kickstart %{ add-highlighter ref kickstart }
hook -group kickstart-highlight global WinSetOption filetype=(?!kickstart).* %{ remove-highlighter kickstart }
