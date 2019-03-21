# http://www.scheme-reports.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# require lisp.kak

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate (.*/)?(.*\.(scm|ss|sld)) %{
    set-option buffer filetype scheme
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/scheme regions
add-highlighter shared/scheme/code default-region group

add-highlighter shared/scheme/string region '"' (?<!\\)(\\\\)*" fill string
add-highlighter shared/scheme/comment region ';' '$' fill comment
add-highlighter shared/scheme/comment-form region -recurse "\(" "#;\(" "\)" fill comment
add-highlighter shared/scheme/comment-block region "#\|" "\|#" fill comment
add-highlighter shared/scheme/quoted-form region -recurse "\(" "'\(" "\)" fill variable

add-highlighter shared/scheme/code/ regex (#t|#f) 0:value
add-highlighter shared/scheme/code/ regex \b[0-9]+\.[0-9]*\b 0:value

evaluate-commands %sh{ exec awk -f - <<'EOF'
    BEGIN {
        # Primitive expressions that cannot be derived.
        split("define do let let* letrec if cond case and or begin lambda delay delay-force set!", keywords);

        # Macro expressions.
        split("define-syntax let-syntax letrec-syntax syntax-rules syntax-case", meta);

        # Basic operators.
        split("* + - ... / < <= = => > >=", operators);

        # Procedures that create a base type and their predicates (for easier type checking)
        split("list vector bytevector cons string boolean? list? pair? "\
              "vector? bytevector? string? char? complex? eof-object? input-port? "\
              "null? number? output-port? procedure? symbol?", types);

        # R5RS available procedures
        split("abs acos angle append apply asin assoc assq assv atan "\
              "caaaar caaadr caaar caadar caaddr caadr "\
              "caar cadaar cadadr cadar caddar cadddr caddr cadr "\
              "call-with-current-continuation call-with-input-file "\
              "call-with-output-file call-with-values car cdaaar cdaadr cdaar "\
              "cdadar cdaddr cdadr cdar cddaar cddadr cddar cdddar cddddr cdddr "\
              "cddr cdr ceiling char->integer char-alphabetic? char-ci<=? "\
              "char-ci<? char-ci=? char-ci>=? char-ci>? char-downcase "\
              "char-lower-case? char-numeric? char-ready? char-upcase "\
              "char-upper-case? char-whitespace? char<=? char<? char=? "\
              "char>=? char>?  close-input-port close-output-port  cons cos "\
              "current-input-port current-output-port denominator display "\
              "dynamic-wind else  eq? equal? eqv? eval even? exact->inexact "\
              "exact? exp expt floor for-each force gcd imag-part inexact->exact "\
              "inexact?  integer->char integer? interaction-environment lcm "\
              "length list list->string list->vector list-ref list-tail  load log "\
              "magnitude make-polar make-rectangular make-string make-vector "\
              "map max member memq memv min modulo negative? newline not "\
              "null-environment  number->string numerator odd? open-input-file "\
              "open-output-file or peek-char positive?  quasiquote quote quotient "\
              "rational? rationalize read read-char real-part real? remainder "\
              "reverse round scheme-report-environment set-car! set-cdr! sin "\
              "sqrt string->list string->number string->symbol string-append "\
              "string-ci<=? string-ci<? string-ci=? string-ci>=? "\
              "string-ci>? string-copy string-fill! string-length string-ref "\
              "string-set! string<=? string<? string=? string>=? string>? "\
              "substring symbol->string   tan truncate values vector "\
              "vector->list vector-fill! vector-length vector-ref vector-set! "\
              "with-input-from-file with-output-to-file write write-char zero?",
              builtins);

        non_word_chars="[\\s\\(\\)\\[\\]\\{\\};\\|]";

        normal_identifiers="-!$%&\\*\\+\\./:<=>\\?\\^_~a-zA-Z0-9";
        identifier_chars="[" normal_identifiers "][" normal_identifiers ",#]*";
    }
    function add_highlighter(regex, highlight) {
        printf("add-highlighter shared/scheme/code/ regex \"%s\" %s\n", regex, highlight);
    }
    function quoted_join(words, quoted, first) {
        first=1
        for (i in words) {
            if (!first) { quoted=quoted "|"; }
            quoted=quoted "\\Q" words[i] "\\E";
            first=0;
        }
        return quoted;
    }
    function add_word_highlighter(words, face, regex) {
        regex = non_word_chars "+(" quoted_join(words) ")" non_word_chars
        add_highlighter(regex, "1:" face)
    }
    function print_words(words) {
        for (i in words) { printf(" %s", words[i]); }
    }

    BEGIN {
        printf("hook global WinSetOption filetype=scheme %%{ set-option window static_words ");
        print_words(keywords); print_words(meta); print_words(operators); print_words(builtins); 
        printf(" }\n")

        add_word_highlighter(keywords, "keyword");
        add_word_highlighter(meta, "meta");
        add_word_highlighter(operators, "operator");
        add_word_highlighter(builtins, "builtin");
        add_word_highlighter(types, "type");
        add_highlighter(non_word_chars "+('" identifier_chars ")", "1:attribute");
        add_highlighter("\\(define\\W+\\((" identifier_chars ")", "1:function");
        add_highlighter("\\(define\\W+(" identifier_chars ")\\W+\\(lambda", "1:function");
    }
EOF
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group scheme-highlight global WinSetOption filetype=scheme %{
    add-highlighter window/scheme ref scheme
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/scheme }
}


hook global WinSetOption filetype=scheme %{
    set-option buffer extra_word_chars '_' '-' '!' '%' '?' '<' '>' '='
    hook window InsertEnd  .* -group scheme-trim-indent  lisp-trim-indent
    hook window InsertChar \n -group scheme-indent lisp-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window scheme-.+ }
}
