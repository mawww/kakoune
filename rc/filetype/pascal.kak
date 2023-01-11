# https://www.freepascal.org/docs-html/ref/ref.html
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection, see https://wiki.freepascal.org/file_types
hook global BufCreate .*\.(p|pp|pas|pascal)$ %{
    set-option buffer filetype pascal
}
hook global BufCreate .*\.(dpr|dpk|dfm)$ %{
    set-option buffer filetype delphi
}
hook global BufCreate .*\.(lpr|lfm)$ %{
    set-option buffer filetype freepascal
}

hook global WinSetOption filetype=((free|object)?pascal|delphi) %[
    require-module pascal
    hook window ModeChange pop:insert:.* -group pascal-trim-indent pascal-trim-indent
    hook window InsertChar \n -group pascal-indent pascal-indent-on-new-line
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window pascal-.+ }
    set-option window static_words %opt{static_words}
]

hook -group pascal-highlight global WinSetOption filetype=((free|object)?pascal|delphi) %[
    add-highlighter window/pascal ref pascal
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/pascal }
]

provide-module pascal %§

add-highlighter shared/pascal regions
add-highlighter shared/pascal/code default-region group

evaluate-commands %sh¶
    # This might seem like a lot of effort to highlight routines and
    # properties correctly but it is worth it

    id='([_a-zA-Z][\w]{,126})\s*' # identifier for variables etc.
    id2="(?:$id\.)?$id(?:<.*?>)?" # (module|type).id
    id4="(?:$id\.)?(?:$id\.)?(?:$id\.)?$id" # type.type.type.id
    type=":\s*(specialize\s+)?(?:(array\s+of\s+)?$id2)" # 1:attribute 2:keyword 3:module 4:type

    cat <<EOF
        # routine without parameters
        add-highlighter shared/pascal/code/simple_routine regex \
            "\b(?i)(function|procedure|constructor|destructor|operator)\s+$id4(?:$type)?" \
            2:type 3:type 4:type 5:function 6:attribute 7:keyword 8:module 9:type
        add-highlighter shared/pascal/simple_property region \
            "\b(?i)property\s+$id2:"  ";" group

        # routine with parameters
        add-highlighter shared/pascal/routine region \
            "\b(?i)(function|procedure|constructor|destructor|operator)(\s+$id4)?\s*(<.*?>)?\s*\("  "\).*?;" regions
        add-highlighter shared/pascal/property region \
            "\b(?i)property\s+$id4\["  "\].*?;" regions

        add-highlighter shared/pascal/routine/parameters  region -recurse \( \( \) regions
        add-highlighter shared/pascal/property/parameters region -recurse \[ \[ \] regions
EOF

    # Used to highlight "var1, var2, var3, var4 : type" declarations
    x="(?:$id,\s*)?"

    for r in property routine; do
        cat <<EOF
            add-highlighter shared/pascal/$r/parameters/default default-region group
            add-highlighter shared/pascal/$r/parameters/default/ regex \
                "(?i)(?:(constref|const|var|out|univ)\s+)?$x$x$x$x$x$id(?:$type)?" \
                1:attribute 2:variable 3:variable 4:variable 5:variable 6:variable 7:variable 8:attribute 9:keyword 10:module 11:type
            add-highlighter shared/pascal/$r/default default-region group
EOF
    done

    cat <<EOF
        add-highlighter shared/pascal/routine/default/ regex \
            "\b(?i)(function|procedure|constructor|destructor|operator)(?:\s+$id4)?" \
            1:keyword 2:type 3:type 4:type 5:function
        add-highlighter shared/pascal/routine/default/return_type regex \
            "(?i)$type" 1:attribute 2:keyword 3:module 4:type
        add-highlighter shared/pascal/routine/default/ regex \
            "(?i)(of\s+object|is\s+nested)" 1:keyword
EOF

    for r in property/default simple_property; do
        cat <<EOF
            add-highlighter shared/pascal/$r/ regex "\b(?i)(property)" 1:keyword
            add-highlighter shared/pascal/$r/type regex ":\s*$id" 1:type

            # https://www.freepascal.org/docs-html/ref/refsu33.html
            add-highlighter shared/pascal/$r/specifier regex \
                "\b(?i)(index|read|write|implements|(no)?default|stored)\b" 0:attribute
EOF
    done

    for r in pascal pascal/routine pascal/routine/parameters pascal/property; do
        cat <<EOF
            # Example string: 'You''re welcome!'
            add-highlighter shared/$r/string region \
                -recurse %{(?<!')('')+(?!')} %{'} %{'(?!')|\$} group
            add-highlighter shared/$r/string/ fill string
            add-highlighter shared/$r/string/escape regex "''" 0:+b

            add-highlighter shared/$r/directive region \{\\$[a-zA-Z] \} fill meta

            # comments (https://www.freepascal.org/docs-html/ref/refse2.html)
            add-highlighter shared/$r/comment_old region -recurse \(\* \(\* \*\) fill comment
            add-highlighter shared/$r/comment_new region -recurse \{ \{  \} fill comment
            add-highlighter shared/$r/comment_oneline region // $ fill comment
EOF
    done


    # The types "string" and "file", the value "nil", and the modifiers
    # "bitpacked" and "packed" are not included.
    reserved='and array as asm begin case class const constructor cppclass
              destructor dispinterface div do downto else end except exports
              finalization finally for function goto if implementation in
              inherited initialization interface is label library mod not
              object of operator or otherwise procedure program property raise
              record repeat resourcestring set shl shr then threadvar to try
              type unit until uses var while with xor'

    # These are not reserved words in Free Pascal, but for consistency
    # with other programming languages are highlighted as reserved words:
    keywords='Break Continue Exit at on package'
    # "at" is used in Delphi, e.g. 'raise object at address'

    # https://www.freepascal.org/docs-html/ref/refsu3.html and
    # https://wiki.freepascal.org/Reserved_words
    # "name" and "alias" are not included beacuse they produce too many
    # false positives. Some modifiers like "index" are only highlighted in
    # certain places.
    modifiers='absolute abstract assembler automated bitpacked cdecl contains
               cppdecl cvar default deprecated dispid dynamic enumerator
               experimental export external far far16 final forward generic
               helper inline interrupt iocheck local near noreturn
               nostackframe oldfpccall overload override packed pascal
               platform private protected public published readonly register
               reintroduce requires safecall saveregisters softfloat specialize
               static stdcall strict syscall unaligned unimplemented varargs
               virtual winapi writeonly'

    # https://wiki.freepascal.org/Category:Data_types
    # not highlighted for consistency with not built-in types
    types='AnsiChar AnsiString Boolean Boolean16 Boolean32 Boolean64 Buffer
           Byte ByteBool Cardinal Char Comp Currency Double DWord Extended
           Int16 Int32 Int64 Int8 Integer IUnknown LongBool LongInt Longword
           NativeInt NativeUInt OleVariant PAnsiChar PAnsiString PBoolean PByte
           PByteArray PCardinal PChar PComp PCurrency PDate PDateTime PDouble
           PDWord PExtended PHandle PInt64 PInteger PLongInt PLongWord Pointer
           PPointer PQWord PShortInt PShortString PSingle PSmallInt PString
           PUnicodeChar PUnicodeString PVariant PWideChar PWideString PWord
           PWordArray PWordBool QWord QWordBool RawBytestring Real Real48
           ShortInt ShortString Single SmallInt TClass TDate TDateTime Text
           TextFile THandle TObject TTime UInt16 UInt32 UInt8 UnicodeChar
           UnicodeString UTF16String UTF8String Variant WideChar WideString
           Word WordBool file string'

    constants='False True nil MaxInt Self'

    # Add the language's grammar to the static completion list
    echo declare-option str-list static_words $reserved $keywords $modifiers \
        $types $constants name index result constref out read write implements \
        nodefault stored message

    # Replace spaces with a pipe
    join() { eval set -- $1; IFS='|'; echo "$*"; }

    cat <<EOF
        add-highlighter shared/pascal/code/keywords  regex \
            (?i)(?<!&)\b($(join "$reserved $keywords")|file\s+of)\b 0:keyword
        add-highlighter shared/pascal/code/modifiers regex \
            (?i)(?<!\.)\b($(join "$modifiers"))\b(?!\()|message\s+(?!:) 0:attribute
        add-highlighter shared/pascal/code/index regex \
            '\b(?i)(index)\s+\w+\s*;' 1:attribute
EOF

    for r in code routine/parameters/default routine/default property/default simple_property; do
        cat <<EOF
            add-highlighter shared/pascal/$r/ regex '[.:=<>@^*/+-]' 0:operator
            add-highlighter shared/pascal/$r/constants regex \
                \b(?i)($(join "$constants"))\b 0:value

            # numbers (https://www.freepascal.org/docs-html/ref/refse6.html)
            add-highlighter shared/pascal/$r/decimal regex \b\d+([eE][+-]?\d+)?\b 0:value
            add-highlighter shared/pascal/$r/hex     regex \\\$[\da-fA-F]+\b 0:value
            add-highlighter shared/pascal/$r/octal   regex &[0-7]+\b 0:value
            add-highlighter shared/pascal/$r/binary  regex \%[01]+\b 0:value
            add-highlighter shared/pascal/$r/char    regex '#\d+\b' 0:value
EOF
    done
¶

define-command -hidden pascal-trim-indent %{
    evaluate-commands -no-hooks -draft -itersel %{
        execute-keys x
        # remove trailing white spaces
        try %{ execute-keys -draft s \h + $ <ret> d }
    }
}

define-command -hidden pascal-indent-on-new-line %{
    evaluate-commands -no-hooks -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # cleanup trailing whitespaces from previous line
        try %{ execute-keys -draft k x s \h+$ <ret> d }
        # indent after certain keywords
        try %{ execute-keys -draft kx<a-k>(?i)(asm|begin|const|else|except|exports|finalization|finally|label|of|otherwise|private|property|public|protected|published|record|repeat|resourcestring|threadvar|try|type|uses|var|:)\h*$<ret>j<a-gt> }
    }
}
§

# Other syntax highlighters for reference:
# https://github.com/pygments/pygments/blob/master/pygments/lexers/pascal.py
# https://github.com/codemirror/CodeMirror/blob/master/mode/pascal/pascal.js
# https://github.com/vim/vim/blob/master/runtime/syntax/pascal.vim
# https://github.com/highlightjs/highlight.js/blob/master/src/languages/delphi.js
