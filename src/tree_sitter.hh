#ifndef tree_sitter_hh_INCLUDED
#define tree_sitter_hh_INCLUDED

#include "highlighter.hh"

namespace Kakoune
{

const HighlighterDesc tree_sitter_injection_desc = {
    "Parameters: <language> <type> <params>...\n"
    "Apply the given delegate highlighter as defined by <type> and <params>\n"
    "to tree-sitter injection.content nodes where the injection.language is\n"
    "<language>.",
    {}
};
std::unique_ptr<Highlighter> create_tree_sitter_injection_highlighter(HighlighterParameters params, Highlighter* parent);

const HighlighterDesc tree_sitter_desc = {
    "Parameters: <language> <id>:<face> <id>:<face>...\n"
    "Highlight the tree-sitter nodes by id with the given faces"
    "The ids will be sorted and matched by longest prefix.",
    {}
};
std::unique_ptr<Highlighter> create_tree_sitter_highlighter(HighlighterParameters params, Highlighter* parent);

}

#endif // tree_sitter_hh_INCLUDED
