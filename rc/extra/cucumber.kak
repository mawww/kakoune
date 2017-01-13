# http://cukes.info
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](feature|story) %{
    set buffer filetype cucumber
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default code cucumber \
    language ^\h*#\h*language: $           '' \
    comment  ^\h*#             $           ''

add-highlighter -group /cucumber/language fill meta
add-highlighter -group /cucumber/comment  fill comment

add-highlighter -group /cucumber/language regex \S+$ 0:value

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

add-highlighter -group /cucumber/code regex \b(Feature|Business\h+Need|Ability|Background|Scenario|Scenario\h+Outline|Scenario\h+Template|Examples|Scenarios|Given|When|Then|And|But)\b 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden cucumber-filter-around-selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden cucumber-indent-on-new-line %{
    eval -draft -itersel %{
        # copy '#' comment prefix and following white spaces
        try %{ exec -draft k <a-x> s ^\h*\K#\h* <ret> y gh j P }
        # preserve previous line indent
        try %{ exec -draft \; K <a-&> }
        # filter previous line
        try %{ exec -draft k : cucumber-filter-around-selections <ret> }
        # indent after lines containing :
        try %{ exec -draft <space> k x <a-k> : <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group cucumber-highlight global WinSetOption filetype=cucumber %{ add-highlighter ref cucumber }

hook global WinSetOption filetype=cucumber %{
    hook window InsertEnd  .* -group cucumber-hooks  cucumber-filter-around-selections
    hook window InsertChar \n -group cucumber-indent cucumber-indent-on-new-line
}

hook -group cucumber-highlight global WinSetOption filetype=(?!cucumber).* %{ remove-highlighter cucumber }

hook global WinSetOption filetype=(?!cucumber).* %{
    remove-hooks window cucumber-indent
    remove-hooks window cucumber-hooks
}
