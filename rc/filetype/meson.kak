# meson syntax highlighting for kakoune (https://mesonbuild.com)
#
# For reference see:
# https://mesonbuild.com/Syntax.html
# https://github.com/mesonbuild/meson/blob/master/data/syntax-highlighting/vim/syntax/meson.vim

## Detection

hook global BufCreate (.*/|^)(meson\.build|meson_options\.txt) %{
  set-option buffer filetype meson
}

## Initialization

hook -group meson-highlight global WinSetOption filetype=meson %{
    require-module meson

    set-option window static_words %opt{meson_static_words}

    add-highlighter window/meson ref meson
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/meson }
}

provide-module meson %§

## Highlighters

add-highlighter shared/meson regions
add-highlighter shared/meson/code default-region group

add-highlighter shared/meson/comment region '(\h|^)#' '$' fill comment

# TODO: highlight escape sequences within strings
add-highlighter shared/meson/string_multiline region "'''" "'''" fill string
add-highlighter shared/meson/string_single    region "'" (?<!\\)(\\\\)*' fill string

# integer literals
add-highlighter shared/meson/code/ regex '\b\d+\b' 0:value

# operators
add-highlighter shared/meson/code/ regex '(?:\+|-|\*|/|%|!=|=|<|>|\?|:)' 0:operator
add-highlighter shared/meson/code/ regex '\b(?:and|not|or|in)\b' 0:operator

# functions
add-highlighter shared/meson/code/ regex "\b(\w+)\(" 1:function

evaluate-commands %sh{
    values="true false"

    keywords="if else endif elif foreach endforeach break continue"

    builtins="add_global_arguments add_global_link_arguments add_languages
    add_project_arguments add_project_link_arguments add_test_setup
    alias_target assert benchmark both_libraries build_machine build_target
    configuration_data configure_file custom_target declare_dependency
    dependency disabler environment error executable files find_library
    find_program generator get_option get_variable gettext host_machine
    import include_directories install_data install_headers install_man
    install_subdir is_disabler is_variable jar join_paths library meson
    message option project run_command run_target set_variable shared_library
    shared_module static_library subdir subdir_done subproject summary
    target_machine test vcs_tag warning"

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    # Add the language's grammar to the static completion list
    printf %s\\n "declare-option str-list meson_static_words $(join "${values} ${keywords} ${builtins}" ' ')"

    # Highlight keywords
    printf %s "
        add-highlighter shared/meson/code/ regex '\b($(join "${values}" '|'))\b' 0:value
        add-highlighter shared/meson/code/ regex '\b($(join "${keywords}" '|'))\b' 0:keyword
        add-highlighter shared/meson/code/ regex '\b($(join "${builtins}" '|'))\b\(' 1:builtin
    "
}

§
