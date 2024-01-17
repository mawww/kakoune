# Prolog
# ----------------------

# Adapted from rc/filetype/erlang.kak

# Detection
# ‾‾‾‾‾‾‾‾‾
hook global BufCreate .*[.](pl|P) %{
    set-option buffer filetype prolog
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾
hook global WinSetOption filetype=prolog %{
    require-module prolog

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window prolog-.+ }
}

hook -group prolog-highlight global WinSetOption filetype=prolog %{
    add-highlighter window/prolog ref prolog
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/prolog }
}

provide-module prolog %[

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/prolog regions
add-highlighter shared/prolog/default default-region group

add-highlighter shared/prolog/comment region '(?<!)%' '$' fill comment
add-highlighter shared/prolog/attribute_atom_single_quoted region %{-'} %{(?<!\\)(?:\\\\)*'(?=[\( \.])} fill builtin
add-highlighter shared/prolog/attribute region '\b-[a-z][\w@]*(?=[\( \.])' '\K' fill builtin
add-highlighter shared/prolog/atom_single_quoted region %{'} %{(?<!\\)(?:\\\\)*'} fill type
add-highlighter shared/prolog/char_list region %{"} %{(?<!\\)(?:\\\\)*"} fill string

# default-region regex highlighters
add-highlighter shared/prolog/default/atom regex '\b[a-z]\w*\b' 0:type
add-highlighter shared/prolog/default/pred_call regex '\b[a-z]\w*(?=\()' 0:function
add-highlighter shared/prolog/default/keywords regex '\b(div|rem|is)\b' 0:keyword
add-highlighter shared/prolog/default/variable_name regex '\b(?<!\?)[A-Z_][\w]*\b' 0:variable

add-highlighter shared/prolog/default/base_number regex '\b(\d[_\d]*(?<!_)#[a-zA-Z0-9][a-z_A-Z0-9]*(?<!_)(?!\{))\b' 1:value
add-highlighter shared/prolog/default/float regex '\b(?<![\.])(\d[\d_]*(?<!_)\.\d[\d_]*(?<!_)(?:e[+-]?\d[\d_]*(?<!_))?)\b' 1:value
add-highlighter shared/prolog/default/integer regex '\b(?<!/)(\d[\d_]*)(?<!_)\b' 1:value

]
