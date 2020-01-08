# https://palletsprojects.com/p/jinja/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

provide-module jinja %[

require-module python

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/jinja regions
add-highlighter shared/jinja/comment region '\{#' '#\}' fill comment

# TODO: line statements # …

add-highlighter shared/jinja/statement region '\{%' '%\}' group
add-highlighter shared/jinja/statement/ ref python
add-highlighter shared/jinja/statement/ regex \{%[+-]?|[+-]?%\} 0:value
add-highlighter shared/jinja/statement/tests regex \b(callable|even|le|none|string|defined|ge|lower|number|undefined|divisibleby|gt|lt|odd|upper|eq|in|mapping|sameas|escaped|iterable|ne|sequence)\b 0:builtin
add-highlighter shared/jinja/statement/functions regex \b(range|lipsum)\b 0:function
add-highlighter shared/jinja/statement/macro regex \b(((end)?(call|macro)))\b 0:keyword
add-highlighter shared/jinja/statement/extensions regex \b(((end)?(block|trans))|(pluralize))\b 0:keyword
add-highlighter shared/jinja/statement/control regex \b(((end)?(if|for|with))|(break|continue))\b 0:keyword
add-highlighter shared/jinja/statement/filters regex \b(?:(?:(filter)\s+|\|\s*)(abs|attr|batch|capitalize|center|default|dictsort|e|escape|filesizeformat|first|float|forceescape|format|groupby|indent|int|join|last|length|list|lower|map|max|min|pprint|random|reject|rejectattr|replace|reverse|round|safe|select|selectattr|slice|sort|string|striptags|sum|title|tojson|trim|truncate|unique|upper|urlencode|urlize|wordcount|wordwrap|xmlattr)|(endfilter))\b 1:keyword 3:keyword 2:builtin
add-highlighter shared/jinja/statement/ regex \b((end)?(autoescape|raw|set))\b 0:keyword
add-highlighter shared/jinja/statement/ regex \b(do|extends|include)\b 0:keyword
add-highlighter shared/jinja/statement/ regex \bignore\s+missing\b 0:meta
add-highlighter shared/jinja/statement/ regex \bwith(out)?\s+context\b 0:meta

add-highlighter shared/jinja/expression region '\{\{' '\}\}' group
add-highlighter shared/jinja/expression/ ref python
add-highlighter shared/jinja/expression/ regex \{\{|\}\} 0:value
add-highlighter shared/jinja/expression/filters regex \|\s*(abs|attr|batch|capitalize|center|default|dictsort|e|escape|filesizeformat|first|float|forceescape|format|groupby|indent|int|join|last|length|list|lower|map|max|min|pprint|random|reject|rejectattr|replace|reverse|round|safe|select|selectattr|slice|sort|string|striptags|sum|title|tojson|trim|truncate|unique|upper|urlencode|urlize|wordcount|wordwrap|xmlattr)\b 1:builtin

]
