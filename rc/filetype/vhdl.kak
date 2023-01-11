# Based on IEEE Std 1076‐2019

# Detection
hook global BufCreate .*[.](vhd[l]?) %[
    set-option buffer filetype vhdl
]

# Initialization
hook global WinSetOption filetype=vhdl %[
    require-module vhdl
    set-option window static_words %opt{vhdl_static_words}
    hook -group vhdl-indent window InsertChar \n vhdl-indent-on-new-line
    hook -group vhdl-indent window InsertChar \) vhdl-indent-on-closing-parenthesis
    hook -group vhdl-insert window InsertChar \n vhdl-insert-on-new-line
    # Cleanup trailing whitespaces on current line insert end.
    hook -group vhdl-trim-indent window ModeChange pop:insert:.* %[ try %[ execute-keys -draft <semicolon> x s ^\h+$ <ret> d ] ]
    hook -once -always window WinSetOption filetype=.* %[ remove-hooks window vhdl-.+ ]
]

hook -group vhdl-highlight global WinSetOption filetype=vhdl %[
    add-highlighter window/vhdl ref vhdl
    hook -once -always window WinSetOption filetype=.* %[ remove-highlighter window/vhdl ]
]

provide-module vhdl %§

# Highlighters & Completion
add-highlighter shared/vhdl regions
add-highlighter shared/vhdl/code default-region group
add-highlighter shared/vhdl/comment_line region '--' $ fill comment
add-highlighter shared/vhdl/comment region /\* \*/ fill comment

# Integer formats
add-highlighter shared/vhdl/code/ regex '(?i)\b0b[01]+l?\b' 0:value
add-highlighter shared/vhdl/code/ regex '(?i)\b0x[\da-f]+l?\b' 0:value
add-highlighter shared/vhdl/code/ regex '(?i)\b0o?[0-7]+l?\b' 0:value
add-highlighter shared/vhdl/code/ regex '(?i)\b([1-9]\d*|0)l?\b' 0:value
# Float formats
add-highlighter shared/vhdl/code/ regex '\b\d+[eE][+-]?\d+\b' 0:value
add-highlighter shared/vhdl/code/ regex '(\b\d+)?\.\d+\b' 0:value
add-highlighter shared/vhdl/code/ regex '\b\d+\.' 0:value
# Imaginary formats
add-highlighter shared/vhdl/code/ regex '\b\d+\+\d+[jJ]\b' 0:value

evaluate-commands %sh[
    values="true false note warning error failure nul"

    # LRM 5.2.4.1
    units="fs ps ns us ms sec min hr Å nm um mm cm m km"

    # LRM 16.2
    predefined_attributes="
        base left right high low ascending length range reverse_range
        subtype image pos succ pred leftof rightof value val
        designated_subtype reflect high low index element delayed
        stable quiet transaction event active last_event last_active
        last_value driving driving_value simple_name instance_name
        path_name record signal converse
    "

    libraries="ieee std"

    packages="
        math_real math_complex std_logic_1164 std_logic_textio numeric_bit numeric_std
        numeric_bit_unsigned numeric_std_unsigned fixed_float_types fixed_generic_pkg
        fixed_pkg float_generic_pkg float_pkg
        standard textio env
    "

     # LRM 15.10
    reserved_words="
        abs access after alias all and architecture array assert assume assume_guarantee attribute
        begin block body buffer bus
        case component configuration constant context cover
        default disconnect downto
        else elsif end entity exit
        fairness file for force function
        generate generic group guarded
        if impure in inertial inout is
        label library linkage literal loop
        map mod
        nand new next nor not null
        of on open or others out
        package parameter port postponed procedure process property protected pure
        range record register reject release rem report restrict restrict_guarantee return rol ror
        select sequence severity signal shared sla sll sra srl strong subtype
        then to transport type
        unaffected units until use
        variable view vpkg vmode vprop vunit
        wait when while with
        xnor xor
    "

    types="
        bit bit_vector
        boolean boolean_vector
        character
        file_open_state file_origin_kind
        integer natural positive
        line line_vector
        real real_vector
        std_logic std_logic_vector
        std_ulogic std_ulogic_vector
        side
        signed unsigned
        string text
        time time_vector
    "

    functions="
        find_leftmost find_rightmost divide reciprocal remainder modulo minimum maximum
        std_match add_carry scalb
        resize to_ufixed to_sfixed to_unsigned to_signed to_real to_integer to_slv
        to_std_logic_vector to_stdlogicvector to_sulv to_std_ulogic_vector to_std_ulogicvector
        to_01 is_x to_x01 to_ux01 to_x01z
        ufixed_high ufixed_low sfixed_high sfixed_low to_ufix to_sfix ufix_high ufix_low
        sfix_high sfix_low
        write read bwrite binary_write bread binary_read owrite oread octal_write octal_read
        hwrite hread hex_write hex_read to_string to_bstring to_binary_string to_ostring
        to_octal_string to_hstring to_hex_string from_string from_bstring from_binary_string
        from_ostring from_octal_string from_hstring from_hex_string
        rising_edge falling_edge
    "

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    # Add the language's grammar to the static completion list
    printf %s\\n "declare-option str-list vhdl_static_words $(join "${values} ${units} ${predefined_attributes} ${libraries} ${packages} ${reserved_words} ${types} ${functions}" ' ')"

     # Highlight keywords
    printf %s "
        add-highlighter shared/vhdl/code/ regex '(?i)\b($(join "${values}" '|'))\b' 0:value
        add-highlighter shared/vhdl/code/ regex '(?i)\b($(join "${units}" '|'))\b' 0:meta
        add-highlighter shared/vhdl/code/ regex \"'(?i)\b($(join "${predefined_attributes}" '|'))\b\" 0:attribute
        add-highlighter shared/vhdl/code/ regex '(?i)\b($(join "${libraries}" '|'))\b' 0:builtin
        add-highlighter shared/vhdl/code/ regex '(?i)\b($(join "${packages}" '|'))\b' 0:builtin
        add-highlighter shared/vhdl/code/ regex '(?i)\b($(join "${reserved_words}" '|'))\b' 0:keyword
        add-highlighter shared/vhdl/code/ regex '(?i)\b($(join "${functions}" '|'))\b\(' 1:builtin
        add-highlighter shared/vhdl/code/ regex '(?i)\b($(join "${types}" '|'))\b' 0:type
        add-highlighter shared/vhdl/code/ regex '^\h*(@[\w_.]+))' 1:attribute
    "
]

add-highlighter shared/vhdl/code/ regex \(|\)|\;|\.|,|:|\| 0:attribute

add-highlighter shared/vhdl/code/ regex \?\?|=|/=|<|<=|>|>=|\?=|\?/=|\?<|\?<=|\?>|\?>=|\+|-|&|\*|/|:= 0:operator

# Meta values highlight.
# The values 'U', 'X', 'W', and '–' are metalogical values; they define the behavior of the model itself rather than the behavior of the hardware being synthesized.
add-highlighter shared/vhdl/code/ regex "(?i)'[U|X|W|-]'" 0:meta
# Highlight other logical values.
add-highlighter shared/vhdl/code/ regex "(?i)'[0|1|Z|L|H]'" 0:value

# String
add-highlighter shared/vhdl/code/ regex '"[^"]*"' 0:string

# Binary vector.
add-highlighter shared/vhdl/code/ regex '[bB]"[01_]*"' 0:value

# Octal vector.
add-highlighter shared/vhdl/code/ regex '[oO]"[01234567_]*"' 0:value

# Hex vector.
add-highlighter shared/vhdl/code/ regex '(?i)x"[0123456789abcdef_]*"' 0:value

define-command -hidden vhdl-insert-on-new-line %[
    # Handle comment lines.
    evaluate-commands -itersel %[
        # Copy '--' comment prefix and following white spaces.
        try %[
            # <a-lt> is needed because of "Preserve previous line indent" command.
            try %[ execute-keys -draft k x s ^\h*--\h* <ret> y j <a-lt> gh P ]
        ]
    ]

    evaluate-commands -save-regs x %[
        # Save previous line indent in register x.
        try %[ execute-keys -draft kxs^\h+<ret>"xy ] catch %[ reg x '' ]

        # All "wisely add" commands share the same concept.
        # Only "end if" has extra comments.
        # Wisely add "end if;".
        evaluate-commands %[
            try %[
                # Validate previous line and that it is not closed yet.
                execute-keys -draft kx <a-k>^\h*(?i)((then|(.*:\h*)?if\b.*\bthen)$)<ret> j}ijx <a-K>^<c-r>x(?i)(elsif|else|end)\b<ret>
                # Don't add for "if ... generate", it requires "end generate;".
                execute-keys -draft kx <a-K>(?i)\bgenerate\b<ret>
                execute-keys -draft o<c-r>xend<space>if<semicolon><esc>
            ]
        ]
        # Wisely add "end generate;".
        evaluate-commands %[
            try %[
                execute-keys -draft kx <a-k>^\h*(?i).*\bgenerate$<ret> j}ijx <a-K>^<c-r>x(?i)(begin|elsif|else|end)\b<ret>
                # Don't add in case of comment line.
                execute-keys -draft kx <a-K>^\h*--<ret>
                execute-keys -draft o<c-r>xend<space>generate<semicolon><esc>
            ]
        ]
        # Wisely add "when" and "end case;".
        evaluate-commands %[
            try %[
                # TODO: Case needs special handling.
                execute-keys -draft kx <a-k>^\h*(?i)(case|.*\h*:\h*case)\b<ret> jwx <a-K>^<c-r>x(?i)(end|when)<ret>
                execute-keys -draft <c-r>xo<c-r>xend<space>case<semicolon><esc>kAwhen<space><esc>
            ]
        ]
        # Wisely add "begin" and "end block;".
        evaluate-commands %[
            try %[
                execute-keys -draft kx <a-k>^\h*(?i)((block|.*:\h*block)\b)<ret> j}ijx <a-K>^<c-r>x(?i)(begin|end)\b<ret>
                execute-keys -draft o<c-r>xbegin<ret><c-r>xend<space>block<semicolon><esc>
            ]
        ]
        # Wisely add "begin" and "end process;".
        evaluate-commands %[
            try %[
                execute-keys -draft kx <a-k>^\h*(?i)(.*:\h*)?(postponed\h+)?process\b<ret> j}ijx <a-K>^<c-r>x(?i)(begin|end)<ret>
                execute-keys -draft o<c-r>xbegin<ret><c-r>xend<space>process<semicolon><esc>
            ]
        ]
        # Wisely add "end loop;".
        evaluate-commands %[
            try %[
                execute-keys -draft kx <a-k>^\h*(?i)(.*\bloop|.*\h*:\h*(for|loop))$<ret> j}ijx <a-K>^<c-r>x(?i)(end)<ret>
                execute-keys -draft o<c-r>xend<space>loop<semicolon><esc>
            ]
        ]
        # Wisely add "end protected;".
        evaluate-commands %[
            try %[
                execute-keys -draft kx <a-k>^\h*(?i)(type\b.*\bis\h+protected)$<ret> j}ijx <a-K>^<c-r>x(?i)(end)<ret>
                execute-keys -draft o<c-r>xend<space>protected<semicolon><esc>
            ]
        ]
        # Wisely add "end protected body;".
        evaluate-commands %[
            try %[
                execute-keys -draft kx <a-k>^(?i)(\h*type\h+\w+\h+is\h+protected\h+body$)<ret> j}ijx <a-K>^<c-r>x(?i)end\h+protected\h+body\b<ret>
                execute-keys -draft o<c-r>xend<space>protected<space>body<semicolon><esc>
            ]
        ]
        # Wisely add "end record;".
        evaluate-commands %[
            try %[
                execute-keys -draft kx <a-k>^\h*(?i)(type\b.*\bis\h+record\h*)$<ret> j}ijx <a-K>^<c-r>x(?i)(end)<ret>
                execute-keys -draft o<c-r>xend<space>record<semicolon><esc>
            ]
        ]
        # Wisely add ");" for "type ... is (".
        evaluate-commands %[
            try %[
                execute-keys -draft kx <a-k>^\h*(?i)(type\b.*\bis\h+\(\h*)$<ret> j}ijx <a-K>^<c-r>x(\))<ret>
                execute-keys -draft o<c-r>x)<semicolon><esc>
            ]
        ]
        # Wisely add "end entity;".
        evaluate-commands %[
            try %[
                execute-keys -draft kx <a-k>^(?i)\h*entity\b.*\bis$<ret> j}ijx <a-K>^<c-r>x(?i)(begin|end)\b<ret>
                execute-keys -draft o<c-r>xend<space>entity<semicolon><esc>
            ]
        ]
        # Wisely add "begin" and "end function;".
        evaluate-commands %[
            try %[
                execute-keys -draft kx <a-k>^(?i)(\h*\)?\h*return\b.*\bis$)<ret> j}ijx <a-K>^<c-r>x(?i)(begin|end)\b<ret>
                execute-keys -draft o<c-r>xbegin<ret><c-r>xend<space>function<semicolon><esc>
            ]
            try %[
                execute-keys -draft kx <a-k>^(?i)(\h*((pure|impure)\h+)?function\b.*\bis$)<ret> j}ijx <a-K>^<c-r>x(?i)(begin|end)\b<ret>
                execute-keys -draft o<c-r>xbegin<ret><c-r>xend<space>function<semicolon><esc>
            ]
        ]
        # Wisely add "begin" and "end procedure;".
        evaluate-commands %[
            try %[
                execute-keys -draft kx <a-k>^(?i)(\h*procedure\b.*\bis$)<ret> j}ijx <a-K>^<c-r>x(?i)\b(begin|end)\b<ret>
                execute-keys -draft o<c-r>xbegin<ret><c-r>xend<space>procedure<semicolon><esc>
            ]
            try %[
                execute-keys -draft kx <a-k>^(?i)\h*\)\h*\bis$<ret> j}ijx <a-K>^<c-r>x(?i)\b(begin|end)\b<ret>
                # Verify that line with opening parenthesis contains "procedure" keyword.
                execute-keys -draft kx s\)<ret> <a-m><semicolon> x<a-k> (?i)\bprocedure\b<ret>
                execute-keys -draft o<c-r>xbegin<ret><c-r>xend<space>procedure<semicolon><esc>
            ]
        ]
        # Wisely add "end package;".
        evaluate-commands %[
            try %[
                execute-keys -draft kx <a-k>^(?i)(package\b)<ret> j}ijx <a-K>^<c-r>x(?i)(end)\b<ret>
                # Make sure it is not package body.
                execute-keys -draft kx<a-K>(?i)\bbody\b<ret>
                execute-keys -draft oend<space>package<semicolon><esc>
            ]
        ]
        # Wisely add "end package body;".
        evaluate-commands %[
            try %[
                execute-keys -draft kx <a-k>^(?i)(package\h+body\b)<ret> j}ijx <a-K>^<c-r>x(?i)(end)\b<ret>
                execute-keys -draft oend<space>package<space>body<semicolon><esc>
            ]
        ]
        # Wisely add "begin" and "end architecture;".
        evaluate-commands %[
            try %[
            execute-keys -draft kx <a-k>^(?i)\h*architecture\b<ret> j}ijx <a-K>^<c-r>x(?i)(begin|end)\b<ret>
            execute-keys -draft o<c-r>xbegin<ret><c-r>xend<space>architecture<semicolon><esc>
            ]
        ]
        # Wisely add ");" for "port (".
        evaluate-commands %[
            try %[
                execute-keys -draft kx <a-k>^\h*(?i)port\h*\($<ret> j}ijx <a-K>^<c-r>x(\)\;)<ret>
                execute-keys -draft o<c-r>x)<semicolon><esc>
            ]
        ]
        # Wisely add ");" for "port map (".
        evaluate-commands %[
            try %[
                execute-keys -draft kx <a-k>^\h*(?i)port\h+map\h*\($<ret> j}ijx <a-K>^<c-r>x(\)\;)<ret>
                execute-keys -draft o<c-r>x)<semicolon><esc>
            ]
        ]
        # Wisely add ");" for "generic (".
        evaluate-commands %[
            try %[
                execute-keys -draft kx <a-k>^\h*(?i)generic\h*\($<ret> j}ijx <a-K>^<c-r>x(\)\;)<ret>
                execute-keys -draft o<c-r>x)<semicolon><esc>
            ]
        ]
        # Wisely add ")" for "generic map (".
        evaluate-commands %[
            try %[
                execute-keys -draft kx <a-k>^\h*(?i)generic\h+map\h*\($<ret> j}ijx <a-K>^<c-r>x(\))<ret>
                execute-keys -draft o<c-r>x)<esc>
            ]
        ]
        # Wisely add ") return ;" for "[pure|impure] function ... (".
        evaluate-commands %[
            try %[
                execute-keys -draft kx <a-k>^\h*(?i)(pure\b|impure\b)?\h*function\b.*\h*\($<ret> j}ijx <a-K>^<c-r>x(\)\h*return.*)<ret>
                execute-keys -draft o<c-r>x)<space>return<space><semicolon><esc>
            ]
        ]
        # Wisely add ");" for "procedure ... (".
        evaluate-commands %[
            try %[
                execute-keys -draft kx <a-k>^\h*(?i)procedure\b.*\h*\($<ret> j}ijx <a-K>^<c-r>x(\)\h*\;)<ret>
                execute-keys -draft o<c-r>x)<semicolon><esc>
            ]
        ]
    ]
]

define-command -hidden vhdl-indent-on-new-line %{
    evaluate-commands -itersel %{
        # Align "then" to previous "if|elsif".
        evaluate-commands -itersel -save-regs x %[
            try %[
                execute-keys -draft k x <a-k> (?i)^\h*then$ <ret>
                try %[ execute-keys -draft <a-/>(?i)\b(if|elsif)\b<ret>xs^\h+<ret>"xy ] catch %[ reg x '' ]
                try %[ execute-keys -draft k xs^\h+<ret>d ] catch %[ ]
                execute-keys -draft kgh i<c-r>x<esc>
            ]
        ]

         # Align "generate" to previous "if|for".
        evaluate-commands -itersel -save-regs x %[
            try %[
                execute-keys -draft k x <a-k> (?i)^\h*generate$ <ret>
                try %[ execute-keys -draft <a-/>(?i)\b(if|for)\b<ret>xs^\h+<ret>"xy ] catch %[ reg x '' ]
                try %[ execute-keys -draft k xs^\h+<ret>d ] catch %[ ]
                execute-keys -draft kgh i<c-r>x<esc>
            ]
        ]

         # Preserve previous line indent.
        try %[ execute-keys -draft <semicolon> K <a-&> ]

         # Cleanup trailing whitespaces from previous line.
        try %[ execute-keys -draft k x s \h+$ <ret> d ]

         # Increase indent after some keywords.
        try %[
            execute-keys -draft kx<a-k> (?i)\b(begin|block|body|else|for|generate|if|is|loop|process|protected|record|select|then)$ <ret>
            # Do not indent if in comment line.
            execute-keys -draft kx<a-K>(?i)^\h*--<ret>
            # Do not indent for "case ... is".
            execute-keys -draft kx<a-K>^\h*(?i)(case|.*\h*:\h*case)\b<ret>
            execute-keys -draft <semicolon> <a-gt>
        ]
        # Copy the indentation of the matching if.
        try %{ execute-keys -draft , k x <a-k> ^\h*(elsif\b|else$) <ret> gh [c^\h*(\S*\h*:\h*)?if\b,\bend\sif\b <ret> x <a-S> 1<a-&> , j K <a-&> }

        # Increase indent after some operators.
        try %[ execute-keys -draft <semicolon> k x <a-k> (\(|=>|<=|:=)$ <ret> j <a-gt> ]
     }
}

define-command vhdl-indent-on-closing-parenthesis %[
    evaluate-commands -itersel %[
        # Decrease indent after ")" at the beginning of line.
        try %[ execute-keys -draft x <a-k> (^\h+\)$) <ret> <a-lt> ]
    ]
]

§
