#!/usr/bin/env perl

use strict;
use warnings;
use JSON;

binmode STDOUT, ":encoding(UTF-8)";


sub single_quote {
    my $token = shift;
    $token =~ s/'/''/g;
    return "'$token'";
}
sub double_quote {
    my $token = shift;
    $token =~ s/"/""/g;
    return "\"$token\"";
}
sub escape_regex {
    my $token = shift;
    $token =~ s/([\\^\$.*+?\[\]{}|()])/\\$1/g;
    return $token
}


if (@ARGV != 1) {
    die "Usage: $0 <json_file>\n";
}

my $abbreviations_fname = $ARGV[0];

my $abbreviations_text = do {
    open(my $json_fh, "<", $abbreviations_fname)
        or die("Can't open \"$abbreviations_fname\": $!\n");
    local $/;
    <$json_fh>
};

my $abbreviations = decode_json($abbreviations_text);

# An _abbreviation_ maps from a _label_ to an _expansion_.

# Sort longest first.
my @labels = sort { length($b) <=> length($a) } keys %$abbreviations;
my @labels_escaped = map { escape_regex($_) } @labels;

# A regex matching any label.
my $labels_regex = '(\\\\(' . join('|', @labels_escaped) . ')|.)';

my @label_terminated_regex_parts;
for my $label (@labels) {
    # Labels for which $label is a prefix (excluding $label itself).
    my @label_extensions = grep { $_ ne $label and index($_, $label) == 0 } @labels;

    if (@label_extensions) {
        my %lookaheads;
        for my $extension (@label_extensions) {
            my $next_char = substr($extension, length($label), 1);
            $lookaheads{$next_char} = 1;
        }
        my @lookaheads_escaped = map { '(?!' . escape_regex($_) . ')' } keys %lookaheads;
        my $lookahead_escaped = join('', @lookaheads_escaped);
        push @label_terminated_regex_parts, escape_regex($label) . $lookahead_escaped;
    } else {
        push @label_terminated_regex_parts, escape_regex($label);
    }
}

# A label followed by another character such that the full string is
# not a prefix of any label.
my $label_terminated_regex = '(\\\\(' . join('|', @label_terminated_regex_parts) . '))?.';

print "set-option global lean_abbreviation_full_regex " . single_quote($labels_regex);
print "\n";
print "set-option global lean_abbreviation_terminated_regex " . single_quote($label_terminated_regex);
print "\n";

my @substitute_try_blocks;
while (my ($label, $expansion) = each %$abbreviations) {
    my $block
        = "set-register / " . single_quote('\A\\\\' . escape_regex($label) . '\z') . "\n"
        . "execute-keys '<a-k><ret>'" . "\n"
        . "set-register \\\" " . single_quote($expansion) . "\n";
    push @substitute_try_blocks, $block;
}
my $substitute_command
  = "try "
  . join(" catch ", map { single_quote($_) } @substitute_try_blocks) . "\n"
  . "execute-keys R" . "\n";
print "set-option global lean_abbreviation_substitute_command " . single_quote($substitute_command);
print "\n";
