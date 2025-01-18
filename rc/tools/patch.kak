define-command patch -params .. -docstring %{
    patch [<arguments>]: apply selections in diff to a file

    Given some selections within a unified diff, apply the changed lines in
    each selection by piping them to "patch <arguments> 1>&2"
    (or "<arguments> 1>&2" if <arguments> starts with a non-option argument).
    If successful, the in-buffer diff will be updated to reflect the applied
    changes.
    For selections that contain no newline, the entire enclosing diff hunk
    is applied (unless the cursor is inside a diff header, in which case
    the entire diff is applied).
    To revert changes, <arguments> must contain "--reverse" or "-R".
} %{
    evaluate-commands -draft -itersel -save-regs aes|^ %{
        try %{
            execute-keys <a-k>\n<ret>
        } catch %{
            # The selection contains no newline.
            execute-keys -save-regs '' Z
            execute-keys <a-l><semicolon><a-?>^diff<ret>
            try %{
                execute-keys <a-k>^@@<ret>
                # If the cursor is in a diff hunk, stage the entire hunk.
                execute-keys z
                execute-keys /.*?(?:(?=\n@@)|(?=\ndiff)|(?=\n\n)|\z)<ret>x<semicolon><a-?>^@@<ret>
            } catch %{
                # If the cursor is in a diff header, stage the entire diff.
                execute-keys <a-semicolon>?.*?(?:(?=\ndiff)|(?=\n\n)|\z)<ret>
            }
        }
        # We want to apply only the selected lines. Remember them.
        execute-keys <a-:>
        set-register s %val{selection_desc}
        # Select forward until the end of the last hunk.
        execute-keys H?.*?(?:(?=\n@@)|(?=\ndiff)|(?=\n\n)|\z)<ret>x
        # Select backward to the beginning of the first hunk's diff header.
        execute-keys <a-semicolon><a-L><a-?>^diff<ret>
        # Move cursor to the beginning so we know the diff's offset within the buffer.
        execute-keys <a-:><a-semicolon>
        set-register a %arg{@}
        set-register e nop
        set-register | %{
            # The selected range to apply.
            IFS=' .,' read min_line _ max_line _ <<-EOF
            $kak_reg_s
	EOF
            min_line=$((min_line - kak_cursor_line + 1))
            max_line=$((max_line - kak_cursor_line + 1))

            # Since registers are never empty, we get an empty arg even if
            # there were no args. This does no harm because we pass it to
            # a shell where it expands to nothing.
            eval set -- "$kak_quoted_reg_a"

            perl "${kak_runtime}"/rc/tools/patch-range.pl -print-remaining-diff $min_line $max_line "$@" ">&2" ||
                echo >$kak_command_fifo "set-register e fail 'patch: failed to apply selections, see *debug* buffer'"
        }
        execute-keys |<ret>
        %reg{e}
    }
}

provide-module patch %§§
