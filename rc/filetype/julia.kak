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
}

hook -group julia-highlight global WinSetOption filetype=julia %{
    add-highlighter window/julia ref julia
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/julia }
}


provide-module julia %{

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/julia regions
add-highlighter shared/julia/code default-region group
add-highlighter shared/julia/string  region '"' (?<!\\)(\\\\)*"         fill string
add-highlighter shared/julia/comment region -recurse '#=' '#=' '=#'     fill comment
add-highlighter shared/julia/line_comment region '#' '$'                fill comment

# taken from https://github.com/JuliaLang/julia/blob/master/contrib/julia-mode.el
add-highlighter shared/julia/code/ regex %{\b(true|false|C_NULL|Inf|NaN|Inf32|NaN32|nothing|\b-?\d+[fdiu]?)\b} 0:value
add-highlighter shared/julia/code/ regex \b(if|else|elseif|while|for|begin|end|quote|try|catch|return|local|abstract|function|macro|ccall|finally|typealias|break|continue|type|global|module|using|import|export|const|let|bitstype|do|in|baremodule|importall|immutable|mutable|struct)\b 0:keyword
add-highlighter shared/julia/code/ regex \b(Number|Real|BigInt|Integer|UInt|UInt8|UInt16|UInt32|UInt64|UInt128|Int|Int8|Int16|Int32|Int64|Int128|BigFloat|FloatingPoint|Float16|Float32|Float64|Complex128|Complex64|Bool|Cuchar|Cshort|Cushort|Cint|Cuint|Clonglong|Culonglong|Cintmax_t|Cuintmax_t|Cfloat|Cdouble|Cptrdiff_t|Cssize_t|Csize_t|Cchar|Clong|Culong|Cwchar_t|Char|ASCIIString|UTF8String|ByteString|SubString|AbstractString|Array|DArray|AbstractArray|AbstractVector|AbstractMatrix|AbstractSparseMatrix|SubArray|StridedArray|StridedVector|StridedMatrix|VecOrMat|StridedVecOrMat|DenseArray|SparseMatrixCSC|BitArray|Range|OrdinalRange|StepRange|UnitRange|FloatRange|Tuple|NTuple|Vararg|DataType|Symbol|Function|Vector|Matrix|Union|Type|Any|Complex|String|Ptr|Void|Exception|Task|Signed|Unsigned|Associative|Dict|IO|IOStream|Rational|Regex|RegexMatch|Set|IntSet|Expr|WeakRef|ObjectIdDict|AbstractRNG|MersenneTwister)\b 0:type

}
