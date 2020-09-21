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

    hook window ModeChange insert:.* -group ninja-trim-indent  ninja-trim-indent
    hook window InsertChar \n -group ninja-indent ninja-indent-on-new-line
    # cleanup trailing whitespaces on current line insert end
    hook window ModeChange pop:insert:.* -group ninja-trim-indent %{ try %{ execute-keys -draft <semicolon> <a-x> s ^\h+$ <ret> d } }

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window ninja-.+ }
}

hook -group ninja-highlight global WinSetOption filetype=ninja %{
    add-highlighter window/ninja ref ninja
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/ninja }
}

# Lazy loading
provide-module ninja %{

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ninja regions

# `#`
add-highlighter shared/ninja/comment region '#' '\n' fill comment

# `rule`
add-highlighter shared/ninja/rule region '^rule' '\n' group
add-highlighter shared/ninja/rule/rule regex '^rule' 0:keyword

# `command`
add-highlighter shared/ninja/command region '^\h+command' '[^$]\n' group
add-highlighter shared/ninja/command/command regex 'command' 0:variable
add-highlighter shared/ninja/command/equal regex '=' 0:operator
add-highlighter shared/ninja/command/linebreak regex '\$$' 0:operator
add-highlighter shared/ninja/command/variables regex '\$\w+|\$\{\w+\}' 0:value

# `build`
add-highlighter shared/ninja/build region '^build ' '\n' group
add-highlighter shared/ninja/build/build regex '^build' 0:keyword
add-highlighter shared/ninja/build/rule regex ':\h+(\w+)' 0:function
add-highlighter shared/ninja/build/colonpipe regex ':|\||\|\|' 0:operator
add-highlighter shared/ninja/build/variables regex '\$\w+|\$\{\w+\}' 0:value

# variables
add-highlighter shared/ninja/variable region '^\h*\w+\h*=' '\n' group
add-highlighter shared/ninja/variable/name regex '(\w+)\h*=' 0:variable
add-highlighter shared/ninja/variable/equal regex '=' 0:operator

# `default`
add-highlighter shared/ninja/default region '^default' '\n' group
add-highlighter shared/ninja/default/default regex '^default' 0:keyword

# `subninja` and `include`
add-highlighter shared/ninja/subinc region '^subninja|include' '\n' group
add-highlighter shared/ninja/subinc/default regex '^subninja|include' 0:module

# `pool`
add-highlighter shared/ninja/pool region '^pool' '\n' group
add-highlighter shared/ninja/pool/pool regex '^pool' 0:keyword

# keywords/builtin variable names
evaluate-commands %sh{
  keywords="rule build command default"
  reserved_names="builddir ninja_required_version pool depfile deps depfile msvc_deps_prefix description dyndep generator restat rspfile rspfile_content"

  printf %s "
      declare-option str-list ninja_static_words ${keywords} ${reserved_names}
  "

  reserved_names_regex="$(echo ${reserved_names} | tr ' ' '|')"
  printf %s "
      add-highlighter shared/ninja/variable/reserved_names regex ${reserved_names_regex} 0:meta
  "
}

# Indent
# ‾‾‾‾‾‾

define-command -hidden ninja-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden ninja-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy -- comments prefix and following white spaces
        try %{ execute-keys -draft k <a-x> s ^\h*\K--\h* <ret> y gh j P }
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : ninja-trim-indent <ret> }
        # indent after lines begining with rule and pool (do people want build too ?)
        try %{ execute-keys -draft \; k x <a-k> ^(\brule|pool) <ret> j <a-gt> }
    }
}

}
