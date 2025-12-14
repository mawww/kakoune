hook global BufCreate .*/\.ssh/config %{
    set-option buffer filetype ssh
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=ssh %{
    require-module ssh

    set-option window static_words %opt{ssh_static_words}

    hook window InsertChar \n -group ssh-insert ssh-insert-on-new-line
    hook window InsertChar \n -group ssh-indent ssh-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window ssh-.+ }
}

hook -group ssh-highlight global WinSetOption filetype=ssh %{
    add-highlighter window/ssh ref ssh
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/ssh }
}

provide-module ssh %@

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ssh regions
add-highlighter shared/ssh/code default-region group

add-highlighter shared/ssh/double_string region %{(?<!\\)(?:\\\\)*\K"} %{(?<!\\)(?:\\\\)*"} group
add-highlighter shared/ssh/single_string region %{(?<!\\)(?:\\\\)*\K'} %{'} fill string

add-highlighter shared/ssh/comment region (?<!\\)(?:\\\\)*(?:^|\h)\K# '$' fill comment

add-highlighter shared/ssh/double_string/fill fill string

evaluate-commands %sh{
    keywords="AddKeysToAgent AddressFamily AskPassGUI BatchMode BindAddress CanonicalDomains CanonicalizeFallbackLocal CanonicalizeHostname
              CanonicalizeMaxDots CanonicalizePermittedCNAMEs ChallengeResponseAuthentication CheckHostIP Cipher Ciphers ClearAllForwardings
              Compression CompressionLevel ConnectionAttempts ConnectTimeout ControlMaster ControlPath ControlPersist DynamicForward
              EnableSSHKeysign EscapeChar ExitOnForwardFailure FingerprintHash ForwardAgent ForwardX11 ForwardX11Timeout ForwardX11Trusted
              GatewayPorts GlobalKnownHostsFile GSSAPIAuthentication GSSAPIClientIdentity GSSAPIDelegateCredentials GSSAPIKeyExchange
              GSSAPIRenewalForcesRekey GSSAPIServerIdentity GSSAPITrustDns HashKnownHosts Host HostbasedAuthentication HostbasedKeyTypes
              HostKeyAlgorithms HostKeyAlias HostName IdentitiesOnly IdentityFile IgnoreUnknown IPQoS KbdInteractiveAuthentication
              KbdInteractiveDevices KexAlgorithms KeychainIntegration LocalCommand LocalForward LogLevel MACs Match NoHostAuthenticationForLocalhost
              NumberOfPasswordPrompts PasswordAuthentication PermitLocalCommand PKCS11Provider Port PreferredAuthentications Protocol ProxyCommand
              ProxyJump ProxyUseFdpass PubkeyAuthentication RekeyLimit RemoteForward RequestTTY RevokedHostKeys RhostsRSAAuthentication
              RSAAuthentication SendEnv ServerAliveCountMax ServerAliveInterval SmartcardDevice StreamLocalBindMask StreamLocalBindUnlink
              StrictHostKeyChecking TCPKeepAlive Tunnel TunnelDevice UpdateHostKeys UseKeychain UsePrivilegedPort User UserKnownHostsFile
              VerifyHostKeyDNS VisualHostKey XAuthLocation"

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    printf %s\\n "declare-option str-list ssh_static_words $(join "${keywords}" ' ') $(join "${builtins}" ' ')"

    printf %s\\n "add-highlighter shared/ssh/code/ regex (?<!-)\b($(join "${keywords}" '|'))\b(?!-) 0:keyword"
}

add-highlighter shared/ssh/code/ regex ^host\s+ 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden ssh-insert-on-new-line %{ evaluate-commands -itersel -draft %{
    execute-keys <semicolon>
    try %{
        evaluate-commands -draft -save-regs '/"' %{
            # Ensure previous line is a comment
            execute-keys -draft kxs^\h*#+\h*<ret>

            # now handle the coment continuation logic
            try %{
                # try and match a regular block comment, copying the prefix
                execute-keys -draft -save-regs '' k x 1s^(\h*#+\h*)\S.*$ <ret> y
                execute-keys -draft P
            } catch %{
                try %{
                    # try and match a regular block comment followed by a single
                    # empty comment line
                    execute-keys -draft -save-regs '' kKx 1s^(\h*#+\h*)\S+\n\h*#+\h*$ <ret> y
                    execute-keys -draft P
                } catch %{
                    try %{
                        # try and match a pair of empty comment lines, and delete
                        # them if we match
                        execute-keys -draft kKx <a-k> ^\h*#+\h*\n\h*#+\h*$ <ret> <a-d>
                    } catch %{
                        # finally, we need a special case for a new line inserted
                        # into a file that consists of a single empty comment - in
                        # that case we can't expect to copy the trailing whitespace,
                        # so we add our own
                        execute-keys -draft -save-regs '' k x1s^(\h*#+)\h*$<ret> y
                        execute-keys -draft P
                        execute-keys -draft i<space>
                    }
                }
            }
        }

        # trim trailing whitespace on the previous line
        try %{ execute-keys -draft k x s\h+$<ret> d }
    }
} }

define-command -hidden ssh-indent-on-new-line %<
    evaluate-commands -draft -itersel %<
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # cleanup trailing whitespaces from previous line
        try %{ execute-keys -draft k x s \h+$ <ret> d }
    >
>

@
