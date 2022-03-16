# ref: https://ninja-build.org/manual.html#ref_ninja_file

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .+\.ninja %{
    set-option buffer filetype ninja
}


# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=ninja %{
    require-module ninja

    set-option window static_words %opt{ninja_static_words}

    hook window ModeChange pop:insert:.* -group ninja-trim-indent ninja-trim-indent
    hook window InsertChar \n -group ninja-insert ninja-insert-on-new-line
    hook window InsertChar \n -group ninja-indent ninja-indent-on-new-line
    # cleanup trailing whitespaces on current line insert end
    hook window ModeChange pop:insert:.* -group ninja-trim-indent %{ try %{ execute-keys -draft <semicolon> x s ^\h+$ <ret> d } }

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window ninja-.+ }
}

hook -group ninja-highlight global WinSetOption filetype=ninja %{
    add-highlighter window/ninja ref ninja
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/ninja }
}


provide-module ninja %{

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ninja regions

# `#`
add-highlighter shared/ninja/comment region '#' '\n' fill comment

# `subninja`, `include`, `rule`, `pool` and `default` declarations
add-highlighter shared/ninja/sirpd region '^(subninja|include|rule|pool|default)\b' '[^$]\n' group
add-highlighter shared/ninja/sirpd/default regex '^(subninja|include)\b' 0:module
add-highlighter shared/ninja/sirpd/rulepool regex '^(rule|pool|default)\b' 0:keyword
add-highlighter shared/ninja/sirpd/linebreak regex '\$$' 0:operator

# `build`
add-highlighter shared/ninja/build region '^build\b' '[^$]\n' group
add-highlighter shared/ninja/build/build regex '^build\b' 0:keyword
add-highlighter shared/ninja/build/rule regex ':\h+((\w|-)+)' 0:function
add-highlighter shared/ninja/build/colonpipe regex ':|\||\|\|' 0:operator
add-highlighter shared/ninja/build/linebreak regex '\$$' 0:operator
add-highlighter shared/ninja/build/variables regex '\$(\w|-)+|\$\{(\w|-)+\}' 0:value

# variables declarations
add-highlighter shared/ninja/variable region '^\h*(\w|-)+\h*=' '[^$]\n' group
add-highlighter shared/ninja/variable/declaredname regex '^\h*((\w|-)+)\h*(=)' 1:variable 0:operator
add-highlighter shared/ninja/variable/linebreak regex '\$$' 0:operator
add-highlighter shared/ninja/variable/variables regex '\$(\w|-)+|\$\{(\w|-)+\}' 0:value

# keywords/builtin variable names
evaluate-commands %sh{
  keywords="rule build command default subninja include"
  reserved_names="builddir ninja_required_version pool depfile deps depfile msvc_deps_prefix description dyndep generator restat rspfile rspfile_content"

  printf %s "
      declare-option str-list ninja_static_words ${keywords} ${reserved_names}
  "

  reserved_names_regex="$(echo ${reserved_names} | tr ' ' '|')"
  printf %s "
      add-highlighter shared/ninja/variable/reserved_names regex '^\h*(${reserved_names_regex})\b' 0:meta
  "
}


# Indent
# ‾‾‾‾‾‾

define-command -hidden ninja-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden ninja-insert-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy -- comments prefix and following white spaces
        try %{ execute-keys -draft k x s ^\h*\K--\h* <ret> y gh j P }
    }
}

define-command -hidden ninja-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : ninja-trim-indent <ret> }
        # indent after lines begining with rule and pool
        try %{ execute-keys -draft \; k x <a-k> ^(rule|pool|build)\b <ret> j <a-gt> }
    }
}

}
