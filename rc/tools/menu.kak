provide-module menu %§§

define-command menu -params 1.. -docstring %{
    menu [<switches>] <name1> <commands1> <name2> <commands2>...: display a 
    menu and execute commands for the selected item

    -auto-single instantly validate if only one item is available
    -select-cmds each item specify an additional command to run when selected
} %{
    evaluate-commands -save-regs a %{
        set-register a %arg{@}
        menu-impl
    }
}
define-command -hidden menu-impl %{
    evaluate-commands %sh{
        echo >$kak_command_fifo "echo -to-file $kak_response_fifo -quoting kakoune -- %reg{a}"
            perl < $kak_response_fifo -we '
            use strict;
            my $Q = "'\''";
            my @args = ();
            {
                my $arg = undef;
                my $prev_is_quote = 0;
                my $state = "before-arg";
                while (not eof(STDIN)) {
                    my $c = getc(STDIN);
                    if ($state eq "before-arg") {
                        ($c eq $Q) or die "bad char: $c";
                        $state = "in-arg";
                        $arg = "";
                    } elsif ($state eq "in-arg") {
                        if ($prev_is_quote) {
                            $prev_is_quote = 0;
                            if ($c eq $Q) {
                                $arg .= $Q;
                                next;
                            }
                            ($c eq " ") or die "bad char: $c";
                            push @args, $arg;
                            $state = "before-arg";
                            next;
                        } elsif ($c eq $Q) {
                            $prev_is_quote = 1;
                            next;
                        }
                        $arg .= $c;
                    }
                }
                ($state eq "in-arg") or die "expected $Q as last char";
                push @args, $arg;
            }

            my $auto_single = 0;
            my $select_cmds = 0;
            my $on_abort = "";
            while (defined $args[0] && $args[0] =~ m/^-/) {
                if ($args[0] eq "--") {
                    shift @args;
                    last;
                }
                if ($args[0] eq "-auto-single") {
                    $auto_single = 1;
                }
                if ($args[0] eq "-select-cmds") {
                    $select_cmds = 1;
                }
                if ($args[0] eq "-on-abort") {
                    if (not defined $args[1]) {
                        print "fail %{menu: missing argument to -on-abort}";
                        exit;
                    }
                    $on_abort = $args[1];
                    shift @args;
                }
                shift @args;
            }
            my $stride = 2 + $select_cmds;
            if (scalar @args == 0 or scalar @args % $stride != 0) {
                print "fail %{menu: wrong argument count}";
                exit;
            }
            if ($auto_single && scalar @args == $stride) {
                print $args[$0];
                exit;
            }

            sub shellquote {
                my $arg = shift;
                $arg =~ s/$Q/$Q\\$Q$Q/g;
                return "$Q$arg$Q";
            }
            sub kakquote {
                my $arg = shift;
                $arg =~ s/$Q/$Q$Q/g;
                return "$Q$arg$Q";
            }

            my $accept_cases = "";
            my $select_cases = "";
            my $completions = "";
            sub case_clause {
                my $name = shellquote shift;
                my $command = shellquote shift;
                return "($name)\n"
                     . " printf \"%s\n\" $command ;;\n";
            }
            for (my $i = 0; $i < scalar @args; $i += $stride) {
                my $name = $args[$i];
                my $command = $args[$i+1];
                $accept_cases .= case_clause $name, $command;
                $select_cases .= case_clause $name, $args[$i+2] if $select_cmds;
                $completions .= "$name\n";
            }
            use File::Temp qw(tempdir);
            my $tmpdir = tempdir;
            sub put {
                my $name = shift;
                my $contents = shift;
                my $filename = "$tmpdir/$name";
                open my $fh, ">", "$filename" or die "failed to open $filename: $!";
                print $fh $contents or die "write: $!";
                close $fh or die "close: $!";
                return $filename;
            };
            my $on_accept = put "on-accept",
                "case \"\$kak_text\" in\n" .
                "$accept_cases" .
                "(*) echo fail -- no such item: \"$Q\$(printf %s \"\$kak_text\" | sed \"s/$Q/$Q$Q/g\")$Q\";\n" .
                "esac\n";
            my $on_change = put "on-change",
                "case \"\$kak_text\" in\n" .
                "$select_cases" .
                "esac\n";
            my $shell_script_candidates = put "shell-script-candidates", $completions;

            print "prompt %{} %{ evaluate-commands %sh{. $on_accept kak_text; rm -r $tmpdir} }";
            print  " -on-abort " . kakquote "nop %sh{rm -r $tmpdir}; $on_abort";
            if ($select_cmds) {
                print " -on-change %{ evaluate-commands %sh{. $on_change kak_text} }";
            }
            print " -menu -shell-script-candidates %{cat $shell_script_candidates}";
        ' ||
            echo 'fail menu: encountered an error, see *debug* buffer';
    }
}
