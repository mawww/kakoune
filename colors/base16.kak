##
## base16.kak by lenormf
##

%m4{
    define(`black_lighterer', `rgb:383838')dnl
    define(`black_lighter', `rgb:2D2D2D')dnl
    define(`black_light', `rgb:1C1C1C')dnl
    define(`cyan_light', `rgb:7CB0FF')dnl
    define(`green_dark', `rgb:A1B56C')dnl
    define(`grey_dark', `rgb:585858')dnl
    define(`grey_light', `rgb:D8D8D8')dnl
    define(`magenta_dark', `rgb:AB4642')dnl
    define(`magenta_light', `rgb:AB4434')dnl
    define(`orange_dark', `rgb:DC9656')dnl
    define(`orange_light', `rgb:F7CA88')dnl
    define(`purple_dark', `rgb:BA8BAF')dnl

    ## code
    face value              orange_dark+b
    face type               orange_light
    face identifier         magenta_dark
    face string             green_dark
    face error              grey_light,magenta_light
    face keyword            purple_dark+b
    face operator           grey_light
    face attribute          orange_dark
    face comment            grey_dark
    face meta               orange_light

    ## markup
    face title              blue
    face header             cyan_light
    face bold               orange_light
    face italic             orange_dark
    face mono               green_dark
    face block              orange_dark
    face link               blue
    face bullet             magenta_light
    face list               magenta_dark

    ## builtin
    face Default            grey_light,black_lighter
    face PrimarySelection   white,blue
    face SecondarySelection black,blue
    face PrimaryCursor      black,white
    face SecondaryCursor    black,white
    face LineNumbers        grey_light,black_lighter
    face LineNumberCursor   grey_light,rgb:282828+b
    face MenuForeground     grey_light,blue
    face MenuBackground     blue,grey_light
    face MenuInfo           cyan_light
    face Information        black_light,cyan_light
    face Error              grey_light,magenta_light
    face StatusLine         grey_light,black_lighterer
    face StatusLineMode     orange_dark
    face StatusLineInfo     cyan_light
    face StatusLineValue    green_dark
    face StatusCursor       black_lighterer,cyan_light
    face Prompt             black_light,cyan_light
    face MatchingChar       cyan_light,black_light+b
    face BufferPadding      cyan_light,black_lighter
}
