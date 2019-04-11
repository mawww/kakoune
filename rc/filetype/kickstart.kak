hook global BufCreate .*\.ks %{
    set-option buffer filetype kickstart
}

hook global WinSetOption filetype=kickstart %{
    require-module kickstart
}

hook -group kickstart-highlight global WinSetOption filetype=kickstart %{
    add-highlighter window/kickstart ref kickstart
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/kickstart }
}


provide-module kickstart %{

add-highlighter shared/kickstart regions
add-highlighter shared/kickstart/code default-region group
add-highlighter shared/kickstart/comment region '(^|\h)\K#' $ fill comment
add-highlighter shared/kickstart/double_string region '"' (?<!\\)(\\\\)*" fill string
add-highlighter shared/kickstart/single_string region "'" (?<!\\)(\\\\)*' fill string
add-highlighter shared/kickstart/packages region '^\h*\K%packages\b' '^\h*\K%end\b' group
add-highlighter shared/kickstart/shell region '^\h*\K%(pre|pre-install|post)\b' '^\h*\K%end\b' group

add-highlighter shared/kickstart/code/ regex "^\h*\b(auth|authconfig|autopart|autostep|bootloader|btrfs|clearpart|cmdline|device|dmraid|driverdisk|fcoe|firewall|firstboot|group|graphical|halt|ignoredisk|install|cdrom|harddrive|liveimg|nfs|url|iscsi|iscsiname|keyboard|lang|logvol|logging|mediacheck|monitor|multipath|network|part|partition|poweroff|raid|realm|reboot|repo|rescue|rootpw|selinux|services|shutdown|sshkey|sshpw|skipx|text|timezone|updates|upgrade|user|vnc|volgroup|xconfig|zerombr|zfcp)\b" 1:keyword
add-highlighter shared/kickstart/code/ regex '(--[\w-]+=? ?)([^-"\n][^\h\n]*)?' 1:attribute 2:string
add-highlighter shared/kickstart/code/ regex '%(include|ksappend)\b' 0:keyword

add-highlighter shared/kickstart/packages/ regex "^\h*[\w-]*" 0:value
add-highlighter shared/kickstart/packages/ regex "#[^\n]*" 0:comment
add-highlighter shared/kickstart/packages/ regex "^\h*@\^?[\h\w-]*" 0:attribute
add-highlighter shared/kickstart/packages/ regex '\A\h*\K%packages\b' 0:type
add-highlighter shared/kickstart/packages/ regex '^\h*%end\b' 0:type
add-highlighter shared/kickstart/shell/ regex '\A\h*\K%(pre-install|pre|post)\b' 0:type
add-highlighter shared/kickstart/shell/ regex '^\h*%end\b' 0:type
add-highlighter shared/kickstart/shell/ ref sh

}
