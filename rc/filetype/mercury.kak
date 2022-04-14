# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](m) %{
    set-option buffer filetype mercury
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=mercury %{
    require-module mercury

    set-option window static_words %opt{mercury_static_words}

    hook window InsertChar \n -group mercury-insert mercury-insert-on-new-line
    hook window InsertChar \n -group mercury-indent mercury-indent-on-new-line
    # cleanup trailing whitespaces on current line insert end
    hook window ModeChange pop:insert:.* -group mercury-trim-indent %{ try %{ execute-keys -draft <semicolon> x s ^\h+$ <ret> d } }
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window mercury-.+ }
}

hook -group mercury-highlight global WinSetOption filetype=mercury %{
    add-highlighter window/mercury ref mercury
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/mercury }
}


provide-module mercury %§


# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/mercury regions
add-highlighter shared/mercury/code default-region group
add-highlighter shared/mercury/comment region '%' '$' fill comment
add-highlighter shared/mercury/line_comment region '%' '$' fill comment

add-highlighter shared/mercury/string region '"' (?<!\\)(\\\\)*"  fill string
# Integer formats
add-highlighter shared/mercury/code/ regex '\b0b[01]+\b' 0:value
add-highlighter shared/mercury/code/ regex '\b0x[\da-f]+\b' 0:value
add-highlighter shared/mercury/code/ regex '\b0o?[0-7]+\b' 0:value
add-highlighter shared/mercury/code/ regex '\b([1-9]\d*|0)(i8|i16|i32|i64|u|u8|u16|u32|u64)?\b' 0:value
# Float formats
add-highlighter shared/mercury/code/ regex '\b(\d+_\d+)*\d+[eE][+-]?\d+(\d+_\d+)*\b' 0:value
add-highlighter shared/mercury/code/ regex '(\b\d+)\.\d+\b' 0:value
add-highlighter shared/mercury/code/ regex  '`[A-Za-z][A-Za-z_0-9]+`' 0:operator

add-highlighter shared/mercury/code/ regex \b[a-z][A-Za-z_0-9]*(?=\.\w) 0:module
add-highlighter shared/mercury/code/ regex \b[a-z][A-Za-z_0-9]*(?=__\w) 0:module
# func() and pred()
add-highlighter shared/mercury/code/ regex \b(pred|func)(?=\() 0:builtin
add-highlighter shared/mercury/code/ regex \b[A-Z][A-Za-z_0-9]*\b 0:variable

# operator symbols
add-highlighter shared/mercury/code/ regex (\.|!|!\.|!:|@|\^|:|\*\*|\\|\*|/|//|<<|>>|\+|\+\+|-|--|/\\|\\/|\.\.|:=|=\^|<|=|=\.\.|=:=|=<|==|=\\=|>|>=|@<|@=<|@>|@>=|\\=|\\==|~=|\\\+|~|<=|<=>|=>|,|&|->|\;|::|==>|--->|-->|:-|\?-) 0:operator

evaluate-commands %sh{
    # Grammar
    values="true fail"

    # There is overlap between all of these. Not sure what to do about that.
    operators="event div mod rem for is and or impure semipure not when all arbitrary atomic disable_warning disable_warnings promise_equivalent_solutions promise_equivalent_solution_setspromise_exclusive promise_exclusive_exhaustive promise_exhaustive promise_impure promise_pure promise_semipure require_complete_switch require_switch_arms_det require_switch_arms_semidet require_switch_arms_multi require_switch_arms_nondet require_switch_arms_cc_multi require_switch_arms_cc_nondet require_switch_arms_erroneous require_switch_arms_failure require_det require_semidet require_multi require_nondet require_cc_multi require_cc_nondet require_erroneous require_failure trace try some or_else then if else where catch catch_any initialize finalize rule solver type pred func inst mode typeclass instance pragma promise initialise finalise mutable module import_module use_module include_module"
    modes="free bound in out di mdi uo muo"
    determinisms="erroneous failure det semidet multi cc_multi nondet cc_nondet"
    reserved_insts="any bound bound_unique clobbered clobbered_any free ground is mostly_clobbered mostly_unique mostly_unique_any not_reached unique unique_any"
    reserved_modes="any_func any_pred func is pred"
    reserved_types="int int8 int16 int32 int64 uint uint8 uint16 uint32 uint64 float character string pred func pure semipure impure"

    declarations="type solver type pred func inst mode typeclass instance pragma promise initialise finalise mutable module interface implementation import_module use_module include_module end_module"
 
    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    # Add the language's grammar to the static completion list
    printf %s\\n "declare-option str-list mercury_static_words $(join "${values} ${declarations} ${operators} ${determinisms} ${modes} ${reserved_insts} ${reserved_modes}" ' ')"

    # Highlight keywords
    printf %s "
        add-highlighter shared/mercury/code/ regex '\b($(join "${values}" '|'))\b' 0:value
        add-highlighter shared/mercury/code/ regex '^:-(\s+($(join "${declarations}" '|')))+\b' 1:keyword
        add-highlighter shared/mercury/code/ regex '\b($(join "${operators}" '|'))\b' 0:operator
        add-highlighter shared/mercury/code/ regex '\b($(join "${determinisms}" '|'))\b' 0:keyword
        add-highlighter shared/mercury/code/ regex '\b($(join "${modes}" '|'))\b' 0:keyword

        # These overlap with previous. Not sure what the solution is.
        # add-highlighter shared/mercury/code/ regex '\b($(join "${reserved_types}" '|'))\b' 0:type
        # add-highlighter shared/mercury/code/ regex '\b($(join "${reserved_insts}" '|'))\b' 0:builtin
        # add-highlighter shared/mercury/code/ regex '\b($(join "${reserved_modes}" '|'))\b' 0:builtin

        # Implementation-defined literals - don't know how to get these to work
        # add-highlighter shared/mercury/code/ regex '\$(file|line|module|pred)' 0:keyword
    "
}

define-command -hidden mercury-insert-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy '%' comment prefix and following white spaces
        try %{ execute-keys -draft k x s ^\h*\%\h* <ret> y gh j P }
    }
}

define-command -hidden mercury-indent-on-char %<
    evaluate-commands -draft -itersel %<
        # align closer token to its opener when alone on a line
        try %/ execute-keys -draft <a-h> <a-k> ^\h+[]}]$ <ret> m s \A|.\z <ret> 1<a-&> /
    >
>

define-command -hidden mercury-indent-on-new-line %<
    evaluate-commands -draft -itersel %<
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # cleanup trailing whitespaces from previous line
        try %{ execute-keys -draft k x s \h+$ <ret> d }
        # indent after line ending with :-
        try %{ execute-keys -draft , k x <a-k> :-$ <ret> j <a-gt> }
        # deindent closing brace/bracket when after cursor
        try %< execute-keys -draft x <a-k> ^\h*[}\])] <ret> gh / [}\])] <ret> m <a-S> 1<a-&> >
    >
>

§
