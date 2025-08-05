# http://julialang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.(jl) %{
    set-option buffer filetype julia
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=julia %{
    require-module julia
    hook window InsertChar \n -group julia-indent julia-indent-on-new-line
    hook window InsertChar d -group julia-insert julia-insert-on-new-line
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window julia-.+ }
}

hook -group julia-highlight global WinSetOption filetype=julia %{
    add-highlighter window/julia ref julia
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/julia }
}


provide-module julia %§

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/julia regions
add-highlighter shared/julia/code default-region group
add-highlighter shared/julia/string  region '"' (?<!\\)(\\\\)*"         fill string
add-highlighter shared/julia/comment region -recurse '#=' '#=' '=#'     fill comment
add-highlighter shared/julia/line_comment region '#' '$'                fill comment

# taken from https://github.com/JuliaEditorSupport/julia-emacs/blob/master/julia-mode.el
add-highlighter shared/julia/code/ regex %{\b(true|false|C_NULL|Inf|NaN|Inf32|NaN32|nothing|\b-?\d+[fdiu]?)\b} 0:value
add-highlighter shared/julia/code/ regex \b(if|else|elseif|while|for|begin|end|quote|try|catch|return|local|abstract|function|macro|ccall|finally|typealias|break|continue|type|global|module|using|import|export|const|let|bitstype|do|in|baremodule|importall|immutable|mutable|struct|where)\b 0:keyword
add-highlighter shared/julia/code/ regex \b(Number|Real|BigInt|Integer|UInt|UInt8|UInt16|UInt32|UInt64|UInt128|Int|Int8|Int16|Int32|Int64|Int128|BigFloat|FloatingPoint|Float16|Float32|Float64|Complex128|Complex64|Bool|Cuchar|Cshort|Cushort|Cint|Cuint|Clonglong|Culonglong|Cintmax_t|Cuintmax_t|Cfloat|Cdouble|Cptrdiff_t|Cssize_t|Csize_t|Cchar|Clong|Culong|Cwchar_t|Cvoid|Char|ASCIIString|UTF8String|ByteString|SubString|AbstractString|Array|DArray|AbstractArray|AbstractVector|AbstractMatrix|AbstractSparseMatrix|SubArray|StridedArray|StridedVector|StridedMatrix|VecOrMat|StridedVecOrMat|DenseArray|SparseMatrixCSC|BitArray|Range|OrdinalRange|StepRange|UnitRange|FloatRange|Tuple|NTuple|Vararg|DataType|Symbol|Function|Vector|Matrix|Union|Type|Any|Complex|String|Ptr|Void|Exception|Task|Signed|Unsigned|Associative|Dict|IO|IOStream|Rational|Regex|RegexMatch|Set|IntSet|Expr|WeakRef|ObjectIdDict|AbstractRNG|MersenneTwister)\b 0:type
add-highlighter shared/julia/code/ regex \w+!*(?=\() 0:function
add-highlighter shared/julia/code/ regex @\w+!*\b 0:meta
add-highlighter shared/julia/code/ regex '(?:\?|=|:=|\+=|-=|\*=|/=|//=|\.//=|\.\*=|\./=|\\=|\.\\=|\^=|\.\^=|÷=|\.÷=|%=|\.%=|\|=|&=|\$=|=>|<<=|>>=|>>>=|~|\.\+=|\.-=|--|-->|←|→|↔|↚|↛|↠|↣|↦|↮|⇎|⇏|⇒|⇔|⇴|⇶|⇷|⇸|⇹|⇺|⇻|⇼|⇽|⇾|⇿|⟵|⟶|⟷|⟷|⟹|⟺|⟻|⟼|⟽|⟾|⟿|⤀|⤁|⤂|⤃|⤄|⤅|⤆|⤇|⤌|⤍|⤎|⤏|⤐|⤑|⤔|⤕|⤖|⤗|⤘|⤝|⤞|⤟|⤠|⥄|⥅|⥆|⥇|⥈|⥊|⥋|⥎|⥐|⥒|⥓|⥖|⥗|⥚|⥛|⥞|⥟|⥢|⥤|⥦|⥧|⥨|⥩|⥪|⥫|⥬|⥭|⥰|⧴|⬱|⬰|⬲|⬳|⬴|⬵|⬶|⬷|⬸|⬹|⬺|⬻|⬼|⬽|⬾|⬿|⭀|⭁|⭂|⭃|⭄|⭇|⭈|⭉|⭊|⭋|⭌|￩|￫|&&|\|\||>|<|>=|≥|<=|≤|==|===|≡|!=|≠|!==|≢|\.>|\.<|\.>=|\.≥|\.<=|\.≤|\.==|\.!=|\.≠|\.=|\.!|<:|>:|∈|∉|∋|∌|⊆|⊈|⊂|⊄|⊊|∝|∊|∍|∥|∦|∷|∺|∻|∽|∾|≁|≃|≄|≅|≆|≇|≈|≉|≊|≋|≌|≍|≎|≐|≑|≒|≓|≔|≕|≖|≗|≘|≙|≚|≛|≜|≝|≞|≟|≣|≦|≧|≨|≩|≪|≫|≬|≭|≮|≯|≰|≱|≲|≳|≴|≵|≶|≷|≸|≹|≺|≻|≼|≽|≾|≿|⊀|⊁|⊃|⊅|⊇|⊉|⊋|⊏|⊐|⊑|⊒|⊜|⊩|⊬|⊮|⊰|⊱|⊲|⊳|⊴|⊵|⊶|⊷|⋍|⋐|⋑|⋕|⋖|⋗|⋘|⋙|⋚|⋛|⋜|⋝|⋞|⋟|⋠|⋡|⋢|⋣|⋤|⋥|⋦|⋧|⋨|⋩|⋪|⋫|⋬|⋭|⋲|⋳|⋴|⋵|⋶|⋷|⋸|⋹|⋺|⋻|⋼|⋽|⋾|⋿|⟈|⟉|⟒|⦷|⧀|⧁|⧡|⧣|⧤|⧥|⩦|⩧|⩪|⩫|⩬|⩭|⩮|⩯|⩰|⩱|⩲|⩳|⩴|⩵|⩶|⩷|⩸|⩹|⩺|⩻|⩼|⩽|⩾|⩿|⪀|⪁|⪂|⪃|⪄|⪅|⪆|⪇|⪈|⪉|⪊|⪋|⪌|⪍|⪎|⪏|⪐|⪑|⪒|⪓|⪔|⪕|⪖|⪗|⪘|⪙|⪚|⪛|⪜|⪝|⪞|⪟|⪠|⪡|⪢|⪣|⪤|⪥|⪦|⪧|⪨|⪩|⪪|⪫|⪬|⪭|⪮|⪯|⪰|⪱|⪲|⪳|⪴|⪵|⪶|⪷|⪸|⪹|⪺|⪻|⪼|⪽|⪾|⪿|⫀|⫁|⫂|⫃|⫄|⫅|⫆|⫇|⫈|⫉|⫊|⫋|⫌|⫍|⫎|⫏|⫐|⫑|⫒|⫓|⫔|⫕|⫖|⫗|⫘|⫙|⫷|⫸|⫹|⫺|⊢|⊣|\|>|<\||:|\.\.|\+|-|⊕|⊖|⊞|⊟|\.\+|\.-|\+\+|\||∪|∨|\$|⊔|±|∓|∔|∸|≂|≏|⊎|⊻|⊽|⋎|⋓|⧺|⧻|⨈|⨢|⨣|⨤|⨥|⨦|⨧|⨨|⨩|⨪|⨫|⨬|⨭|⨮|⨹|⨺|⩁|⩂|⩅|⩊|⩌|⩏|⩐|⩒|⩔|⩖|⩗|⩛|⩝|⩡|⩢|⩣|<<|>>|>>>|\.<<|\.>>|\.>>>|\*|/|\./|÷|\.÷|%|⋅|∘|×|\.%|\.\*|\\|\.\\|&|∩|∧|⊗|⊘|⊙|⊚|⊛|⊠|⊡|⊓|∗|∙|∤|⅋|≀|⊼|⋄|⋆|⋇|⋉|⋊|⋋|⋌|⋏|⋒|⟑|⦸|⦼|⦾|⦿|⧶|⧷|⨇|⨰|⨱|⨲|⨳|⨴|⨵|⨶|⨷|⨸|⨻|⨼|⨽|⩀|⩃|⩄|⩋|⩍|⩎|⩑|⩓|⩕|⩘|⩚|⩜|⩞|⩟|⩠|⫛|⊍|▷|⨝|⟕|⟖|⟗|//|\.//|\^|\.\^|↑|↓|⇵|⟰|⟱|⤈|⤉|⤊|⤋|⤒|⤓|⥉|⥌|⥍|⥏|⥑|⥔|⥕|⥘|⥙|⥜|⥝|⥠|⥡|⥣|⥥|⥮|⥯|￪|￬|::|\.)' 0:operator

define-command -hidden julia-indent-on-new-line %<
    evaluate-commands -draft -itersel %<
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # preserve previous line comment
        try %{ execute-keys -draft k x <a-k> ^\h*# <ret> j i <#> <space> }
        # cleanup trailing whitespaces from previous line
        try %{ execute-keys -draft k x s \h+$ <ret> d }
        # indent after block start
        try %{ execute-keys -draft , k x <a-k> ^\h*(struct|macro|function|begin|if|try|for|while|let|quote|do) <ret> <a-K> end$ <ret> j <a-gt> }
        # deindent closing brace/bracket when after cursor (for arrays and dictionaries)
        try %< execute-keys -draft x <a-k> ^\h*[}\]] <ret> gh / [}\]] <ret> m <a-S> 1<a-&> >
    >
>

define-command -hidden julia-insert-on-new-line %<
    evaluate-commands -draft -itersel %<
        # deindent on end
        try %{ execute-keys -draft x <a-k> ^\h*end <ret> <lt> }
    >
>
§
