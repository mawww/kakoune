# palenight theme

# This was ported from https://github.com/drewtempelmeyer/palenight.vim

evaluate-commands %sh{
    scope="${1:-global}"

    red=rgb:ff5370
    light_red=rgb:ff869a
    dark_red=rgb:be5046
    green=rgb:c3e88d
    yellow=rgb:ffcb6b
    dark_yellow=rgb:f78c6c
    blue=rgb:82b1ff
    purple=rgb:c792ea
    cyan=rgb:89ddff
    white=rgb:bfc7d5
    black=rgb:292d3e
    comment_grey=rgb:697098
    gutter_fg_grey=rgb:4b5263
    cursor_grey=rgb:2c323c
    visual_grey=rgb:3e4452
    menu_grey=rgb:3e4452
    special_grey=rgb:3b4048
    vertsplit=rgb:181a1f
    visual_black=default

    printf "%s\n" "
    # Code
    face ${scope} value      $dark_yellow
    face ${scope} type       $yellow
    face ${scope} function   $blue
    face ${scope} variable   $blue
    face ${scope} identifier $blue
    face ${scope} string     $green
    face ${scope} error      rgb:c3bf9f+b
    face ${scope} keyword    $purple
    face ${scope} operator   $cyan
    face ${scope} attribute  rgb:eedc82
    face ${scope} comment    $comment_grey+i

    # #include <...>
    face ${scope} meta       $yellow

    # Markup
    face ${scope} title  $blue
    face ${scope} header $cyan
    face ${scope} bold   $red
    face ${scope} italic $yellow
    face ${scope} mono   $green
    face ${scope} block  $purple
    face ${scope} link   $cyan
    face ${scope} bullet $cyan
    face ${scope} list   $yellow

    # Builtin
    face ${scope} Default            $white,$black

    face ${scope} PrimarySelection   $black,$white+bfg
    face ${scope} SecondarySelection $black,$white+fg

    face ${scope} PrimaryCursor      white,$purple+bfg
    face ${scope} SecondaryCursor    $black,$purple+fg

    face ${scope} PrimaryCursorEol   $black,$green+fg
    face ${scope} SecondaryCursorEol $black,$green+fg

    face ${scope} LineNumbers        $gutter_fg_grey
    face ${scope} LineNumberCursor   $yellow,default+b

    # Bottom menu:
    # text + background
    face ${scope} MenuBackground     $black,$white
    face ${scope} MenuForeground     $black,$purple

    # completion menu info
    face ${scope} MenuInfo           $black,$white+i

    # assistant, [+]
    face ${scope} Information        $white,$visual_grey

    face ${scope} Error              $white,$red
    face ${scope} StatusLine         $white,$black

    # Status line
    face ${scope} StatusLineMode     $black,$purple      # insert, prompt, enter key ...
    face ${scope} StatusLineInfo     $white,$visual_grey # 1 sel
    face ${scope} StatusLineValue    $visual_grey,$green # param=value, reg=value. ex: \"ey
    face ${scope} StatusCursor       white,$purple+bg

    face ${scope} Prompt             $purple,$black # :
    face ${scope} MatchingChar       $red+b         # (), {}
    face ${scope} BufferPadding      $gutter_fg_grey,$black   # EOF tildas (~)

    # Whitespace characters
    face ${scope} Whitespace         $gutter_fg_grey,$black+fg
    "
}
