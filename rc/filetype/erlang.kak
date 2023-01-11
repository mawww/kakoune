# Erlang/OTP
# https://erlang.org
# ----------------------

# Detection and Initialization sections were adapted from rc/filetype/elixir.kak

# Detection
# ‾‾‾‾‾‾‾‾‾
hook global BufCreate .*[.](erl|hrl) %{
    set-option buffer filetype erlang
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾
hook global WinSetOption filetype=erlang %{
    require-module erlang

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window erlang-.+ }
}

hook -group erlang-highlight global WinSetOption filetype=erlang %{
    add-highlighter window/erlang ref erlang
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/erlang }
}

provide-module erlang %[

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/erlang regions
add-highlighter shared/erlang/default default-region group

add-highlighter shared/erlang/comment region '(?<!\$)%' '$' fill comment
add-highlighter shared/erlang/attribute_atom_single_quoted region %{-'} %{(?<!\\)(?:\\\\)*'(?=[\( \.])} fill builtin
add-highlighter shared/erlang/attribute region '\b-[a-z][\w@]*(?=[\( \.])' '\K' fill builtin
add-highlighter shared/erlang/atom_single_quoted region %{'} %{(?<!\\)(?:\\\\)*'} fill type
add-highlighter shared/erlang/char_list region %{"} %{(?<!\\)(?:\\\\)*"} fill string
add-highlighter shared/erlang/dollar_double_quote region %{\$\\?"} '\K' fill value
add-highlighter shared/erlang/dollar_single_quote region %{\$\\?'} '\K' fill value

# default-region regex highlighters
add-highlighter shared/erlang/default/atom regex '\b[a-z][\w@]*\b' 0:type
add-highlighter shared/erlang/default/funtion_call regex '\b[a-z][\w@]*(?=\()' 0:function
add-highlighter shared/erlang/default/keywords regex '\b(after|begin|case|try|catch|end|fun|if|of|receive|when|andalso|orelse|bnot|not|div|rem|band|and|bor|bxor|bsl|bsr|or|xor)\b' 0:keyword
add-highlighter shared/erlang/default/variable_name regex '\b(?<!\?)[A-Z_][\w@]*\b' 0:variable
add-highlighter shared/erlang/default/module_prefix regex '\b([a-z][\w_@]+)(?=:)' 1:value
# e.g. #Ref<0.1380825548.292038421.197518>
add-highlighter shared/erlang/default/ref regex '#Ref<\d+\.\d+\.\d+\.\d+>' 0:value
# e.g. #Port<0.1>
add-highlighter shared/erlang/default/port regex '#Port<\d+\.\d+>' 0:value
# e.g. <0.401.0>
add-highlighter shared/erlang/default/pid regex '<\d+\.\d+\.\d+>' 0:value
add-highlighter shared/erlang/default/base_number regex '\b(\d[_\d]*(?<!_)#[a-zA-Z0-9][a-z_A-Z0-9]*(?<!_)(?!\{))\b' 1:value
add-highlighter shared/erlang/default/float regex '\b(?<![\.])(\d[\d_]*(?<!_)\.\d[\d_]*(?<!_)(?:e[+-]?\d[\d_]*(?<!_))?)\b' 1:value
add-highlighter shared/erlang/default/integer regex '\b(?<!/)(\d[\d_]*)(?<!_)\b' 1:value
# e.g $\xff
add-highlighter shared/erlang/default/dollar_hex regex '\$\\x[0-9a-f][0-9a-f]' 0:value
# e.g. $\^a $\^C
add-highlighter shared/erlang/default/dollar_ctrl_char regex '\$\\\^[a-zA-Z]' 0:value
# e.g. $\101, $\70
add-highlighter shared/erlang/default/dollar_octal regex '\$\\[0-7][0-7][0-7]?' 0:value
# e.g. $\↕ $\7
add-highlighter shared/erlang/default/dollar_esc_char regex '\$\\[^x]' 0:value
# e.g. $↕ $a
add-highlighter shared/erlang/default/dollar_char regex '\$.' 0:value

]
