# Chetan Vardhan Copyright Waiver

# I dedicate any and all copyright interest in this software to the
#  public domain.  I make this dedication for the benefit of the public at
#  large and to the detriment of my heirs and successors.  I intend this
#  dedication to be an overt act of relinquishment in perpetuity of all
#  present and future rights to this software under copyright law.


# http://julialang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](jl) %{
    set-option buffer filetype julia
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=julia %{
    require-module julia
    
    hook window ModeChange pop:insert:.* -group julia-trim-indent julia-trim-indent
    hook window InsertChar .* -group julia-indent julia-indent-on-char
    hook window InsertChar \n -group julia-indent julia-indent-on-new-line
    #hook window InsertChar \n -group julia-insert julia-insert-on-new-line

    alias window alt julia-alternative-file

    hook -once -always window WinSetOption filetype=.* %{
        remove-hooks window julia-.+
        unalias window alt julia-alternative-file
    }
}

hook -group julia-highlight global WinSetOption filetype=julia %{
    add-highlighter window/julia ref julia
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/julia }
}


provide-module julia %[

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/julia regions
add-highlighter shared/julia/code default-region group
add-highlighter shared/julia/string  region '"' (?<!\\)(\\\\)*"         fill string
add-highlighter shared/julia/comment region '#' '$'                     fill comment

# taken from https://github.com/JuliaLang/julia/blob/master/contrib/julia-mode.el
add-highlighter shared/julia/code/ regex %{\b(true|false|C_NULL|Inf|NaN|Inf32|NaN32|nothing|\b-?\d+[fdiu]?)\b} 0:value
add-highlighter shared/julia/code/ regex \b(if|else|elseif|while|for|begin|end|quote|try|catch|return|local|abstract|function|macro|ccall|finally|typealias|break|continue|type|global|module|using|import|export|const|let|bitstype|do|in|baremodule|importall|immutable|mutable|struct)\b 0:keyword
add-highlighter shared/julia/code/ regex \b(Number|Real|BigInt|Integer|UInt|UInt8|UInt16|UInt32|UInt64|UInt128|Int|Int8|Int16|Int32|Int64|Int128|BigFloat|FloatingPoint|Float16|Float32|Float64|Complex128|Complex64|Bool|Cuchar|Cshort|Cushort|Cint|Cuint|Clonglong|Culonglong|Cintmax_t|Cuintmax_t|Cfloat|Cdouble|Cptrdiff_t|Cssize_t|Csize_t|Cchar|Clong|Culong|Cwchar_t|Char|ASCIIString|UTF8String|ByteString|SubString|AbstractString|Array|DArray|AbstractArray|AbstractVector|AbstractMatrix|AbstractSparseMatrix|SubArray|StridedArray|StridedVector|StridedMatrix|VecOrMat|StridedVecOrMat|DenseArray|SparseMatrixCSC|BitArray|Range|OrdinalRange|StepRange|UnitRange|FloatRange|Tuple|NTuple|Vararg|DataType|Symbol|Function|Vector|Matrix|Union|Type|Any|Complex|String|Ptr|Void|Exception|Task|Signed|Unsigned|Associative|Dict|IO|IOStream|Rational|Regex|RegexMatch|Set|IntSet|Expr|WeakRef|ObjectIdDict|AbstractRNG|MersenneTwister)\b 0:type

define-command julia-alternative-file -docstring 'Jump to the alternate file (implementation ↔ test)' %{ evaluate-commands %sh{
    case $kak_buffile in
        *spec/*_spec.jl)
            altfile=$(eval printf %s\\n $(printf %s\\n $kak_buffile | sed s+spec/+'*'/+';'s/_spec//))
            [ ! -f $altfile ] && echo "fail 'implementation file not found'" && exit
        ;;
        *.jl)
            altfile=""
            altdir=""
            path=$kak_buffile
            dirs=$(while [ $path ]; do printf %s\\n $path; path=${path%/*}; done | tail -n +2)
            for dir in $dirs; do
                altdir=$dir/spec
                if [ -d $altdir ]; then
                    altfile=$altdir/$(realpath $kak_buffile --relative-to $dir | sed s+[^/]'*'/++';'s/.jl$/_spec.jl/)
                    break
                fi
            done
            [ ! -d "$altdir" ] && echo "fail 'spec/ not found'" && exit
        ;;
        *)
            echo "fail 'alternative file not found'" && exit
        ;;
    esac
    printf %s\\n "edit $altfile"
}}

define-command -hidden julia-trim-indent %{
    # remove trailing whitespaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden julia-indent-on-char %{
    evaluate-commands -no-hooks -draft -itersel %{
        # align middle and end structures to start and indent when necessary, elseif is already covered by else
        try %{ execute-keys -draft <a-x><a-k>^\h*(else)$<ret><a-semicolon><a-?>^\h*(if)<ret>s\A|.\z<ret>)<a-&> }
        try %{ execute-keys -draft <a-x><a-k>^\h*(end)$<ret><a-semicolon><a-?>^\h*(for|function|if|while)<ret>s\A|.\z<ret>)<a-&> }
    }
}

define-command -hidden julia-indent-on-new-line %{
    evaluate-commands -no-hooks -draft -itersel %{
        # remove trailing white spaces from previous line
        try %{ execute-keys -draft k : julia-trim-indent <ret> }
        # preserve previous non-empty line indent
        try %{ execute-keys -draft <space><a-?>^[^\n]+$<ret>s\A|.\z<ret>)<a-&> }
        # indent after start structure
        try %{ execute-keys -draft <a-?>^[^\n]*\w+[^\n]*$<ret><a-k>^\h*(else|elseif|for|begin|let|module|function|if|while)\b<ret><a-:><semicolon><a-gt> }
    }
}

#uncomment the below if you want and automatic `end`, after your if, else, for, function etc
#define-command -hidden julia-insert-on-new-line %[
#    evaluate-commands -no-hooks -draft -itersel %[
#        # copy -- comment prefix and following white spaces
#        try %{ execute-keys -draft k<a-x>s^\h*\K--\h*<ret>yghjP }
#        # wisely add end structure
#        evaluate-commands -save-regs x %[
#            try %{ execute-keys -draft k<a-x>s^\h+<ret>"xy } catch %{ reg x '' } # Save previous line indent in register x
#            try %[ execute-keys -draft k<a-x> <a-k>^<c-r>x(for|function|if|while)<ret> J}iJ<a-x> <a-K>^<c-r>x(else|end|elseif)$<ret> # Validate previous line and that it is not closed yet
#                   execute-keys -draft o<c-r>xend<esc> ] # auto insert end
#        ]
#    ]
#]

]
