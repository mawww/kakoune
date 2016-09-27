# http://cukes.info
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-cucumber %{
    set buffer filetype cucumber
}

hook global BufCreate .*[.](feature|story) %{
    set buffer filetype cucumber
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code cucumber \
    language ^\h*#\h*language: $           '' \
    comment  ^\h*#             $           ''

addhl -group /cucumber/language fill meta
addhl -group /cucumber/comment  fill comment

addhl -group /cucumber/language regex \S+$ 0:value

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

addhl -group /cucumber/code regex \b(Feature|Business\h+Need|Ability|Background|Scenario|Scenario\h+Outline|Scenario\h+Template|Examples|Scenarios|Given|When|Then|And|But)\b 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _cucumber_filter_around_selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden _cucumber_indent_on_new_line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # filter previous line
        try %{ exec -draft k : _cucumber_filter_around_selections <ret> }
        # copy '#' comment prefix and following white spaces
        try %{ exec -draft k x s ^\h*\K#\h* <ret> y j p }
        # indent after lines containing :
        try %{ exec -draft <space> k x <a-k> : <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group cucumber-highlight global WinSetOption filetype=cucumber %{ addhl ref cucumber }

hook global WinSetOption filetype=cucumber %{
    hook window InsertEnd  .* -group cucumber-hooks  _cucumber_filter_around_selections
    hook window InsertChar \n -group cucumber-indent _cucumber_indent_on_new_line
}

hook global WinSetOption filetype=(?!cucumber).* %{
    rmhl cucumber
    rmhooks window cucumber-indent
    rmhooks window cucumber-hooks
}
