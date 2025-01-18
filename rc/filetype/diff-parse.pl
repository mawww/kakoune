#!/usr/bin/env perl

use warnings;

sub quote {
    my $token = shift;
    $token =~ s/'/''/g;
    return "'$token'";
}
sub fail {
    my $reason = shift;
    print quote("diff-parse.pl: $reason");
    exit 1;
}

my $begin;
my $end;

while (defined $ARGV[0]) {
    if ($ARGV[0] eq "--") {
        shift;
        last;
    }
    if ($ARGV[0] =~ m{^(BEGIN|END)$}) {
        if (not defined $ARGV[1]) {
            fail "missing argument to $ARGV[0]";
        }
        if ($ARGV[0] eq "BEGIN") {
            $begin = $ARGV[1];
        } else {
            $end = $ARGV[1];
        }
        shift, shift;
        next;
    }
    fail "unknown argument: $ARGV[0]";
}

# Inputs
our $directory = $ENV{PWD};
our $strip;
our $in_file;
our $in_file_line;
our $version = "+";

eval $begin if defined $begin;

$in_file = "$directory/$in_file" if defined $in_file && $in_file ne "";

# Outputs
our $diff_line = 0;
our $commit;
our $file;
our $file_line;
our $other_file;
our $other_file_line;
our $diff_line_text;

my $other_version;
if ($version eq "+") {
    $other_version = "-";
} else {
    $other_version = "+";
}
my $is_recursive_diff = 0;
my $state = "header";
my $fallback_file;

sub strip {
    my $is_recursive_diff = shift;
    my $f = shift;

    my $effective_strip;
    if (defined $strip) {
        $effective_strip = $strip;
    } else {
        # A "diff -r" or "git diff" adds "diff" lines to
        # the output.  If no such line is present, we have
        # a plain diff between files (not directories), so
        # there should be no need to strip the directory.
        $effective_strip = $is_recursive_diff ? 1 : 0;
    }

    if ($f !~ m{^/}) {
        $f =~ s,^([^/]+/+){$effective_strip},, or fail "directory prefix underflow";
        $f = "$directory/$f";
    }
    return $f;
}

while (<STDIN>) {
    $diff_line++;
    s/^(> )*//g;
    $diff_line_text = $_;
    if (m{^commit (\w+)}) {
        $commit = $1;
        next;
    }
    if (m{^diff\b}) {
        $state = "header";
        $is_recursive_diff = 1;
        if (m{^diff -\S* (\S+) (\S+)$}) {
            $fallback_file = strip $is_recursive_diff, ($version eq "+" ? $2 : $1);
        }
        next;
    }
    if ($state eq "header") {
        if (m{^[$version]{3} ([^\t\n]+)}) {
            $file = strip $is_recursive_diff, $1;
            next;
        }
        if (m{^[$other_version]{3} ([^\t\n]+)}) {
            $other_file = strip $is_recursive_diff, $1;
            next;
        }
    }
    if (m{^@@ -(\d+)(?:,\d+)? \+(\d+)(?:,\d+)? @@}) {
        $state = "contents";
        $file_line = ($version eq "+" ? $2 : $1) - 1;
        $other_file_line = ($version eq "+" ? $1 : $2) - 1;
    } else {
        my $iscontext = m{^[ ]};
        if (m{^[ $version]}) {
           $file_line++ if defined $file_line;
        }
        if (m{^[ $other_version]}) {
           $other_file_line++ if defined $other_file_line;
        }
    }
    if (defined $in_file and defined $file and ($in_file eq "" or $file eq $in_file)) {
        if (defined $in_file_line and defined $file_line and $file_line >= $in_file_line) {
            last;
        }
    }
}
if (not defined $file) {
    $file = ($fallback_file or $other_file);
}
if (not defined $file) {
    fail "missing diff header";
}

eval $end if defined $end;
