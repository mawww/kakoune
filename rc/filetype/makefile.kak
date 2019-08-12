# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*(/?[mM]akefile|\.mk) %{
    set-option buffer filetype makefile
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=makefile %{
    require-module makefile
    require-module sh

    set-option window static_words %opt{makefile_static_words}

    hook window InsertChar \n -group makefile-indent makefile-indent-on-new-line
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window makefile-.+ }
}

hook -group makefile-highlight global WinSetOption filetype=makefile %{
    add-highlighter window/makefile ref makefile
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/makefile }
}

provide-module makefile %{

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/makefile group
add-highlighter shared/makefile_functions group
add-highlighter shared/makefile_recipe group

add-highlighter shared/makefile_functions/regions regions
# Highlight shell scripts in $(shell …) scopes
add-highlighter shared/makefile_functions/regions/ region -recurse '\(' '\$\(shell\s' '\)' ref sh

# Highlight the entire scopes (delimiters + command name) as values
# but highlight the command name as a keyword when it's a special variable, e.g. $(^:.c=.o)
add-highlighter shared/makefile_functions/ regex \$\((?:(%|\*|\+|<|\?|@|^|\|)|([\w_-]*))|\) 0:value 1:keyword

add-highlighter shared/makefile_recipe/regions regions
# Recipes that start with tabs are highlighted as shell
add-highlighter shared/makefile_recipe/regions/ region '^\t+[@-]?' '$' ref sh
# Highlight makefile scopes on top of shell script e.g. @printf %s\\n $(CC)
add-highlighter shared/makefile_recipe/ regex \$\((?:(%|\*|\+|<|\?|@|^|\|)|([\w_-]*))|\) 0:value 1:keyword

# Reference the highlighter groups implemented above in the main highlighter group
add-highlighter shared/makefile/regions regions
add-highlighter shared/makefile/regions/ region -recurse '\(' '\$\(' '\)' ref makefile_functions
add-highlighter shared/makefile/regions/recipe_shell region '^\t+[@-]?' '$' ref makefile_recipe

# Targets
add-highlighter shared/makefile/ regex ^[\w.%-]+\h*:\s 0:variable
# Include statements
add-highlighter shared/makefile/ regex ^[-s]?include\b 0:variable
# Operators used in value assigments
add-highlighter shared/makefile/ regex [+?:]= 0:operator
# Comments
add-highlighter shared/makefile/ regex '#[^\n]*' 0:comment
# Special variables
add-highlighter shared/makefile/ regex \$(%|\*|\+|<|\?|@|^|\|)\s 0:value

evaluate-commands %sh{
    # Grammar
    keywords="ifeq|ifneq|ifdef|ifndef|else|endif|define|endef"
    gnu_functions="abspath|addprefix|addsuffix|and|basename|call|dir|error|eval|file|filter|filter-out|findstring|firstword|flavor|foreach|guile|info|join|lastword|notdir|or|origin|patsubst|realpath|sort|strip|shell|subst|suffix|value|warning|wildcard|word|wordlist|words"

    # Add the language's grammar to the static completion list
    printf %s\\n "declare-option str-list makefile_static_words ${keywords}|${gnu_functions}" | tr '|' ' '

    # Highlight keywords
    printf %s "
        add-highlighter shared/makefile/ regex \b(${keywords})\b 0:keyword
        add-highlighter shared/makefile_functions/ regex \\\$\((${gnu_functions})\b 1:keyword
    "
}

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden makefile-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft \;K<a-&> }
        ## If the line above is a target indent with a tab
        try %{ execute-keys -draft Z k<a-x> <a-k>^[^:]+:\s<ret> z i<tab> }
        # cleanup trailing white space son previous line
        try %{ execute-keys -draft k<a-x> s \h+$ <ret>d }
        # indent after some keywords
        try %{ execute-keys -draft Z k<a-x> <a-k> ^\h*(ifeq|ifneq|ifdef|ifndef|else|define)\b<ret> z <a-gt> }
    }
}

}
