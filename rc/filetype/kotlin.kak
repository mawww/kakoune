# References --------------------------------------------------------------------------------------- #
# ‾‾‾‾‾‾‾‾‾‾
# Team: Yerlan & Kirk Duncan
#
# Kotlin 2020, Keywords and Operators, v1.4.0, viewed 9 September 2020, https://kotlinlang.org/docs/reference/keyword-reference.html
# Kdoc 2020, Documenting Kotlin Code, Block Tags, v1.4.0, viewed 9 September 2020, https://kotlinlang.org/docs/reference/kotlin-doc.html
# Oracle 2020, Java Platform, Standard Edition & Java Development Kit, Version 14 API Specification, viewed 8 September 2020, https://docs.oracle.com/en/java/javase/14/docs/api/index.html
#
# File types --------------------------------------------------------------------------------------- #
# ‾‾‾‾‾‾‾‾‾‾
hook global BufCreate .*[.](kt|kts)  %{
  set-option buffer filetype kotlin
}

# Initialization ----------------------------------------------------------------------------------- #
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾
hook global WinSetOption filetype=kotlin %{
  require-module kotlin

  set-option window static_words %opt{kotlin_static_words}

  # cleanup trailing whitespaces when exiting insert mode
  hook window ModeChange pop:insert:.* -group kotlin-trim-indent %{ try %{ execute-keys -draft xs^\h+$<ret>d } }
  hook window InsertChar \n -group kotlin-indent kotlin-insert-on-new-line
  hook window InsertChar \n -group kotlin-indent kotlin-indent-on-new-line
  hook window InsertChar \{ -group kotlin-indent kotlin-indent-on-opening-curly-brace
  hook window InsertChar \} -group kotlin-indent kotlin-indent-on-closing-curly-brace

  hook -once -always window WinSetOption filetype=.* %{ remove-hooks window kotlin-.+ }
}

hook -group kotlin-highlighter global WinSetOption filetype=kotlin %{
  add-highlighter window/kotlin ref kotlin
  add-highlighter window/kdoc ref kdoc

  hook -once -always window WinSetOption filetype=.* %{
    remove-highlighter window/kotlin
    remove-highlighter window/kdoc
  }
}

hook global BufSetOption filetype=kotlin %{
  require-module kotlin

  set-option buffer comment_line '//'
  set-option buffer comment_block_begin '/*'
  set-option buffer comment_block_end '*/'

  hook -once -always buffer BufSetOption filetype=.* %{ remove-hooks buffer kotlin-.+ }
}

# Module ------------------------------------------------------------------------------------------- #
# ‾‾‾‾‾‾
provide-module kotlin %§

add-highlighter shared/kotlin regions
add-highlighter shared/kotlin/code default-region group
add-highlighter shared/kotlin/string region %{(?<!')"} %{(?<!\\)(\\\\)*"} group
add-highlighter shared/kotlin/character region %{'} %{(?<!\\)'} group
add-highlighter shared/kotlin/comment region /\* \*/ fill comment
add-highlighter shared/kotlin/inline_documentation region /// $ fill documentation
add-highlighter shared/kotlin/line_comment region // $ fill comment

add-highlighter shared/kotlin/code/annotations regex @\w+\b|\b\w+@(?=\{) 0:meta
add-highlighter shared/kotlin/code/identifiers regex \b(field|it)\b 1:variable
add-highlighter shared/kotlin/code/fields      regex \.([A-Za-z_][\w]*)\s*?\. 1:type

# String interpolation
add-highlighter shared/kotlin/string/ fill string
add-highlighter shared/kotlin/string/ regex \$\{.*?\} 0:value

# Character
add-highlighter shared/kotlin/character/ fill value
add-highlighter shared/kotlin/character/ regex ('.{1})(.+)(') 2:meta

# As at 15 March 2021, method see: https://regex101.com/r/Mhy4HG/1
add-highlighter shared/kotlin/code/methods     regex ::([A-Za-z_][\w]*)|\.([A-Za-z_][\w]*)\s*?[\(\{]|\.([A-Za-z_][\w]*)[\s\)\}>](?=[^\(\{]) 1:function 2:function 3:function

# Test suite functions: fun `this is a valid character function test`()
add-highlighter shared/kotlin/code/fun_tests   regex ^\h*?fun\s*?`(.[^<>:/\[\]\\\.]+?)`\h*?(?=\() 1:default+iuf
add-highlighter shared/kotlin/code/delimiters  regex (\(|\)|\[|\]|\{|\}|\;|') 1:operator
add-highlighter shared/kotlin/code/operators   regex (\+|-|\*|&|=|\\|\?|%|\|-|!|\||->|\.|,|<|>|:|\^|/) 1:operator
add-highlighter shared/kotlin/code/numbers     regex \b((0(x|X)[0-9a-fA-F]*)|(([0-9]+\.?[0-9]*)|(\.[0-9]+))((e|E)(\+|-)?[0-9]+)?)([LlFf])?\b 0:value

# Generics need improvement, as after a colon will match as a constant only.
# val program: IOU = XXXX; val cat: DOG = XXXX. matches IOU or DOG as a
# CONSTANT when it could be generics. See: https://regex101.com/r/VPO5LE/10
add-highlighter shared/kotlin/code/constants_and_generics regex \b((?<==\h)\([A-Z][A-Z0-9_]+(?=[<:\;])|(?<!<)[A-Z][A-Z0-9_]+\b(?!<[>\)]))|\b((?<!=\s)(?<!\.)[A-Z]+\d*?(?![\(\;:])(?=[,\)>\s]))\b 1:meta 2:type

add-highlighter shared/kotlin/code/target regex @(delegate|field|file|get|param|property|receiver|set|setparam)(?=:) 0:meta
add-highlighter shared/kotlin/code/soft   regex \b(by|catch|constructor|dynamic|finally|get|import|init|set|where)\b 1:keyword
add-highlighter shared/kotlin/code/hard   regex \b(as|as\?|break|class|continue|do|else|false|for|fun|if|in|!in|interface|is|!is|null|object|package|return|super|this|throw|true|try|typealias|val|var|when|while)\b 1:keyword

add-highlighter shared/kotlin/code/modifier regex \b(actual|abstract|annotation|companion|const|crossinline|data|enum|expect|external|final|infix|inline|inner|internal|lateinit|noinline|open|operator|out|override|private|protected|public|reified|sealed|suspend|tailrec|vararg)\b(?=[\s\n]) 1:attribute

add-highlighter shared/kotlin/code/type regex \b(Annotation|Any|Boolean|BooleanArray|Byte|ByteArray|Char|Character|CharArray|CharSequence|Class|ClassLoader|Cloneable|Comparable|Compiler|DeprecationLevel|Double|DoubleArray|Enum|Float|FloatArray|Function|Int|IntArray|Integer|Lazy|LazyThreadSafetyMode|Long|LongArray|Math|Nothing|Number|Object|Package|Pair|Process|Runnable|Runtime|SecurityManager|Short|ShortArray|StackTraceElement|StrictMath|String|StringBuffer|System|Thread|ThreadGroup|ThreadLocal|Triple|Unit|Void)\b(?=[^<]) 1:type

# Kdoc --------------------------------------------------------------------------------------------- #
# ‾‾‾‾
add-highlighter shared/kdoc group
add-highlighter shared/kdoc/tag regex \*(?:\s+)?(@(author|constructor|exception|param|property|receiver|return|sample|see|since|suppress|throws))\b 1:default+ui

# Discolour ---------------------------------------------------------------------------------------- #
# ‾‾‾‾‾‾‾‾‾
add-highlighter shared/kotlin/code/discolour regex ^(package|import)(?S)(.+) 2:default+fa

# Commands ----------------------------------------------------------------------------------------- #
# ‾‾‾‾‾‾‾‾
define-command -hidden kotlin-insert-on-new-line %[
  # copy // comments prefix and following white spaces
  try %{ execute-keys -draft <semicolon><c-s>kx s ^\h*\K/{2,}\h* <ret> y<c-o>P<esc> }
]

define-command -hidden kotlin-indent-on-new-line %~
  evaluate-commands -draft -itersel %<
    # preserve previous line indent
    try %{ execute-keys -draft <semicolon>K<a-&> }
    # indent after lines ending with { or (
    try %[ execute-keys -draft kx <a-k> [{(]\h*$ <ret> j<a-gt> ]
    # cleanup trailing white spaces on the previous line
    try %{ execute-keys -draft kx s \h+$ <ret>d }
    # align to opening paren of previous line
    try %{ execute-keys -draft [( <a-k> \A\([^\n]+\n[^\n]*\n?\z <ret> s \A\(\h*.|.\z <ret> '<a-;>' & }
    # indent after a pattern match on when/where statements
    try %[ execute-keys -draft kx <a-k> ^\h*(when|where).*$ <ret> j<a-gt> ]
    # indent after term on an expression
    try %[ execute-keys -draft kx <a-k> =\h*?$ <ret> j<a-gt> ]
    # indent after keywords
    try %[ execute-keys -draft <semicolon><a-F>)MB <a-k> \A(catch|do|else|for|if|try|while)\h*\(.*\)\h*\n\h*\n?\z <ret> s \A|.\z <ret> 1<a-&>1<a-,><a-gt> ]
    # deindent closing brace(s) when after cursor
    try %[ execute-keys -draft x <a-k> ^\h*[})] <ret> gh / [})] <ret> m <a-S> 1<a-&> ]
  >
~

define-command -hidden kotlin-indent-on-opening-curly-brace %[
  # align indent with opening paren when { is entered on a new line after the closing paren
  try %[ execute-keys -draft -itersel h<a-F>)M <a-k> \A\(.*\)\h*\n\h*\{\z <ret> s \A|.\z <ret> 1<a-&> ]
]

define-command -hidden kotlin-indent-on-closing-curly-brace %[
  # align to opening curly brace when alone on a line
  try %[ execute-keys -itersel -draft <a-h><a-k>^\h+\}$<ret>hms\A|.\z<ret>1<a-&> ]
]

# Exceptions, Errors, and Types -------------------------------------------------------------------- #
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
# macro: 93le<a-;>i<ret><esc><esc>
evaluate-commands %sh{

  kotlin_keywords='abstract actual annotation as break by catch class companion const
    constructor continue crossinline data delegate do dynamic else enum expect external
    false field file final finally for fun get if import in infix init inline inner
    interface internal is lateinit noinline null object open operator out override
    package param private property protected public receiver reified return sealed set
    setparam super suspend tailrec this throw true try typealias val var vararg when where while'

  kotlin_types='Annotation Any Boolean BooleanArray Byte ByteArray Char Character CharArray
    CharSequence Class ClassLoader Cloneable Comparable Compiler DeprecationLevel Double
    DoubleArray Enum Float FloatArray Function Int IntArray Integer Lazy LazyThreadSafetyMode
    Long LongArray Math Nothing Number Object Package Pair Process Runnable Runtime
    SecurityManager Short ShortArray StackTraceElement StrictMath String StringBuffer System
    Thread ThreadGroup ThreadLocal Triple Unit Void'

  # ------------------------------------------------------------------------------------------------ #

  kotlin_kdocs='author constructor exception param property receiver return sample see since suppress throws'

  # ------------------------------------------------------------------------------------------------ #

  kotlin_errors_exceptions='CharacterCodingException Error AssertionError NotImplementedError
    OutOfMemoryErrorIllegalCallableAccessException IllegalPropertyDelegateAccessException
    NoSuchPropertyException RuntimeException Throwable'

  # ------------------------------------------------------------------------------------------------ #

  join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

  # ------------------------------------------------------------------------------------------------ #

  printf %s\\n "declare-option str-list kotlin_static_words $(join "${kotlin_keywords} ${kotlin_types} ${kotlin_kdocs} ${kotlin_errors_exceptions}" ' ')"

  # ------------------------------------------------------------------------------------------------ #

  printf %s\\n "add-highlighter shared/kotlin/code/errors_exceptions regex \b($(join "${kotlin_errors_exceptions}" '|'))\b 0:type"
}
§
# ------------------------------------------------------------------------------------------------- #
