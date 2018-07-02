# http://cukes.info
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](feature|story) %{
    set-option buffer filetype cucumber
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/cucumber regions
add-highlighter shared/cucumber/code default-region group
add-highlighter shared/cucumber/language region ^\h*#\h*language: $ group
add-highlighter shared/cucumber/comment  region ^\h*#             $ fill comment

add-highlighter shared/cucumber/language/ fill meta
add-highlighter shared/cucumber/language/ regex \S+$ 0:value

# Spoken languages
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
# https://github.com/cucumber/cucumber/wiki/Spoken-languages
#
# curl --location https://github.com/cucumber/gherkin/raw/master/lib/gherkin/i18n.json
#
# {
#   "en": {
#     "name": "English",
#     "native": "English",
#     "feature": "Feature|Business Need|Ability",
#     "background": "Background",
#     "scenario": "Scenario",
#     "scenario_outline": "Scenario Outline|Scenario Template",
#     "examples": "Examples|Scenarios",
#     "given": "*|Given",
#     "when": "*|When",
#     "then": "*|Then",
#     "and": "*|And",
#     "but": "*|But"
#   },
#   …
# }
#
# jq 'with_entries({ key: .key, value: .value | del(.name) | del(.native) | join("|") })'
#
# {
#   "en": "Feature|Business Need|Ability|Background|Scenario|Scenario Outline|Scenario Template|Examples|Scenarios|*|Given|*|When|*|Then|*|And|*|But",
#   …
# }

add-highlighter shared/cucumber/code/ regex \b(Feature|Business\h+Need|Ability|Background|Scenario|Scenario\h+Outline|Scenario\h+Template|Examples|Scenarios|Given|When|Then|And|But)\b 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden cucumber-filter-around-selections %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden cucumber-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy '#' comment prefix and following white spaces
        try %{ execute-keys -draft k <a-x> s ^\h*\K#\h* <ret> y gh j P }
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : cucumber-filter-around-selections <ret> }
        # indent after lines containing :
        try %{ execute-keys -draft <space> k x <a-k> : <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group cucumber-highlight global WinSetOption filetype=cucumber %{ add-highlighter window/cucumber ref cucumber }

hook global WinSetOption filetype=cucumber %{
    hook window ModeChange insert:.* -group cucumber-hooks  cucumber-filter-around-selections
    hook window InsertChar \n -group cucumber-indent cucumber-indent-on-new-line
}

hook -group cucumber-highlight global WinSetOption filetype=(?!cucumber).* %{ remove-highlighter window/cucumber }

hook global WinSetOption filetype=(?!cucumber).* %{
    remove-hooks window cucumber-indent
    remove-hooks window cucumber-hooks
}
