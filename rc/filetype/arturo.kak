# https://arturo-lang.io/documentation/language
#

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.art? %{
    set-option buffer filetype arturo
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=arturo %{
    require-module arturo
    set-option window static_words %opt{arturo_static_words}

    hook window InsertChar \n -group arturo-insert arturo-insert-on-new-line

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window arturo.+ }
}

hook -group arturo-highlight global WinSetOption filetype=arturo %{
    add-highlighter window/arturo ref arturo
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/arturo }
}

provide-module arturo %{

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/arturo regions
add-highlighter shared/arturo/code default-region group
add-highlighter shared/arturo/string region (?<!'\\)" ((?<!\\)(\\\\)*"|$) fill string
add-highlighter shared/arturo/multi_string region \{ \} fill string
add-highlighter shared/arturo/char region ` ` fill string
add-highlighter shared/arturo/string_endfile region  ------ (.|\n)+ fill string
add-highlighter shared/arturo/comment_line region \; $ fill comment

evaluate-commands %sh{
    # Grammar
    declit="\d+"
    intlit="\b(${declit})\b"
    floatlit="\b${declit}\.${declit}\b"
	verlit="\b${declit}\.${declit}\.${declit}[\w-\+\d]*\b"
	colorlit="\b(?<=#)(red|green|blue|[0-9a-fA-F]{6})\b"	
    keywords="abs absolute? accept acos acosh acsec acsech actan actanh add after alert alias all? alphabet and and? angle any? append arg args arity arrange array as ascii? asec asech asin asinh atan atan2 atanh attr attr? attribute? attributeLabel? attrs average before benchmark between? binary? blend block? break browse bytecode? call capitalize case ceil char? chop chunk clamp clear clip close cluster coalesce collect color color? combine compare complex? config conj connect contains? continue copy cos cosh couple crc csec csech ctan ctanh cursor darken database? date? dec decode decouple define delete denominator desaturate deviation dialog dictionary dictionary? difference digest digits disjoint? div divmod do download drop dup else empty empty? encode ensure enumerate env epsilon equal? escape even? every? execute exists? exit exp extend extract factorial factors false false? fdiv filter first flatten floating? floor fold friday? from function function? future? gamma gather gcd get goto grayscale greater? greaterOrEqual? hash hidden? hypot if if? in in? inc indent index infinite infinite? info inline? input insert inspect integer? intersect? intersection invert is? jaro join key? keys kurtosis label? last lcm leap? less? lessOrEqual? let levenshtein lighten list listen literal? ln log logical? loop lower lower? mail map match match? max maximum maybe median min minimum mod module monday? move mul nand nand? neg negative? new nor nor? normalize not not? notEqual? now null null? numerator numeric? object? odd? one? open or or? outdent pad palette panic past? path path? pathLabel? pause permissions permutate pi pop popup positive? pow powerset powmod prefix? prepend prime? print prints process product quantity? query random range range? rational? read receive reciprocal regex? relative remove rename render repeat replace request return reverse rotate round same? sample saturate saturday? script sec sech select send send? serve set set? shl shr shuffle sin sinh size skewness slice some? sort sorted? spin split sqrt squeeze stack standalone? store string? strip sub subset? suffix? sum sunday? superset? superuser? switch symbol? symbolLiteral? symbols symlink sys take tally tan tanh terminal terminate thursday? timestamp to today? translate true true? truncate try try? tuesday? type type? unclip union unique unless unless? unplug until unzip upper upper? values var variance version? volume webview wednesday? when? while whitespace? with word? wordwrap write xnor xnor? xor xor? zero? zip"
    # operators="\+ ∧ \+\+ @ <=> \?\? # / /% <= = // \$ > >= < =< : % \* ⊼ ¬ <> ∨ ^ ∏ \.\. << \./ -- ~ - ∑ \? >> ⊻"
    types="null :logical :integer :floating :complex :rational :version :type :char :string :regex :literal :symbolLiteral :path :pathLabel :inline :block :dictionary :function :color :date :database :binary :bytecode"
    values="false true maybe null ø"

    join() { sep=$2; set -- $1; IFS="$sep"; echo "$*"; }

    static_words="$(join "${keywords} ${types} ${values}" ' ')"

    # Add the language's grammar to the static completion list
    printf %s\\n "declare-option str-list arturo_static_words ${static_words}"

    keywords="$(join "${keywords}" '|')"
    types="$(join "${types}" '|')"
    values="$(join "${values}" '|')"
    # Highlight keywords

    printf %s "
        add-highlighter shared/arturo/code/ regex \W(\+|∧|\+\+|@|<=>|\?\?|#|/|/%|<=|=|//|\Q$\E|>|->|>=|<|=<|:|%|\*|⊼|¬|<>|∨|^|∏|\.\.|<<|\./|--|~|-|∑|\?|>>|⊻) 0:operator
        add-highlighter shared/arturo/code/ regex \b(${keywords})\b 0:builtin
        add-highlighter shared/arturo/code/ regex \b(${types})\b 0:type
        add-highlighter shared/arturo/code/ regex :[\w]+ 0:type 
        add-highlighter shared/arturo/code/ regex [\w+\?!]+: 0:identifier
        add-highlighter shared/arturo/code/ regex \'[\w+\?!]+ 0:identifier
        add-highlighter shared/arturo/code/ regex \b(${values})\b 0:value
        add-highlighter shared/arturo/code/ regex ${intlit} 0:value
        add-highlighter shared/arturo/code/ regex ${floatlit} 0:value
        add-highlighter shared/arturo/code/ regex ${verlit} 0:value
        add-highlighter shared/arturo/code/ regex ${colorlit} 0:value
    "
}

add-highlighter shared/arturo/code/ regex ',' 0:meta

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden arturo-insert-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy ';' comment prefix and following white spaces
        try %{ exec -draft k x s ^\h*<semicolon>\h* <ret> y jgh P }
    }
}

# define-command -hidden arturo-indent-on-new-line %{
#     evaluate-commands -draft -itersel %{
#         # preserve previous line indent
#         try %{ exec -draft <semicolon> K <a-&> }
#         # cleanup trailing whitespaces from previous line
#         try %{ exec -draft k x s \h+$ <ret> d }
#         # indent after line ending with enum, tuple, object, type, import, export, const, let, var, ':' or '='
#         try %{ exec -draft , k x <a-k> (:|=|\b(?:enum|tuple|object|const|let|var|import|export|type))$ <ret> j <a-gt> }
#     }
# }

}
