# palenight theme

# This was ported from https://github.com/drewtempelmeyer/palenight.vim

evaluate-commands %sh{
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
    menu_grey=rgb:697098
    special_grey=rgb:3b4048
    vertsplit=rgb:181a1f
    visual_black=default

    printf "%s\n" "
    # Code
    face global value         $dark_yellow
    face global type          $yellow
    face global function      $blue
    face global variable      $blue
    face global identifier    $blue
    face global string        $green
    face global error         rgb:c3bf9f+b
    face global keyword       $purple
    face global operator      $cyan
    face global attribute     rgb:eedc82
    face global comment       $comment_grey+i
    face global documentation comment

    # #include <...>
    face global meta       $yellow

    # Markup
    face global title  $blue
    face global header $cyan
    face global mono   $green
    face global block  $purple
    face global link   $cyan
    face global bullet $cyan
    face global list   $yellow

    # Builtin
    face global Default            $white,$black

    face global PrimarySelection   $black,$white+bfg
    face global SecondarySelection $black,$white+fg

    face global PrimaryCursor      white,$purple+bfg
    face global SecondaryCursor    $black,$purple+fg

    face global PrimaryCursorEol   $black,$green+fg
    face global SecondaryCursorEol $black,$green+fg

    face global LineNumbers        $gutter_fg_grey
    face global LineNumberCursor   $yellow,default+b

    # Bottom menu:
    # text + background
    face global MenuBackground     $black,$white
    face global MenuForeground     $black,$purple

    # completion menu info
    face global MenuInfo           $menu_grey,default+i

    # assistant, [+]
    face global Information        $white,$visual_grey

    face global Error              $white,$red
    face global DiagnosticError    $red
    face global DiagnosticWarning  $yellow
    face global StatusLine         $white,$black

    # Status line
    face global StatusLineMode     $black,$purple      # insert, prompt, enter key ...
    face global StatusLineInfo     $white,$visual_grey # 1 sel
    face global StatusLineValue    $visual_grey,$green # param=value, reg=value. ex: \"ey
    face global StatusCursor       white,$purple+bg

    face global Prompt             $purple,$black # :
    face global MatchingChar       $red+b         # (), {}
    face global BufferPadding      $gutter_fg_grey,$black   # EOF tildas (~)

    # Whitespace characters
    face global Whitespace         $gutter_fg_grey,$black+fg
    "
}
