#!/usr/bin/env perl

use strict;
use warnings;

my $print_remaining = 0;
if ($ARGV[0] eq "-print-remaining-diff") {
    $print_remaining = 1;
    shift @ARGV;
}

my $line_number_kind = "diff";
if ($ARGV[0] eq "-line-numbers-from-new-file") {
    $line_number_kind = "new-file";
    shift @ARGV;
}

my $min_line = $ARGV[0];
shift @ARGV;
my $max_line = $ARGV[0];
shift @ARGV;

my $patch_cmd;
if (defined $ARGV[0] and $ARGV[0] =~ m{^[^-]}) {
    $patch_cmd = "@ARGV";
} else {
    $patch_cmd = "patch @ARGV";
}
my $reverse = grep /^(--reverse|-R)$/, @ARGV;

my $lineno = $line_number_kind eq "diff" ? 0 : undef;
my $original = "";
my $diff_header = "";
my $wheat = "";
my $chaff = "" if $print_remaining;
my $state = undef;
my $hunk_wheat = undef;
my $hunk_chaff = undef if $print_remaining;
my $hunk_header = undef;
my $hunk_remaining_lines = undef;
my $signature = "" if $print_remaining;

sub compute_hunk_header {
    my $original_header = shift;
    my $hunk = shift;
    my $old_lines = 0;
    my $new_lines = 0;
    for (split /\n/, $hunk) {
        $old_lines++ if m{^[ -]};
        $new_lines++ if m{^[ +]};
    }
    my $updated_header = $original_header =~ s/^@@ -(\d+),\d+\s+\+(\d+),\d+ @@(.*)/@@ -$1,$old_lines +$2,$new_lines @\@$3/mr;
    return $updated_header;
}

sub finish_hunk {
    return unless defined $hunk_header;
    if ($hunk_wheat =~ m{^[-+]}m) {
        if ($diff_header) {
            $wheat .= $diff_header;
            $diff_header = "";
        }
        $wheat .= (compute_hunk_header $hunk_header, $hunk_wheat). $hunk_wheat;
    }
    if ($print_remaining) {
        $chaff .= (compute_hunk_header $hunk_header, $hunk_chaff) . $hunk_chaff . $signature;
    }
    $hunk_header = undef;
}

while (<STDIN>) {
    ++$lineno if $line_number_kind eq "diff";
    $original .= $_;
    if (m{^diff} || (not defined $state and m{^---})) {
        finish_hunk();
        $state = "diff header";
        $diff_header = "";
    }
    if ($state eq "signature") {
        $signature .= $_ if $print_remaining;
        next;
    }
    if (m{^@@ -\d+(?:,(\d)+)? \+(\d+)(?:,\d+)? @@}) {
        $lineno = $2 - 1 if $line_number_kind eq "new-file";
        $hunk_remaining_lines = $1 or 1;
        finish_hunk();
        $state = "diff hunk";
        $hunk_header = $_;
        $hunk_wheat = "";
        if ($print_remaining) {
            $hunk_chaff = "";
            $signature = "";
        }
        next;
    }
    if ($state eq "diff header") {
        $diff_header .= $_;
        $chaff .= $_ if $print_remaining;
        next;
    }
    if ($hunk_remaining_lines == 0 and m{^-- $}) {
        $state = "signature";
        $signature .= $_ if $print_remaining;
        next;
    }
    ++$lineno if $line_number_kind eq "new-file" && m{^[ +]};
    --$hunk_remaining_lines if m{^[ -]};
    my $include = m{^ } ||
        ($lineno >= $min_line && $lineno <= $max_line) ||
        ($line_number_kind eq "new-file" && m{^-} && $lineno == $min_line - 1);
    if ($include) {
        $hunk_wheat .= $_;
        if ($print_remaining) {
            $hunk_chaff .= $_ if m{^ };
            if ($reverse ? m{^[-]} : m{^\+}) {
                $hunk_chaff .= " " . substr $_, 1;
            }
        }
    } else {
        if ($reverse ? m{^\+} : m{^-}) {
            $hunk_wheat .= " " . substr $_, 1;
        }
        $hunk_chaff .= $_ if $print_remaining;
    }
}
finish_hunk();

open PATCH_COMMAND, "|-", "$patch_cmd" or die "patch-range.pl: error running '$patch_cmd': $!";
print PATCH_COMMAND $wheat;
if (not close PATCH_COMMAND) {
    print $original;
    print STDERR "patch-range.pl: error running:\n" . "\$ $patch_cmd << EOF\n$wheat" . "EOF\n";
    exit 1;
}
print $chaff if $print_remaining;
