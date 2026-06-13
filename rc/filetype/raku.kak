# https://raku.org/

# Detection

hook global BufCreate .*\.(raku|rakumod|rakutest|rakudoc|p6|pm6|t6|pod6)$ %{
    set-option buffer filetype raku
}

# Initialization

hook global WinSetOption filetype=raku %{
    require-module raku
    set-option window static_words %opt{raku_static_words}
}

hook -group raku-highlight global WinSetOption filetype=raku %{
    add-highlighter window/raku ref raku
    hook -once -always window WinSetOption filetype=.* %{
        remove-highlighter window/raku
    }
}

provide-module raku %§

# Highlighters

add-highlighter shared/raku regions
add-highlighter shared/raku/code default-region group

# Pod and comments
add-highlighter shared/raku/pod-block region -match-capture %{^=begin\h+([\w'-]+)} %{^=end\h+([\w'-]+)} fill comment
add-highlighter shared/raku/pod-paragraph region %{^=(?:for|head\d*|item\d*|para|comment|defn|config)\b} %{^\h*$} fill comment
add-highlighter shared/raku/comment-paren region -recurse \( %{\Q#`(} \) fill comment
add-highlighter shared/raku/comment-brace region -recurse \{ %/\Q#`{/ \} fill comment
add-highlighter shared/raku/comment-bracket region -recurse \[ %{\Q#`[} \] fill comment
add-highlighter shared/raku/comment-angle region -recurse < %{\Q#`<} > fill comment
add-highlighter shared/raku/comment region '#' $ fill comment

# Strings, commands, and heredocs
add-highlighter shared/raku/double-string region '"' %{(?<!\\)(\\\\)*"} fill string
add-highlighter shared/raku/single-string region "'" %{(?<!\\)(\\\\)*'} fill string
add-highlighter shared/raku/command region %{(?<!\\)`} %{(?<!\\)(\\\\)*`} fill meta
add-highlighter shared/raku/heredoc-slash region -match-capture %{\b(?:q|qq|Q|qw|qx):to\h*/([\w'-]+)/} %{^\h*([\w'-]+)$} fill string
add-highlighter shared/raku/heredoc-angle region -match-capture %{\b(?:q|qq|Q|qw|qx):to\h*<([\w'-]+)>} %{^\h*([\w'-]+)$} fill string

# Quote and regex constructs with paired delimiters
add-highlighter shared/raku/quote-paren region -recurse \( %{\b(?:q|qq|Q|qw|qx)\h*\(} \) fill string
add-highlighter shared/raku/quote-brace region -recurse \{ %/\b(?:q|qq|Q|qw|qx)\h*\{/ \} fill string
add-highlighter shared/raku/quote-bracket region -recurse \[ %{\b(?:q|qq|Q|qw|qx)\h*\[} \] fill string
add-highlighter shared/raku/quote-angle region -recurse < %{\b(?:q|qq|Q|qw|qx)\h*<} > fill string
add-highlighter shared/raku/quote-punct region -match-capture %{\b(?:q|qq|Q|qw|qx)([:;!@#$%^&*|,.?/~=+-])} (.) fill string

# Recursive regions cannot reliably count Raku regex delimiters because
# character classes use those same delimiters.
add-highlighter shared/raku/regex-bracket-multiline region %{\b(?:rx|m|ms|s|ss|tr)\h*\[\h*$} %{^\h*\]} fill meta
add-highlighter shared/raku/regex-slash region %{\b(?:rx|m|ms)\h*/} %{(?<!\\)(\\\\)*/\w*} fill meta
add-highlighter shared/raku/code/ regex %{\b(?:rx|m|ms|s|ss|tr)\h*(?:\([^\n)]*\)|[<][^>\n]*[>]|\[[^\n]*\])} 0:meta
add-highlighter shared/raku/code/ regex %/\b(?:rx|m|ms|s|ss|tr)\h*\{[^\n}]*\}/ 0:meta

# Grammar
add-highlighter shared/raku/code/ regex %{\b(?:also|and|andthen|anon|augment|but|catch|class|constant|default|do|else|elsif|enum|equiv|for|gather|given|grammar|if|is|lazy|leave|loop|macro|make|method|module|multi|my|need|next|not|or|orelse|our|proceed|proto|redo|regex|repeat|require|return|role|rule|state|sub|submethod|subset|succeed|take|temp|token|trust|try|unit|unless|until|use|when|whenever|while|will|without|xor)\b} 0:keyword
add-highlighter shared/raku/code/ regex %{\b(?:BEGIN|CHECK|INIT|END|ENTER|LEAVE|KEEP|UNDO|FIRST|NEXT|LAST|PRE|POST|CATCH|CONTROL|QUIT)\b} 0:keyword
add-highlighter shared/raku/code/ regex %{\b(?:method|multi|sub|submethod|macro|regex|rule|token)\h+([\w'-]+)} 1:function
add-highlighter shared/raku/code/ regex %{\b(?:class|grammar|module|package|role|subset|unit)\h+([\w'-]+(?:::[\w'-]+)*)} 1:module
add-highlighter shared/raku/code/ regex %{\b(?:Any|Array|Associative|Bag|BagHash|Blob|Bool|Buf|Callable|Capture|Complex|Cool|Date|DateTime|Duration|Exception|FatRat|Hash|Instant|Int|IntStr|IO|Junction|List|Map|Match|Mix|MixHash|Mu|Nil|Numeric|Pair|Positional|Promise|Range|Rat|RatStr|Real|Regex|Routine|Scalar|Seq|Set|SetHash|Signature|Slip|Str|StrDistance|Supply|UInt|Version)\b} 0:type
add-highlighter shared/raku/code/ regex %{\b(?:True|False|Nil|Empty|Failure|Inf|NaN|Whatever|now|pi|tau|e)\b} 0:value
add-highlighter shared/raku/code/ regex %{(?:^|[^\w'-])\K(?:\d[\d_]*(?:\.\d[\d_]*)?(?:[eE][+-]?\d[\d_]*)?|0[xX][0-9a-fA-F_]+|0[bB][01_]+|0[oO][0-7_]+|:[0-9]+<[^>]+>)\b} 0:value

# Variables, named arguments, types, and calls
add-highlighter shared/raku/code/ regex %{(?:[$@%&](?:[.!*?^:=~])?(?:[\w'-]+(?:::[\w'-]+)*|\d+|[/!]|<[\w'-]+>))} 0:variable
add-highlighter shared/raku/code/ regex %{::[?*]?[\w'-]+(?:::[\w'-]+)*} 0:module
add-highlighter shared/raku/code/ regex %{:[\w'-]+} 0:attribute
add-highlighter shared/raku/code/ regex %{\b([\w'-]+)\h*\(} 1:function
add-highlighter shared/raku/code/ regex %{\b(?:does|handles|has|of|returns|where)\b} 0:attribute
add-highlighter shared/raku/code/ regex %{\b(?:so|not|div|mod|gcd|lcm|leg|cmp|eqv|before|after|xx|x|Z|X)\b|(?:\.\.?|\^\.\.?|\.\^\.\.?|\^\.\^\.\.|\?\?|//|==|!=|===|=:=|~~|!~~|=>|==>|<==|->|<->|<=>|[+*~&|^]=)} 0:operator

declare-option str-list raku_static_words \
    my our state temp has constant class role grammar module package subset enum \
    method multi proto sub submethod regex rule token macro \
    if elsif else unless with without given when default for loop while until repeat \
    try catch control return take gather whenever react supply use need require unit \
    True False Nil Empty Failure Any Mu Int UInt Rat Num Str Bool Array Hash List Map \
    Pair Set Bag Seq Range Regex Match Promise Supply IO Date DateTime

§
