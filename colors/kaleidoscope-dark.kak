# Kaleidoscope: colorblind-friendly light colorscheme
# https://personal.sron.nl/~pault/

evaluate-commands %sh{
    scope="${1:-global}"

    # NOTE: tone down black and white for aesthetics,
    # ideally those should be pure #000 and #FFF
    black="rgb:303030"
    white="rgb:FDFDFD"

    # Regular text
    bright_blue="rgb:4477AA"
    bright_cyan="rgb:66CCEE"
    bright_green="rgb:228833"
    bright_yellow="rgb:CCBB44"
    bright_red="rgb:EE6677"
    bright_purple="rgb:AA3377"
    bright_grey="rgb:BBBBBB"

    # Emphasis
    high_contrast_blue="rgb:004488"
    high_contrast_yellow="rgb:DDAA33"
    high_contrast_red="rgb:BB5566"

    # High contrast alternative text
    vibrant_orange="rgb:EE7733"
    vibrant_blue="rgb:0077BB"
    vibrant_cyan="rgb:33BBEE"
    vibrant_magenta="rgb:EE3377"
    vibrant_red="rgb:CC3311"
    vibrant_teal="rgb:009988"
    vibrant_grey="rgb:BBBBBB"

    # Darker text with no red
    muted_rose="rgb:CC6677"
    muted_indigo="rgb:332288"
    muted_sand="rgb:DDCC77"
    muted_green="rgb:117733"
    muted_cyan="rgb:88CCEE"
    muted_wine="rgb:882255"
    muted_teal="rgb:44AA99"
    muted_olive="rgb:999933"
    muted_purple="rgb:AA4499"
    muted_pale_grey="rgb:DDDDDD"

    # Low contrast background colors
    light_blue="rgb:77AADD"
    light_orange="rgb:EE8866"
    light_yellow="rgb:EEDD88"
    light_pink="rgb:FFAABB"
    light_cyan="rgb:99DDFF"
    light_mint="rgb:44BB99"
    light_pear="rgb:BBCC33"
    light_olive="rgb:AAAA00"
    light_grey="rgb:DDDDDD"

    # Pale background colors, black foreground
    pale_blue="rgb:BBCCEE"
    pale_cyan="rgb:CCEEFF"
    pale_green="rgb:CCDDAA"
    pale_yellow="rgb:EEEEBB"
    pale_red="rgb:FFCCCC"
    pale_grey="rgb:DDDDDD"

    # Dark background colors, white foreground
    dark_blue="rgb:222255"
    dark_cyan="rgb:225555"
    dark_green="rgb:225522"
    dark_yellow="rgb:666633"
    dark_red="rgb:663333"
    dark_grey="rgb:555555"

    # NOTE: Do not use any color that hasn't been defined above (no hardcoding)
    cat <<- EOF

    # For Code
    set-face "${scope}" keyword ${vibrant_blue}
    set-face "${scope}" attribute ${muted_purple}
    set-face "${scope}" type ${vibrant_blue}
    set-face "${scope}" string ${muted_rose}
    set-face "${scope}" value ${light_pink}
    set-face "${scope}" meta ${light_olive}
    set-face "${scope}" builtin ${vibrant_blue}+b
    set-face "${scope}" module ${vibrant_orange}
    set-face "${scope}" comment ${bright_green}+i
    set-face "${scope}" function Default
    set-face "${scope}" operator Default
    set-face "${scope}" variable Default

    # For markup
    set-face "${scope}" title ${vibrant_blue}+b
    set-face "${scope}" header ${muted_cyan}
    set-face "${scope}" block ${vibrant_magenta}
    set-face "${scope}" mono ${vibrant_magenta}
    set-face "${scope}" link ${bright_cyan}+u
    set-face "${scope}" list Default
    set-face "${scope}" bullet +b
    set-face "${scope}" bold +b
    set-face "${scope}" italic +i

    # Built-in faces
    set-face "${scope}" Default ${white},${black}
    set-face "${scope}" PrimarySelection ${black},${pale_blue}+fg
    set-face "${scope}" SecondarySelection ${black},${pale_cyan}+fg
    set-face "${scope}" PrimaryCursor ${white},${high_contrast_blue}+fg
    set-face "${scope}" SecondaryCursor ${white},${dark_cyan}+fg
    set-face "${scope}" PrimaryCursorEol ${black},${vibrant_grey}+fg
    set-face "${scope}" SecondaryCursorEol ${black},${pale_grey}+fg

    set-face "${scope}" StatusLine ${black},${vibrant_grey}
    set-face "${scope}" StatusLineMode ${black},${light_blue}
    set-face "${scope}" StatusLineInfo ${black},${light_yellow}
    set-face "${scope}" StatusLineValue ${high_contrast_red},${light_yellow}+b
    set-face "${scope}" StatusCursor ${black},${light_orange}
    set-face "${scope}" Prompt ${black},${light_yellow}
    set-face "${scope}" MenuForeground ${black},${light_yellow}
    set-face "${scope}" MenuBackground ${black},${pale_grey}
    set-face "${scope}" MenuInfo ${vibrant_blue}+i

    set-face "${scope}" LineNumbers ${white},${dark_grey}
    set-face "${scope}" LineNumbersWrapped ${black},${vibrant_grey}+i
    set-face "${scope}" LineNumberCursor ${black},${pale_grey}+b
    set-face "${scope}" MatchingChar ${black},${vibrant_grey}
    set-face "${scope}" Whitespace ${dark_grey}+f
    set-face "${scope}" WrapMarker ${dark_grey}+f

    set-face "${scope}" Information ${black},${light_yellow}
    set-face "${scope}" Error ${white},${vibrant_red}
    set-face "${scope}" BufferPadding ${dark_grey}

EOF
}
