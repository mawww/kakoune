# zenburn theme

%sh{
    # define some named colors
    zentext="rgb:cfcfcf"
    zenselectionbg="rgb:3f7fcc"
    zencursor="rgb:2a2a2a,rgb:dfdfbf"
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
        face value ${zenconstant}
        face type ${zentype}
        face variable ${zenvariable}
        face module ${zenstring}
        face function ${zenfunction}
        face string ${zenstring}
        face keyword ${zenkeyword}
        face operator ${zenfunction}
        face attribute ${zenstatement}
        face comment ${zencomment}
        face meta ${zenspecial}
        face builtin default+b

        # and markup
        face title ${zenkeyword}
        face header ${zenconstant}
        face bold ${zenstorageClass}
        face italic ${zenfunction}
        face mono ${zennumber}
        face block ${zenstatement}
        face link ${zenstring}
        face bullet ${zenvariable}
        face list ${zentype}

        # and built in faces
        face Default ${zendefault}
        face PrimarySelection ${zentext},${zenselectionbg}
        face SecondarySelection ${zensecondaryfg},${zenselectionbg}
        face PrimaryCursor ${zencursor}
        face SecondaryCursor ${zencursor}
        face LineNumbers ${zendefault}
        face LineNumberCursor ${zenstatus}
        face MenuForeground ${zenmenufg}
        face MenuBackground ${zenmenubg}
        face MenuInfo rgb:cc9393
        face Information ${zeninfo}
        face Error default,red
        face StatusLine ${zenstatus}
        face StatusLineMode ${zencomment}
        face StatusLineInfo ${zenspecial}
        face StatusLineValue ${zennumber}
        face StatusCursor ${zenstatuscursor}
        face Prompt ${zenconstant}
        face MatchingChar default+b
        face BufferPadding ${zenpadding}
    "
}
