# https://pest.rs/

# Detection
# ---------

hook global BufCreate .*\.pest %{
	set-option buffer filetype pest
}

# Initialization
# --------------

hook global WinSetOption filetype=pest %{
	require-module pest

	set-option window static_words %opt{pest_static_words}

	hook window ModeChange pop:insert:.* -group pest-trim-indent pest-trim-indent
	# apply indentation rules
	hook window InsertChar .* -group pest-indent pest-indent-on-char
	hook window InsertChar \n -group pest-indent pest-indent-on-new-line

	hook -once -always window WinSetOption filetype=.* %{ try %{ remove-hooks window pest-.+ } }
}

hook -group pest-highlight global WinSetOption filetype=pest %{
	add-highlighter window/pest ref pest
	hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/pest }
}

provide-module pest %∴

# Highlighters

add-highlighter shared/pest regions
add-highlighter shared/pest/code default-region group
# comments
add-highlighter shared/pest/comment region '//' '$' fill comment
add-highlighter shared/pest/comment2 region '/\*' '\*/' fill comment
# string types
add-highlighter shared/pest/string region '"' '(?<!\\)(\\\\)*"' fill string
add-highlighter shared/pest/char region "'" "'" fill string
add-highlighter shared/pest/string_case_insensitive region '\^"' '(?<!\\)(\\\\)*"' fill string

# identifiers
add-highlighter shared/pest/code/identifier regex [a-zA-Z_][a-zA-Z0-9_]* 0:variable

# operators, modifiers
add-highlighter shared/pest/code/modifier regex '(?<![A-Za-z0-9_])_(?![A-Za-z0-9_])|[@$!]' 0:operator
add-highlighter shared/pest/code/operator regex '[\|\*\+\?&!~]' 0:operator

evaluate-commands %sh{
	# grammar
	stack="PUSH POP POP_ALL PEEK PEEK_ALL DROP PUSH_LITERAL"
	keywords="WHITESPACE COMMENT ANY SOI EOI ASCII_DIGIT ASCII_NONZERO_DIGIT
			ASCII_BIN_DIGIT ASCII_OCT_DIGIT ASCII_HEX_DIGIT ASCII_ALPHA_LOWER
			ASCII_ALPHA_UPPER ASCII_ALPHA ASCII_ALPHANUMERIC ASCII
			ASCII_ALPHANUMERIC NEWLINE LETTER CASED_LETTER UPPERCASE_LETTER
			LOWERCASE_LETTER TITLECASE_LETTER MODIFIER_LETTER OTHER_LETTER MARK
			NONSPACING_MARK SPACING_MARK ENCLOSING_MARK NUMBER DECIMAL_NUMBER
			LETTER_NUMBER OTHER_NUMBER PUNCTUATION CONNECTOR_PUNCTUATION
			DASH_PUNCTUATION OPEN_PUNCTUATION CLOSE_PUNCTUATION
			INITIAL_PUNCTUATION FINAL_PUNCTUATION OTHER_PUNCTUATION SYMBOL
			MATH_SYMBOL CURRENCY_SYMBOL MODIFIER_SYMBOL OTHER_SYMBOL SEPARATOR
			SPACE_SEPARATOR LINE_SEPARATOR PARAGRAPH_SEPARATOR OTHER CONTROL FORMAT
			SURROGATE PRIVATE_USE UNASSIGNED ALPHABETIC BIDI_CONTROL BIDI_MIRRORED
			CASE_IGNORABLE CASED CHANGES_WHEN_CASEFOLDED CHANGES_WHEN_CASEMAPPED
			CHANGES_WHEN_LOWERCASED CHANGES_WHEN_TITLECASED CHANGES_WHEN_UPPERCASED
			DASH DEFAULT_IGNORABLE_CODE_POINT DEPRECATED DIACRITIC EMOJI
			EMOJI_COMPONENT EMOJI_MODIFIER EMOJI_MODIFIER_BASE EMOJI_PRESENTATION
			EXTENDED_PICTOGRAPHIC EXTENDER GRAPHEME_BASE GRAPHEME_EXTEND
			GRAPHEME_LINK HEX_DIGIT HYPHEN IDS_BINARY_OPERATOR IDS_TRINARY_OPERATOR
			ID_CONTINUE ID_START IDEOGRAPHIC JOIN_CONTROL LOGICAL_ORDER_EXCEPTION
			LOWERCASE MATH NONCHARACTER_CODE_POINT OTHER_ALPHABETIC
			OTHER_DEFAULT_IGNORABLE_CODE_POINT OTHER_GRAPHEME_EXTEND OTHER_ID_CONTINUE
			OTHER_ID_START OTHER_LOWERCASE OTHER_MATH OTHER_UPPERCASE PATTERN_SYNTAX
			PATTERN_WHITE_SPACE PREPENDED_CONCATENATION_MARK QUOTATION_MARK RADICAL
			REGIONAL_INDICATOR SENTENCE_TERMINAL SOFT_DOTTED TERMINAL_PUNCTUATION
			UNIFIED_IDEOGRAPH UPPERCASE VARIATION_SELECTOR WHITE_SPACE XID_CONTINUE
			XID_START"
	scripts="ADLAM AHOM ANATOLIAN_HIEROGLYPHS ARABIC ARMENIAN AVESTAN
			BALINESE BAMUM BASSA_VAH BATAK BENGALI BHAIKSUKI BOPOMOFO BRAHMI BRAILLE
			BUGINESE BUHID CANADIAN_ABORIGINAL CARIAN CAUCASIAN_ALBANIAN CHAKMA CHAM
			CHEROKEE CHORASMIAN COMMON COPTIC CUNEIFORM CYPRIOT CYPRO_MINOAN CYRILLIC
			DESERET DEVANAGARI DIVES_AKURU DOGRA DUPLOYAN EGYPTIAN_HIEROGLYPHS
			ELBASAN ELYMAIC ETHIOPIC GEORGIAN GLAGOLITIC GOTHIC GRANTHA GREEK
			GUJARATI GUNJALA_GONDI GURMUKHI HAN HANGUL HANIFI_ROHINGYA HANUNOO
			HATRAN HEBREW HIRAGANA IMPERIAL_ARAMAIC INHERITED INSCRIPTIONAL_PAHLAVI
			INSCRIPTIONAL_PARTHIAN JAVANESE KAITHI KANNADA KATAKANA KAWI KAYAH_LI
			KHAROSHTHI KHITAN_SMALL_SCRIPT KHMER KHOJKI KHUDAWADI LAO LATIN LEPCHA
			LIMBU LINEAR_A LINEAR_B LISU LYCIAN LYDIAN MAHAJANI MAKASAR MALAYALAM
			MANDAIC MANICHAEAN MARCHEN MASARAM_GONDI MEDEFAIDRIN MEETEI_MAYEK
			MENDE_KIKAKUI MEROITIC_CURSIVE MEROITIC_HIEROGLYPHS MIAO MODI MONGOLIAN
			MRO MULTANI MYANMAR NABATAEAN NAG_MUNDARI NANDINAGARI NEW_TAI_LUE
			NEWA NKO NUSHU NYIAKENG_PUACHUE_HMONG OGHAM OL_CHIKI OLD_HUNGARIAN
			OLD_ITALIC OLD_NORTH_ARABIAN OLD_PERMIC OLD_PERSIAN OLD_SOGDIAN
			OLD_SOUTH_ARABIAN OLD_TURKIC OLD_UYGHUR ORIYA OSAGE OSMANYA PAHAWH_HMONG
			PALMYRENE PAU_CIN_HAU PHAGS_PA PHOENICIAN PSALTER_PAHLAVI REJANG RUNIC
			SAMARITAN SAURASHTRA SHARADA SHAVIAN SIDDHAM SIGNWRITING SINHALA SOGDIAN
			SORA_SOMPENG SOYOMBO SUNDANESE SYLOTI_NAGRI SYRIAC TAGALOG TAGBANWA
			TAI_LE TAI_THAM TAI_VIET TAKRI TAMIL TANGSA TANGUT TELUGU THAANA THAI
			TIBETAN TIFINAGH TIRHUTA TOTO UGARITIC VAI VITHKUQI WANCHO WARANG_CITI
			YEZIDI YI ZANABAZAR_SQUARE"

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }
    printf %s\\n "declare-option str-list pest_static_words $(join "${stack}" ' ') $(join "${stack}" ' ')"
    printf %s\\n "add-highlighter shared/pest/code/keyword regex (?<!-)\b($(join "${keywords}" '|'))\b(?!-) 0:keyword"
    printf %s\\n "add-highlighter shared/pest/code/stack regex (?<!-)\b($(join "${stack}" '|'))\b(?!-) 0:builtin"
    printf %s\\n "add-highlighter shared/pest/code/scripts regex (?<!-)\b($(join "${scripts}" '|'))\b(?!-) 0:type"
}

# Commands

define-command -hidden pest-trim-indent %{
	try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden pest-insert-on-new-line %{
	evaluate-commands -draft -itersel %{
		# copy '//' comment prefix and following white spaces
		try %{ exec -draft k x s ^\h*//\h* <ret> y jgh P }
	}
}

define-command -hidden pest-indent-on-char %<
	evaluate-commands -draft -itersel %<
		# align closer token to opener when alone on a line
		try %< execute-keys -draft <a-h> <a-k> ^\h+[\]})]$ <ret> m <a-S> 1<a-&> >
	>
>

define-command -hidden pest-indent-on-new-line %<
	evaluate-commands -draft -itersel %<
		# preserve previous line indent
		try %{ exec -draft <semicolon> K <a-&> }
		# cleanup trailing whitespaces from previous line
		try %{ execute-keys -draft k :pest-trim-indent <ret>  }
		# indent after lines ending with opener token
		try %< execute-keys -draft k x <a-k> [[{(]\h*$ <ret> j <a-gt> >
		# deindent closer token(s) when after cursor
		try %< execute-keys -draft x <a-k> ^\h*[}\])] <ret> gh / [}\])] <ret> m <a-S> 1<a-&> >
	>
>

∴
