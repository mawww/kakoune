# zenburn theme

evaluate-commands %sh{
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
        face global value ${zenconstant}
        face global type ${zentype}
        face global variable ${zenvariable}
        face global module ${zenstring}
        face global function ${zenfunction}
        face global string ${zenstring}
        face global keyword ${zenkeyword}
        face global operator ${zenfunction}
        face global attribute ${zenstatement}
        face global comment ${zencomment}
        face global documentation comment
        face global meta ${zenspecial}
        face global builtin default+b

        # and markup
        face global title ${zenkeyword}
        face global header ${zenconstant}
        face global mono ${zennumber}
        face global block ${zenstatement}
        face global link ${zenstring}
        face global bullet ${zenvariable}
        face global list ${zentype}

        # and built in faces
        face global Default ${zendefault}
        face global PrimarySelection ${zentext},${zenselectionbg}+fg
        face global SecondarySelection ${zensecondaryfg},${zenselectionbg}+fg
        face global PrimaryCursor ${zencursor}+fg
        face global SecondaryCursor ${zencursor}+fg
        face global PrimaryCursorEol ${zencursoreol}+fg
        face global SecondaryCursorEol ${zencursoreol}+fg
        face global LineNumbers ${zendefault}
        face global LineNumberCursor ${zenstatus}
        face global MenuForeground ${zenmenufg}
        face global MenuBackground ${zenmenubg}
        face global MenuInfo rgb:cc9393
        face global Information ${zeninfo}
        face global Error default,red
        face global DiagnosticError red
        face global DiagnosticWarning yellow
        face global StatusLine ${zenstatus}
        face global StatusLineMode ${zencomment}
        face global StatusLineInfo ${zenspecial}
        face global StatusLineValue ${zennumber}
        face global StatusCursor ${zenstatuscursor}
        face global Prompt ${zenconstant}
        face global MatchingChar default+b
        face global BufferPadding ${zenpadding}
        face global Whitespace ${zensecondaryfg}+f
    "
}
