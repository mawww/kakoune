# http://w3.org/Style/CSS
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](css) %{
    set-option buffer filetype css
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=css %[
    require-module css

    hook window ModeChange pop:insert:.* -group css-trim-indent css-trim-indent
    hook window InsertChar \n -group css-insert css-insert-on-new-line
    hook window InsertChar \n -group css-indent css-indent-on-new-line
    hook window InsertChar \} -group css-indent css-indent-on-closing-curly-brace
    set-option buffer extra_word_chars '_' '-'
    set-face global cssLogicalOperator +i@keyword

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window css-.+ }
]

hook -group css-highlight global WinSetOption filetype=css %{
    add-highlighter window/css ref css
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/css }
}


provide-module css %[

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾
add-highlighter shared/css regions
add-highlighter shared/css/code default-region group
add-highlighter shared/css/attr_selector region \[ \] regions
add-highlighter shared/css/double_string region '"'  (?<!\\)(\\\\)*" fill string
add-highlighter shared/css/single_string region "'"  (?<!\\)(\\\\)*' fill string
add-highlighter shared/css/comment    region /\* \*/ fill comment

evaluate-commands %sh{
  # html tags
  # generated from the URL & <code> below
  # includes elements that cannot be styled, which is fine.
  # 
  # https://developer.mozilla.org/en-US/docs/Web/HTML/Element
  html_tags='html body address article aside footer header h1 h2 h3 h4 h5 h6 main nav section blockquote dd div dl dt figcaption figure hr li ol p pre ul a abbr b bdi bdo br cite code data dfn em i kbd mark q rp rt ruby s samp small span strong sub sup time u var wbr area audio img map track video embed iframe object param picture portal source canvas noscript script del ins caption col colgroup table tbody td tfoot th thead tr button datalist fieldset form input label legend meter optgroup option output progress select textarea details dialog menu summary slot template acronym applet basefont bgsound big blink center content dir font frame frameset hgroup image keygen marquee menuitem nobr noembed noframes plaintext rb rtc shadow spacer strike tt xmp'

  # Units
  # ‾‾‾‾‾
  # generated from the URL & <code> below
  # includes #rgb, #rrggbb, #rrggbbaa as color values {3,8}
  # 
  # https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_Values_and_Units
  units='% cap ch cm deg dpcm dpi dppx em ex grad Hz ic in kHz lh mm ms pc pt px Q rad rem rlh s turn vb vh vi vmax vmin vw x'

  logical_ops='and not only from to'
  keywords='!important auto inherit initial unset none'
  media_types='all print screen speech'

  # easing_re='linear|ease(-(in-out|in|out))?|step-(start|end)'

  # combinators='+ > ~ ||'
  # attribute_op='= ~= |= ^= $= *='

  join() { eval set -- $1; IFS="|"; echo "$*"; }

  # Selectors
  # ‾‾‾‾‾‾‾‾‾
  # universal: *, ns|*, *|*, |*
  # class/id: .class, #id
  # type: element
  # attr: [attr=val]

  # order below matters
  printf %s "
  add-highlighter shared/css/code/tag_selectors regex \b($(join "${html_tags}"))((:[a-z:])|[\h.#,]) 1:keyword

  add-highlighter shared/css/code/functional_notation regex ([a-zA-Z0-9-_]+[a-zA-Z0-9])\( 1:keyword

  add-highlighter shared/css/code/logical_operators regex (\b($(join "${logical_ops}"))\b|$(join "${keywords}")) 1:cssLogicalOperator

  add-highlighter shared/css/code/media_types regex \b($(join "${media_types}"))\b 1:+i

  # (after functional notation as they may contain paranthesis)
  add-highlighter shared/css/code/pseudo regex (:{1,2})([a-z-]+) 0:attribute

  add-highlighter shared/css/code/at_rules regex @[a-z-]+ 0:function 

  add-highlighter shared/css/code/css_property regex ([A-Za-z][A-Za-z0-9_-]*)\h*:\h 1:operator 1:+a

  add-highlighter shared/css/code/selectors regex (\*|[*]?[.][A-Za-z][A-Za-z0-9_-]+) 0:type
  add-highlighter shared/css/code/selectors_id regex (\*|[*]?[#][A-Za-z][A-Za-z0-9_-]+) 0:type 0:+i

  add-highlighter shared/css/code/hex_values regex (#[0-9A-Fa-f]{3,8})\b 0:value 0:+a
  add-highlighter shared/css/code/units regex \b(\d*\.)?\d+($(join "${units}"))?\b 0:value 0:+a
  "
}

# Attribute Selectors
add-highlighter shared/css/attr_selector/base default-region group
add-highlighter shared/css/attr_selector/base/ regex ([a-zA-Z0-9-]+) 1:attribute
add-highlighter shared/css/attr_selector/base/ regex \h(i|s) 1:type
add-highlighter shared/css/attr_selector/double_string region '"'  (?<!\\)(\\\\)*" fill string
add-highlighter shared/css/attr_selector/single_string region "'"  (?<!\\)(\\\\)*' fill string

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden css-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden css-indent-on-new-line %[
    evaluate-commands -draft -itersel %[
        execute-keys <semicolon>
        try %<
            # if previous line is part of a comment, do nothing
            execute-keys -draft <a-?>/\*<ret> <a-K>^\h*[^/*\h]<ret>
        > catch %<
            # else if previous line closed a paren (possibly followed by words and a comment),
            # copy indent of the opening paren line
            execute-keys -draft kx 1s(\))(\h+\w+)*\h*(\;\h*)?(?://[^\n]+)?\n\z<ret> m<a-semicolon>J <a-S> 1<a-&>
        > catch %<
            # else indent new lines with the same level as the previous one
            execute-keys -draft K <a-&>
        >
        # filter previous line
        try %< execute-keys -draft k x <a-k>^\h+$<ret> Hd >
        # indent after lines ending with with {
        try %[ execute-keys -draft k x <a-k> \{$ <ret> j <a-gt> ]
        # deindent closing brace when after cursor
        try %[ execute-keys -draft x <a-k> ^\h*\} <ret> gh / \} <ret> m <a-S> 1<a-&> ]
    ]
]

define-command -hidden css-insert-on-new-line %[
    evaluate-commands -draft -itersel %<
    execute-keys <semicolon>
    try %[
        # if the previous line isn't within a comment scope, break
        execute-keys -draft kx <a-k>^(\h*/\*|\h+\*(?!/))<ret>

        # find comment opening, validate it was not closed, and check its using star prefixes
        execute-keys -draft <a-?>/\*<ret><a-H> <a-K>\*/<ret> <a-k>\A\h*/\*([^\n]*\n\h*\*)*[^\n]*\n\h*.\z<ret>

        try %[
            # if the previous line is opening the comment, insert star preceeded by space
            execute-keys -draft kx<a-k>^\h*/\*<ret>
            execute-keys -draft i*<space><esc>
        ] catch %[
           try %[
                # if the next line is a comment line insert a star
                execute-keys -draft jx<a-k>^\h+\*<ret>
                execute-keys -draft i*<space><esc>
            ] catch %[
                try %[
                    # if the previous line is an empty comment line, close the comment scope
                    execute-keys -draft kx<a-k>^\h+\*\h+$<ret> x1s\*(\h*)<ret>c/<esc>
                ] catch %[
                    # if the previous line is a non-empty comment line, add a star
                    execute-keys -draft i*<space><esc>
                ]
            ]
        ]

        # trim trailing whitespace on the previous line
        try %[ execute-keys -draft s\h+$<ret> d ]
        # align the new star with the previous one
        execute-keys Kx1s^[^*]*(\*)<ret>&
    ]
    >
]

define-command -hidden css-indent-on-closing-curly-brace %[
    evaluate-commands -draft -itersel %[
        # align to opening curly brace when alone on a line
        try %[ execute-keys -draft <a-h> <a-k> ^\h+\}$ <ret> m s \A|.\z <ret> 1<a-&> ]
    ]
]

]
