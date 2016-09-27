# Detection
# ---------
hook global BufCreate .*\.(s|S|asm)$ %{
    set buffer filetype gas
}


addhl -group / regions -default code gas \
    string         '"' (?<!\\)(\\\\)*"        '' \
    commentMulti   /\*       \*/              '' \
    commentSingle1 '#'       '$'              '' \
    commentSingle2 ';'       '$'              ''

addhl -group /gas/string fill string
addhl -group /gas/commentMulti fill comment
addhl -group /gas/commentSingle1 fill comment
addhl -group /gas/commentSingle2 fill comment

# Constant
addhl -group /gas/code regex (0[xX][0-9]+|\b[0-9]+)\b 0:value

# Labels
addhl -group /gas/code regex ^\h*([A-Za-z0-9_.-]+): 0:operator

# ARM Directives
addhl -group /gas/code regex ((^|\s+)\.([248]byte|align|arch(_extension)?|arm|bsscantunwind|code|[cf]pu|[dq]n|eabi_attribute|even|extend|ldouble|fnend|fnstart|force_thumb|handlerdata|inst(\.[nw])?|ltorg|movsp|object_arch|packed|pad|personality(index)?|pool|req|save|setfp|screl32|syntax|thumb(_func|_set)?|tlsdescseq|unreq|unwind_raw|vsave)(\h+|$)) 0:type

# Assembler Directives
addhl -group /gas/code regex ((^|\s+)\.(abort|ABORT|align|app-file|ascii|asciz|balign[wl]|byte|comm|data|def|desc|dim|double|eject|else|endif|equ|extern|file|fill|float|global|globl|hword|ident|if|include|int|irp|irpc|lcomm|iflags|line|linkonce|ln|mri|list|loc|local|long|macro|nolist|octa|org|print|purgem|p2align[wl]|psize|quad|rept|sbttl|section|set|short|single|size|skip|space|stab[dns]|string|struct|tag|text|title|type|title|uleb128|val|vtable_entry|weak|word|rodata|zero)(\h+|$)) 0:type

# Registers
addhl -group /gas/code regex \%(([re](ax|bx|cx|dx|si|di|bp|sp))|(al|bl|cl|dl|sil|dil|bpl|spl)|(r[8-9][dwb])|(r1[0-5][dwb])|(cs|ds|es|fs|gs|ss|ip|eflags)|([xy]mm[0-9]|[xy]mm1[0-5]))\b 0:identifier

# General Instructions
addhl -group /gas/code regex \
^\h*(mov|lea|call|test|cmp)([bwlq])?\b|\
^\h*(bswap[lq]|cmpxchg[bwlq]|cmpxchg8b|cwt[ld]|movabs([bwlq])?|popa([lw])?|pusha([wl])?)\b|\
^\h*(and|or|not|xor|sar|sal|shr|shl|sub|add|(i)?mul|(i)?div|inc|dec|adc|sbb)([bwlq])?\b|\
^\h*(rcl|rcr|rol|ror|shld|shrd)([bwlq])?\b|\
^\h*(bsf|bsr|bt|btc|btr|bts)([wlq])?\b|\
^\h*(cmps|lods|movs)([sxbwdq])?\b|\
^\h*(ret([bwlq])?|[il]ret([dq])?|leave|movzb[wlq]|movzw[lq]|movsb[wlq]|movsw[lq]|movslq|cwt[dl]|clt[sdq]|cqt[od])\b|\
^\h*set(([bagl])?e|(n)?[zlesgabop]|(n)?(ae|le|ge|be))\b|\
^\h*(cmovn[eszlgba]|cmov[glab]e|cmov[esglabz]|cmovn[lgba]e)\b|\
^\h*(jmp|j[esglabzcop]|jn[esglabzcop]|j[glasbp]e|jn[glab]e|j(e)?cxz|jpo)\b|\
^\h*(aa[adms]|da[as]|xadd[bwlq]|xchg[lwq])\b|\
^\h*(rep|repnz|repz|scas([qlwb])?|stos([qlwb])?)\b|\
^\h*(cl[cdi]|cmc|lahf|popf([lwq])?|pushf([lwq])?|sahf|st[cdi])\b|\
^\h*(l[defgs]s([wl])?|cpuid|nop|ud2|xlat(b)?)\b|\
^\h*(lea|call|push|pop)([wlq])?\b|\
^\h*(in|ins([lwb])?|out|outs([lwb])?)\b|\
^\h*(cb(t)?w|cwde|cdqe|cwd|cdq|cqo|sahf|lahf|por|pxor|movap[ds])\b|\
^\h*(bound([wl])?|enter|int(o)?|lcall|loop(n)?[ez]|pause)\b 0:keyword

#Floating Point Instructions
addhl -group /gas/code regex \
^\h*f(add|sub|mul|com|comp|sub|subr|div|divr|ld|xch|st|nop|stp|ldenv|chs|abs)\b|\
^\h*f(tst|xam|ldcw|ld1|ld2[te]|ldpi|ld[gn]2|ldz|(n)?stenv|2xm1|yl2x|p(a)?tan)\b|\
^\h*f(xtract|prem(1)?|(dec|inc)stp|(n)?stcw|yl2xp1|sqrt|sincos|rndint|scale|sin|cos|iadd)\b|\
^\h*f(cmov[bn]e|cmove|cmovn[beu]|cmovnbe|cmovu|imul|icom|icomp|isub|isubr|icomp)\b|\
^\h*(div|add|sub|mul|div)[ps]s\b|\
^\h*(div|add|sub|mul|div)[ps]d\b|\
^\h*(vmovs[ds]|vmovap[sd])\b|\
^\h*(vcvtts[ds]2si(q)?|vcvtsi2s[d](q)?|vunpcklps|vcvtps2pd|vmovddup|vcvtpd2psx)\b|\
^\h*(cvtss2s[di]|cvtsi2s[ds]|cvtsd2s[is]|cvtdq2p[ds]|cvtpd2(dq|pi|ps)|cvtpi2p[ds]|cvtps2p[id])\b|\
^\h*(cvttp[ds]2dq|cvttp[ds]2pi|cvtts[ds]2si)\b|\
^\h*(vxorp[sd]|vandp[sd]|ucomis[sd])\b 0:keyword

def -hidden _gas_filter_around_selections %{
    eval -draft -itersel %{
        exec <a-x>
        # remove trailing white spaces
        try %{ exec -draft s \h+$ <ret> d }
    }
}

def -hidden _gas_indent_on_new_line %~
    eval -draft -itersel %<
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # filter previous line
        try %{ exec -draft k : _gas_filter_around_selections <ret> }
        # indent after label
        try %[ exec -draft k <a-x> <a-k> :$ <ret> j <a-gt> ]
    >
~

hook -group gas-highlight global WinSetOption filetype=gas %{ addhl ref gas }

hook global WinSetOption filetype=gas %{
    hook window InsertChar \n -group gas-indent _gas_indent_on_new_line
}

hook global WinSetOption filetype=(?!gas).* %{
    rmhl gas

    rmhooks window gas-indent
}
