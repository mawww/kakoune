# Kaleidoscope: colorblind-friendly light colorscheme
# https://personal.sron.nl/~pault/

evaluate-commands %sh{
    scope="${1:-global}"

    # NOTE: tone down black and white for aesthetics,
    # ideally those should be pure #000 and #FFF
    black="rgb:1C1C1C"
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
    set-face "${scope}" keyword ${muted_indigo}
    set-face "${scope}" attribute ${muted_purple}
    set-face "${scope}" type ${vibrant_blue}
    set-face "${scope}" string ${muted_wine}
    set-face "${scope}" value ${muted_rose}
    set-face "${scope}" meta ${muted_olive}
    set-face "${scope}" builtin ${muted_indigo}+b
    set-face "${scope}" module ${vibrant_orange}
    set-face "${scope}" comment ${muted_green}+i
    set-face "${scope}" function Default
    set-face "${scope}" operator Default
    set-face "${scope}" variable Default

    # For markup
    set-face "${scope}" title ${muted_indigo}+b
    set-face "${scope}" header ${high_contrast_blue}
    set-face "${scope}" block ${vibrant_magenta}
    set-face "${scope}" mono ${vibrant_red}
    set-face "${scope}" link ${vibrant_blue}+u
    set-face "${scope}" list Default
    set-face "${scope}" bullet +b
    set-face "${scope}" bold +b
    set-face "${scope}" italic +i

    # Built-in faces
    set-face "${scope}" Default ${black},${white}
    set-face "${scope}" PrimarySelection ${black},${pale_blue}+fg
    set-face "${scope}" SecondarySelection ${black},${pale_cyan}+fg
    set-face "${scope}" PrimaryCursor ${white},${dark_blue}+fg
    set-face "${scope}" SecondaryCursor ${white},${dark_cyan}+fg
    set-face "${scope}" PrimaryCursorEol ${white},${dark_grey}+fg
    set-face "${scope}" SecondaryCursorEol ${white},${vibrant_grey}+fg

    set-face "${scope}" StatusLine ${white},${dark_grey}
    set-face "${scope}" StatusLineMode ${black},${pale_blue}
    set-face "${scope}" StatusLineInfo ${black},${muted_sand}
    set-face "${scope}" StatusLineValue ${vibrant_orange},${muted_sand}+b
    set-face "${scope}" StatusCursor ${black},${high_contrast_yellow}
    set-face "${scope}" Prompt ${black},${muted_sand}
    set-face "${scope}" MenuForeground ${black},${muted_sand}
    set-face "${scope}" MenuBackground ${black},${pale_grey}
    set-face "${scope}" MenuInfo ${high_contrast_blue}+i

    set-face "${scope}" LineNumbers ${black},${pale_grey}
    set-face "${scope}" LineNumbersWrapped ${black},${vibrant_grey}+i
    set-face "${scope}" LineNumberCursor ${white},${dark_grey}+b
    set-face "${scope}" MatchingChar ${white},${dark_grey}
    set-face "${scope}" Whitespace ${vibrant_grey}+f
    set-face "${scope}" WrapMarker ${vibrant_grey}+f

    set-face "${scope}" Information ${black},${muted_sand}
    set-face "${scope}" Error ${white},${vibrant_red}
    set-face "${scope}" BufferPadding ${vibrant_grey}

EOF
}
