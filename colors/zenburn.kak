# zenburn theme

%sh{
    # define some named colors
    zenbackground="default,rgb:3f3f3f"
    zenstatus="rgb:efdcbc,rgb:2a2a2a"
    zencursor="default,rgb:7f9f7f"
    zeninfo="rgb:cc9393,rgb:2a2a2a"
    zenmenubg="rgb:7f9f7f,rgb:4a4a4a"
    zenmenufg="rgb:8cd0d3,rgb:5b5b5b"
    zentext="rgb:efefef"
    zenkeyword="rgb:f0dfaf+b"
    zenstorageClass="rgb:c3bf9f+b"
    zennumber="rgb:8cd0d3"
    zencomment="rgb:7f9f7f"
    zenconstant="rgb:dca3a3+b"
    zenspecial="rgb:cfbfaf"
    zenfunction="rgb:efef8f"
    zenstatement="rgb:e3ceab"
    zenidentifier="rgb:efdcbc"
    zentype="rgb:dfdfbf"
    zenstring="rgb:cc9393"
    zenexception="rgb:c3bf9f+b"
    zenmatching="rgb:3f3f3f,rgb:8cd0d3"

    echo "
        # then we map them to code
        face value ${zenconstant}
        face type ${zentype}
        face identifier ${zenidentifier}
        face string ${zenstring}
        face error ${zenexception}
        face keyword ${zenkeyword}
        face operator ${zenfunction}
        face attribute ${zenstatement}
        face comment ${zencomment}
        face meta ${zenspecial}

        # and markup
        face title ${zenkeyword}
        face header ${zenconstant}
        face bold ${zenstorageClass}
        face italic ${zenfunction}
        face mono ${zennumber}
        face block ${zenstatement}
        face link ${zenstring}
        face bullet ${zenidentifier}
        face list ${zentype}

        # and built in faces
        face Default ${zenbackground}
        face PrimarySelection white,blue
        face SecondarySelection black,blue
        face PrimaryCursor black,white
        face SecondaryCursor black,white
        face LineNumbers default
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
        face StatusCursor ${zencursor}
        face Prompt yellow
        face MatchingChar default+b
    "
}
