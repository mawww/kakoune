# solarized theme

# Solarized colorscheme will start in light or dark background,
# depending on the following option.
# 
decl -hidden str solarized_initially light

# Map the following toggle function to an alias or a key sequence
# for easy toggling between light and dark, eg,
# map global user t :solarized-toggle-background<ret>
# 
def -docstring 'toggle solarized background between light and dark' -allow-override \
solarized-toggle-background %{
    %sh{
        set_bg() printf %s\\n "set global solarized_background $1"
        case "$kak_opt_solarized_background" in
        ( dark) set_bg 'light';;
        (light) set_bg  'dark';;
        esac
    }
    colorscheme solarized
}

%sh{
    # Base color definitions
    dark() {
        base03="rgb:002b36"
        base02="rgb:073642"
        base01="rgb:586e75"
        base00="rgb:657b83"
         base0="rgb:839496"
         base1="rgb:93a1a1"
         base2="rgb:eee8d5"
         base3="rgb:fdf6e3"
    }
    light() {
         base3="rgb:002b36"
         base2="rgb:073642"
         base1="rgb:586e75"
         base0="rgb:657b83"
        base00="rgb:839496"
        base01="rgb:93a1a1"
        base02="rgb:eee8d5"
        base03="rgb:fdf6e3"
    }

    if test ! "$kak_opt_solarized_background"; then
        decl_bg() printf %s\\n "decl -hidden str solarized_background $1"
        case "$kak_opt_solarized_initially" in
        ( dark)  dark; decl_bg  'dark';;
        (light) light; decl_bg 'light';;
        esac
    else
        case "$kak_opt_solarized_background" in
        ( dark)  dark;;
        (light) light;;
        esac
    fi

    yellow="rgb:b58900"
    orange="rgb:cb4b16"
       red="rgb:dc322f"
   magenta="rgb:d33682"
    violet="rgb:6c71c4"
      blue="rgb:268bd2"
      cyan="rgb:2aa198"
     green="rgb:859900"

    echo "
        # then we map them to code
        face value      ${cyan}
        face type       ${yellow}
        face variable   ${blue}
        face module     ${cyan}
        face function   default
        face string     ${cyan}
        face keyword    ${green}
        face operator   default
        face attribute  ${violet}
        face comment    ${base01}
        face meta       ${orange}
        face builtin    default+b

        # and markup
        face title      ${yellow}
        face header     ${blue}
        face bold       ${base1}
        face italic     ${base2}
        face mono       ${base3}
        face block      ${violet}
        face link       ${magenta}
        face bullet     ${orange}
        face list       ${yellow}

        # and built in faces
        face Default            ${base0},${base03}
        face PrimarySelection   white,blue
        face SecondarySelection black,blue
        face PrimaryCursor      black,white
        face SecondaryCursor    black,white
        face LineNumbers        ${base0},${base03}
        face LineNumberCursor   default,${base03}+b
        face MenuForeground     ${cyan},${base01}
        face MenuBackground     ${base02},${base01}
        face MenuInfo           ${base03}
        face Information        ${base02},${base1}
        face Error              default,red
        face StatusLine         default,${base02}
        face StatusLineMode     ${orange}
        face StatusLineInfo     ${cyan}
        face StatusLineValue    ${green}
        face StatusCursor       ${base00},${base3}
        face Prompt             yellow
        face MatchingChar       default+b
        face BufferPadding      ${base01},${base03}
    "
}
