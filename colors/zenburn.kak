# zenburn theme

evaluate-commands %sh{
    scope="${1:-global}"

    # define some named colors
    zentext="rgb:cfcfcf"
    zenselectionbg="rgb:3f7fcc"
    zencursor="rgb:2a2a2a,rgb:dfdfbf"
    zencursoreol="rgb:2a2a2a,rgb:cc9393"
    zensecondaryfg="rgb:2a2a2a"
    zendefault="${zentext},rgb:3f3f3f"
    zenstatus="rgb:efdcbc,rgb:2a2a2a"
    zenstatuscursor="${zentext},rgb:7f9f7f"
    zeninfo="rgb:cc9393,rgb:2a2a2a"
    zenmenubg="rgb:7f9f7f,rgb:4a4a4a"
    zenmenufg="rgb:8cd0d3,rgb:5b5b5b"
    zenkeyword="rgb:f0dfaf+b"
    zenstorageClass="rgb:c3bf9f+b"
    zennumber="rgb:8cd0d3"
    zencomment="rgb:7f9f7f"
    zenconstant="rgb:dca3a3+b"
    zenspecial="rgb:cfbfaf"
    zenfunction="rgb:efef8f"
    zenstatement="rgb:e3ceab"
    zenvariable="rgb:efdcbc"
    zentype="rgb:dfdfbf"
    zenstring="rgb:cc9393"
    zenmodule="${zenstring}"
    zenexception="rgb:c3bf9f+b"
    zenmatching="rgb:3f3f3f,rgb:8cd0d3"
    zenpadding="rgb:f0dfaf,rgb:343434+b"

    echo "
        # then we map them to code
        face ${scope} value ${zenconstant}
        face ${scope} type ${zentype}
        face ${scope} variable ${zenvariable}
        face ${scope} module ${zenstring}
        face ${scope} function ${zenfunction}
        face ${scope} string ${zenstring}
        face ${scope} keyword ${zenkeyword}
        face ${scope} operator ${zenfunction}
        face ${scope} attribute ${zenstatement}
        face ${scope} comment ${zencomment}
        face ${scope} meta ${zenspecial}
        face ${scope} builtin default+b

        # and markup
        face ${scope} title ${zenkeyword}
        face ${scope} header ${zenconstant}
        face ${scope} bold ${zenstorageClass}
        face ${scope} italic ${zenfunction}
        face ${scope} mono ${zennumber}
        face ${scope} block ${zenstatement}
        face ${scope} link ${zenstring}
        face ${scope} bullet ${zenvariable}
        face ${scope} list ${zentype}

        # and built in faces
        face ${scope} Default ${zendefault}
        face ${scope} PrimarySelection ${zentext},${zenselectionbg}+fg
        face ${scope} SecondarySelection ${zensecondaryfg},${zenselectionbg}+fg
        face ${scope} PrimaryCursor ${zencursor}+fg
        face ${scope} SecondaryCursor ${zencursor}+fg
        face ${scope} PrimaryCursorEol ${zencursoreol}+fg
        face ${scope} SecondaryCursorEol ${zencursoreol}+fg
        face ${scope} LineNumbers ${zendefault}
        face ${scope} LineNumberCursor ${zenstatus}
        face ${scope} MenuForeground ${zenmenufg}
        face ${scope} MenuBackground ${zenmenubg}
        face ${scope} MenuInfo rgb:cc9393
        face ${scope} Information ${zeninfo}
        face ${scope} Error default,red
        face ${scope} StatusLine ${zenstatus}
        face ${scope} StatusLineMode ${zencomment}
        face ${scope} StatusLineInfo ${zenspecial}
        face ${scope} StatusLineValue ${zennumber}
        face ${scope} StatusCursor ${zenstatuscursor}
        face ${scope} Prompt ${zenconstant}
        face ${scope} MatchingChar default+b
        face ${scope} BufferPadding ${zenpadding}
        face ${scope} Whitespace ${zensecondaryfg}+f
    "
}
