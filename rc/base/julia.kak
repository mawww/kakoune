# http://julialang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.(jl) %{
    set buffer filetype julia
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code julia \
    string  '"' (?<!\\)(\\\\)*"         '' \
    comment '#' '$'                     ''

addhl -group /julia/string fill string
addhl -group /julia/comment fill comment

# taken from https://github.com/JuliaLang/julia/blob/master/contrib/julia-mode.el
addhl -group /julia/code regex %{\b(true|false|C_NULL|Inf|NaN|Inf32|NaN32|nothing|\b-?\d+[fdiu]?)\b} 0:value
addhl -group /julia/code regex \b(if|else|elseif|while|for|begin|end|quote|try|catch|return|local|abstract|function|macro|ccall|finally|typealias|break|continue|type|global|module|using|import|export|const|let|bitstype|do|in|baremodule|importall|immutable)\b 0:keyword
addhl -group /julia/code regex \b(Number|Real|BigInt|Integer|UInt|UInt8|UInt16|UInt32|UInt64|UInt128|Int|Int8|Int16|Int32|Int64|Int128|BigFloat|FloatingPoint|Float16|Float32|Float64|Complex128|Complex64|Bool|Cuchar|Cshort|Cushort|Cint|Cuint|Clonglong|Culonglong|Cintmax_t|Cuintmax_t|Cfloat|Cdouble|Cptrdiff_t|Cssize_t|Csize_t|Cchar|Clong|Culong|Cwchar_t|Char|ASCIIString|UTF8String|ByteString|SubString|Array|DArray|AbstractArray|AbstractVector|AbstractMatrix|AbstractSparseMatrix|SubArray|StridedArray|StridedVector|StridedMatrix|VecOrMat|StridedVecOrMat|DenseArray|SparseMatrixCSC|BitArray|Range|OrdinalRange|StepRange|UnitRange|FloatRange|Tuple|NTuple|Vararg|DataType|Symbol|Function|Vector|Matrix|Union|Type|Any|Complex|String|Ptr|Void|Exception|Task|Signed|Unsigned|Associative|Dict|IO|IOStream|Rational|Regex|RegexMatch|Set|IntSet|Expr|WeakRef|ObjectIdDict|AbstractRNG|MersenneTwister)\b 0:type

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=julia %{ addhl ref julia }
hook global WinSetOption filetype=(?!julia).* %{ rmhl julia }
