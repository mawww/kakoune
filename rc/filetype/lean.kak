# https://lean-lang.org/
# ----------------------

# Detection
# ---------

hook global BufCreate .*[.]lean %{
    set-option buffer filetype lean
    set-option -add buffer matching_pairs ⟨ ⟩
}


# Initialization
# --------------

hook global WinSetOption filetype=lean %{
    require-module lean

    hook buffer InsertChar \n lean-indent-on-new-line

    hook buffer InsertChar [^\n] %{ lean-abbreviation-try-substitute %{
        reg / %opt{lean_abbreviation_terminated_regex}
        execute-keys ';<a-/><ret>1s<ret>'
    } }

    hook buffer ModeChange pop:insert:[^:]* %{ lean-abbreviation-try-substitute %{
        reg / %opt{lean_abbreviation_full_regex}
        execute-keys ';<a-/><ret>'
    } }
}


provide-module lean %{

# Options
# -------
declare-option regex lean_abbreviation_full_regex
declare-option regex lean_abbreviation_terminated_regex
declare-option str lean_abbreviation_substitute_command

# defines:
#  - lean_abbreviation_full_regex
#  - lean_abbreviation_terminated_regex
#  - lean_abbreviation_substitute_command
evaluate-commands %sh{perl "$kak_runtime/rc/filetype/lean-abbreviations.pl" "$kak_runtime/rc/filetype/lean_abbreviations.json"}

define-command lean-abbreviation-substitute %{
    evaluate-commands %opt{lean_abbreviation_substitute_command}
}

define-command -params 1 lean-abbreviation-try-substitute %{
    evaluate-commands -save-regs '/"|^@s' %{
        try %{
            evaluate-commands -draft %{
                evaluate-commands %arg{1}
                lean-abbreviation-substitute
                reg s %val{selections_desc}
            }
            evaluate-commands -draft %{
                select %reg{s}
                set-register / '\$CURSOR'
                execute-keys 's<ret>d'
                reg s %val{selections_desc}
            }
            select %reg{s}
        }
    }
}

# Commands
# --------

define-command -hidden lean-indent-on-new-line %<
    # preserve previous line indent
    try %{ execute-keys -draft <semicolon> K <a-&> }
    # cleanup trailing whitespaces from previous line
    try %{ execute-keys -draft k x s \h+$ <ret> d }
    # remove previous empty lines resulting from the automatic indent
    try %< execute-keys -draft k x <a-k>^\h+$<ret> Hd >
>

}
