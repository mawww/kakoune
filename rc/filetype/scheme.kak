# http://www.scheme-reports.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate (.*/)?(.*\.(scm|ss|sld|sps|sls)) %{
    set-option buffer filetype scheme
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=scheme %{
    require-module scheme

    set-option window static_words %opt{scheme_static_words}

    set-option buffer extra_word_chars '!' '$' '%' '&' '*' '+' '-' '.' '/' ':' '<' '=' '>' '?' '@' '^' '_' '~'
    hook window ModeChange pop:insert:.* -group scheme-trim-indent lisp-trim-indent
    hook window InsertChar \n -group scheme-indent lisp-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window scheme-.+ }
}

hook -group scheme-highlight global WinSetOption filetype=scheme %{
    add-highlighter window/scheme ref scheme
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/scheme }
}

provide-module scheme %{

require-module lisp

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/scheme regions
add-highlighter shared/scheme/code default-region group

add-highlighter shared/scheme/string region %{(?<!#\\)"} (?<!\\)(\\\\)*" fill string
add-highlighter shared/scheme/comment region %{(?<!#\\);} '$' fill comment
add-highlighter shared/scheme/comment-form region -recurse "\(" "#;\(" "\)" fill comment
add-highlighter shared/scheme/comment-block region "#\|" "\|#" fill comment
add-highlighter shared/scheme/quoted-form region -recurse "\(" "'\(" "\)" fill variable

add-highlighter shared/scheme/code/ regex (#t|#f) 0:value

# Numbers
add-highlighter shared/scheme/code/ regex '(#[bB]#[eiEI]|#[eiEI]#[bB]|#[bB])[01]+' 0:value
add-highlighter shared/scheme/code/ regex '(#[oO]#[eiEI]|#[eiEI]#[oO]|#[oO])[0-7]+' 0:value
add-highlighter shared/scheme/code/ regex '(#[dD](#[eiEI])?|#[eiEI]#[dD]|#[eiEI])(\d+(?:\.\d*)?|\.\d+)([esfdlESFDL][-+]?\d+)?' 0:value
add-highlighter shared/scheme/code/ regex '(#[xX]#[eiEI]|#[eiEI]#[xX]|#[xX])[0-9a-fA-F]+' 0:value

add-highlighter shared/scheme/code/ regex (#\\((\w+)|(.))) 0:value

add-highlighter shared/scheme/code/ regex '#!(?:no-)?fold-case\b' 0:meta

evaluate-commands %sh{ exec awk -f - <<'EOF'
    BEGIN {
        split("and begin call-with-current-continuation call/cc case case-lambda cond define "\
              "define-record-type define-values delay delay-force do else guard if lambda "\
              "let let* let-values let*-values letrec letrec* or set! unless when", keywords);

        # Macro expressions, imports/exports/library
        split("begin-syntax cond-expand define-library define-syntax export import include "\
              "include-ci include-library-declarations let-syntax letrec-syntax quote "\
              "quasiquote syntax-rules syntax-case unquote unquote-splicing", meta);

        # Basic operators.
        split("* + - ... / < <= = => > >=", operators);

        # Procedures that create a base type and their predicates (for easier type checking)
        split("list vector bytevector cons string boolean? list? pair? vector? bytevector? "\
              "string? char? complex? eof-object eof-object? input-port? null? number? "\
              "output-port? port? procedure? symbol?", types);

        # R7RS available procedures
        split("abs acos angle append apply asin assoc assq assv atan boolean=? "\
              "bytevector-append bytevector-copy bytevector-copy! bytevector-length "\
              "bytevector-u8-ref bytevector-u8-set! caaaar caaadr caaar caadar caaddr caadr "\
              "caar cadaar cadadr cadar caddar cadddr caddr cadr call-with-input-file "\
              "call-with-output-file call-with-values car cdaaar cdaadr cdaar cdadar cdaddr "\
              "cdadr cdar cddaar cddadr cddar cdddar cddddr cdddr cddr cdr ceiling "\
              "char->integer char-alphabetic? char-ci<=? char-ci<? char-ci=? char-ci>=? "\
              "char-ci>? char-downcase char-foldcase char-lower-case? char-numeric? "\
              "char-ready? char-upcase char-upper-case? char-whitespace? char<=? char<? "\
              "char=? char>=? char>? close-input-port close-output-port close-port cons cos "\
              "current-input-port current-output-port denominator digit-value display "\
              "dynamic-wind eq? equal? eqv? error error-object-irritants "\
              "error-object-message error-object? eval even? exact exact->inexact "\
              "exact-integer? exact-integer-sqrt exact? exp expt features file-error? "\
              "finite? floor floor/ floor-quotient floor-remainder for-each force "\
              "flush-output-port gcd get-output-bytevector get-output-string guard imag-part "\
              "inexact->exact inexact inexact? infinite? input-port-open? integer->char "\
              "integer? interaction-environment lcm length list list-copy list-set! "\
              "list->string list->vector list-ref list-tail load log magnitude "\
              "make-bytevector make-list make-parameter make-polar make-promise "\
              "make-rectangular make-string make-vector map max member memq memv min modulo "\
              "nan? negative? newline not null-environment number->string numerator odd? "\
              "open-input-bytevector open-input-file open-input-string "\
              "open-output-bytevector open-output-file open-output-string output-port-open? "\
              "or parameterize peek-char peek-u8 positive? promise? quotient raise "\
              "raise-continuable rational? rationalize read read-bytevector read-bytevector! "\
              "read-char read-error? read-line read-string read-u8 real-part real? remainder "\
              "reverse round scheme-report-environment set-car! set-cdr! sin square sqrt "\
              "string->list string->number string->symbol string->utf8 string->vector "\
              "string-append string-ci<=? string-ci<? string-ci=? string-ci>=? string-ci>? "\
              "string-copy string-copy! string-downcase string-fill! string-foldcase "\
              "string-for-each string-length string-map string-ref string-set! string-upcase "\
              "string<=? string<? string=? string>=? string>? substring symbol=? "\
              "symbol->string syntax-error tan textual-port? truncate truncate/ "\
              "truncate-quotient truncate-remainder u8-ready? unless utf8->string values "\
              "vector vector->list vector->string vector-append vector-copy vector-copy! "\
              "vector-for-each vector-fill! vector-length vector-map vector-ref vector-set! "\
              "when with-exception-handler with-input-from-file with-output-to-file write "\
              "write-bytevector write-char write-string write-u8 zero?", builtins);

        non_word_chars="['\"\\s\\(\\)\\[\\]\\{\\};]";

        normal_identifiers="-!$%&\\*\\+\\./:<=>\\?@\\^_~a-zA-Z0-9";
        identifier_chars="[" normal_identifiers "][" normal_identifiers ",#]*";
    }
    function kak_escape(s) {
        gsub(/'/, "''", s);
        return "'" s "'";
    }
    function add_highlighter(regex, highlight) {
        printf("add-highlighter shared/scheme/code/ regex %s %s\n", kak_escape(regex), highlight);
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
        regex = "(?<![" normal_identifiers "])(" quoted_join(words) ")(?![" normal_identifiers "])";
        add_highlighter(regex, "1:" face);
    }
    function print_words(words) {
        for (i in words) { printf(" %s", words[i]); }
    }

    BEGIN {
        printf("declare-option str-list scheme_static_words ");
        print_words(keywords); print_words(meta); print_words(operators); print_words(builtins);
        printf("\n");

        add_word_highlighter(keywords, "keyword");
        add_word_highlighter(meta, "meta");
        add_word_highlighter(operators, "operator");
        add_word_highlighter(builtins, "function");
        add_word_highlighter(types, "type");
        add_highlighter(non_word_chars "+('" identifier_chars ")", "1:attribute");
        add_highlighter("\\(define\\W+\\((" identifier_chars ")", "1:function");
        add_highlighter("\\(define\\W+(" identifier_chars ")\\W+\\(lambda", "1:function");

        # unprefixed decimals
        add_highlighter("(?<![" normal_identifiers "])(\\d+(\\.\\d*)?|\\.\\d+)(?:[esfdlESFDL][-+]?\\d+)?(?![" normal_identifiers "])", "0:value");
        # inf and nan
        add_highlighter("(?<![" normal_identifiers "])[+-](?:inf|nan)\.0(?![" normal_identifiers "])", "0:value");
    }
EOF
}

}
