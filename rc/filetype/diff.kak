hook global BufCreate .*\.(diff|patch) %{
    set-option buffer filetype diff
}

hook global WinSetOption filetype=diff %{
    require-module diff
    map buffer normal <ret> :diff-jump<ret>
}

hook -group diff-highlight global WinSetOption filetype=diff %{
    add-highlighter window/diff ref diff
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/diff }
}

provide-module diff %§

add-highlighter shared/diff group
add-highlighter shared/diff/ regex "^\+[^\n]*\n" 0:green,default
add-highlighter shared/diff/ regex "^-[^\n]*\n" 0:red,default
add-highlighter shared/diff/ regex "^@@[^\n]*@@" 0:cyan,default
# If any trailing whitespace was introduced in diff, show it with red background
add-highlighter shared/diff/ regex "^\+[^\n]*?(\h+)\n" 1:default,red

define-command diff-jump -params .. -docstring %{
        diff-jump [<switches>] [<directory>]: edit the diff's source file at the cursor position.
        Paths are resolved relative to <directory>, or the current working directory if unspecified.

        Switches:
            -       jump to the old file instead of the new file
            -<num> strip <num> leading directory components, like -p<num> in patch(1). Defaults to 1 if there is a 'diff' line (as printed by 'diff -r'), or 0 otherwise.
    } %{
    evaluate-commands -draft -save-regs ac| %{
        # Save the column because we will move the cursor.
        set-register c %val{cursor_column}
        # If there is a "diff" line, we don't need to look further back.
        try %{
            execute-keys %{<a-l><semicolon><a-?>^(?:> )*diff\b<ret>x}
        } catch %{
            # A single file diff won't have a diff line. Start parsing from
            # the buffer start, so we can tell if +++/--- lines are headers
            # or content.
            execute-keys Gk
        }
        set-register a %arg{@}
        set-register | %{
            [ -n "$kak_reg_a" ] && eval set -- $kak_quoted_reg_a
            cmd=$(column=$kak_reg_c perl -we '
                sub quote {
                    $SQ = "'\''";
                    $token = shift;
                    $token =~ s/$SQ/$SQ$SQ/g;
                    return "$SQ$token$SQ";
                }
                sub fail {
                    $reason = shift;
                    print "fail " . quote("diff-jump: $reason");
                    exit;
                }
                $version = "+", $other_version = "-";
                $strip = undef;
                $directory = $ENV{PWD};
                $seen_ddash = 0;
                foreach (@ARGV) {
                    if ($seen_ddash or !m{^-}) {
                        $directory = $_;
                    } elsif ($_ eq "-") {
                        $version = "-", $other_version = "+";
                    } elsif (m{^-(\d+)$}) {
                        $strip = $1;
                    } elsif ($_ eq "--") {
                        $seen_ddash = 1;
                    } else {
                        fail "unknown option: $_";
                    }
                }
                $have_diff_line = 0;
                $state = "header";
                while (<STDIN>) {
                    s/^(> )*//g;
                    $last_line = $_;
                    if (m{^diff\b}) {
                        $state = "header";
                        $have_diff_line = 1;
                        if (m{^diff -\S* (\S+) (\S+)$}) {
                            $fallback_file = $version eq "+" ? $2 : $1;
                        }
                        next;
                    }
                    if ($state eq "header") {
                        if (m{^[$version]{3} ([^\t\n]+)}) {
                            $file = $1;
                            next;
                        }
                        if (m{^[$other_version]{3} ([^\t\n]+)}) {
                            $fallback_file = $1;
                            next;
                        }
                    }
                    if (m{^@@ -(\d+)(?:,\d+)? \+(\d+)(?:,\d+)? @@}) {
                        $state = "contents";
                        $line = ($version eq "+" ? $2 : $1) - 1;
                    } elsif (m{^[ $version]}) {
                        $line++ if defined $line;
                    }
                }
                if (not defined $file) {
                    $file = $fallback_file;
                }
                if (not defined $file) {
                    fail "missing diff header";
                }
                if (not defined $strip) {
                    # A "diff -r" or "git diff" adds "diff" lines to
                    # the output.  If no such line is present, we have
                    # a plain diff between files (not directories), so
                    # there should be no need to strip the directory.
                    $strip = $have_diff_line ? 1 : 0;
                }
                if ($file !~ m{^/}) {
                    $file =~ s,^([^/]+/+){$strip},, or fail "directory prefix underflow";
                    $file = "$directory/$file";
                }

                if (defined $line) {
                    $column = $ENV{column} - 1; # Account for [ +-] diff prefix.
                    # If the cursor was on a hunk header, go to the section header if possible.
                    if ($last_line =~ m{^(@@ -\d+(?:,\d+)? \+\d+(?:,\d+) @@ )([^\n]*)}) {
                        $hunk_header_prefix = $1;
                        $hunk_header_from_userdiff = $2;
                        open FILE, "<", $file or fail "failed to open file: $!: $file";
                        @lines = <FILE>;
                        for (my $i = $line - 1; $i >= 0 && $i < scalar @lines; $i--) {
                            if ($lines[$i] !~ m{\Q$hunk_header_from_userdiff}) {
                                next;
                            }
                            $line = $i + 1;
                            # Re-add 1 because the @@ line does not have a [ +-] diff prefix.
                            $column = $column + 1 - length $hunk_header_prefix;
                            last;
                        }
                    }
                }

                printf "edit -existing -- %s $line $column", quote($file);
            ' -- "$@")
            echo "set-register c $cmd" >"$kak_command_fifo"
        }
        execute-keys <a-|><ret>
        evaluate-commands -client %val{client} %{
            evaluate-commands -try-client %opt{jumpclient} %{
                %reg{c}
            }
        }
    }
}
complete-command diff-jump file

§

define-command \
    -docstring %{diff-select-file: Select surrounding patch file} \
    -params 0 \
    diff-select-file %{
                evaluate-commands -itersel -save-regs 'ose/' %{
        try %{
            execute-keys '"oZgl<a-?>^diff <ret>;"sZ' 'Ge"eZ'
            try %{ execute-keys '"sz?\n(?=diff )<ret>"e<a-Z><lt>' }
            execute-keys '"ez'
        } catch %{
            execute-keys '"oz'
            fail 'Not in a diff file'
        }
    }
}

define-command \
    -docstring %{diff-select-hunk: Select surrounding patch hunk} \
    -params 0 \
    diff-select-hunk %{
    evaluate-commands -itersel -save-regs 'ose/' %{
        try %{
            execute-keys '"oZgl<a-?>^@@ <ret>;"sZ' 'Ge"eZ'
            try %{ execute-keys '"sz?\n(?=diff )<ret>"e<a-Z><lt>' }
            try %{ execute-keys '"sz?\n(?=@@ )<ret>"e<a-Z><lt>' }
            execute-keys '"ez'
        } catch %{
            execute-keys '"oz'
            fail 'Not in a diff hunk'
        }
    }
}
