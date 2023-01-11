# http://clojure.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](clj|cljc|cljs|cljx|edn) %{
    set-option buffer filetype clojure
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾
hook global WinSetOption filetype=clojure %{
    require-module clojure
    clojure-configure-window
}

hook -group clojure-highlight global WinSetOption filetype=clojure %{
    add-highlighter window/clojure ref clojure
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/clojure }
}

hook -group clojure-insert global BufNewFile .*[.](clj|cljc|cljs|cljx) %{
    require-module clojure
    clojure-insert-ns
}

provide-module clojure %{

require-module lisp

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/clojure regions
add-highlighter shared/clojure/code default-region group
add-highlighter shared/clojure/comment region '(?<!\\)(?:\\\\)*\K;' '$'                 fill comment
add-highlighter shared/clojure/shebang region '(?<!\\)(?:\\\\)*\K#!' '$'                fill comment
add-highlighter shared/clojure/string  region '(?<!\\)(?:\\\\)*\K"' '(?<!\\)(?:\\\\)*"' fill string

add-highlighter shared/clojure/code/ regex \b(nil|true|false)\b 0:value
add-highlighter shared/clojure/code/ regex \
    \\(?:space|tab|newline|return|backspace|formfeed|u[0-9a-fA-F]{4}|o[0-3]?[0-7]{1,2}|.)\b 0:string

evaluate-commands %sh{
    exec awk -f - <<'EOF'
    BEGIN{
        symbol_char="[^\\s()\\[\\]{}\"\\;@^`~\\\\%/]";
        in_core="(clojure\\.core/|(?<!/))";
        split( \
        "case cond condp cond-> cond->> def definline definterface defmacro defmethod "\
        "defmulti defn defn- defonce defprotocol defrecord defstruct deftype fn if "\
        "if-let if-not if-some let letfn new ns when when-first when-let when-not "\
        "when-some . ..", keywords);

        split( \
        "* *' + +' - -' -> ->> ->ArrayChunk ->Eduction ->Vec ->VecNode ->VecSeq / < "\
        "<= = == > >= StackTraceElement->vec Throwable->map accessor aclone "\
        "add-classpath add-watch agent agent-error agent-errors aget alength alias "\
        "all-ns alter alter-meta! alter-var-root amap ancestors and any? apply "\
        "areduce array-map as-> aset aset-boolean aset-byte aset-char aset-double "\
        "aset-float aset-int aset-long aset-short assert assoc assoc! assoc-in "\
        "associative? atom await await-for bases bean bigdec bigint biginteger "\
        "binding bit-and bit-and-not bit-clear bit-flip bit-not bit-or bit-set "\
        "bit-shift-left bit-shift-right bit-test bit-xor boolean boolean-array "\
        "boolean? booleans bound-fn bound-fn* bound? bounded-count butlast byte "\
        "byte-array bytes bytes?  cast cat catch char char-array char-escape-string "\
        "char-name-string char? chars class class? clear-agent-errors "\
        "clojure-version coll? comment commute comp comparator compare "\
        "compare-and-set! compile complement completing concat  conj conj! cons "\
        "constantly construct-proxy contains? count counted? create-ns "\
        "create-struct cycle dec dec' decimal? declare dedupe default-data-readers "\
        "delay delay? deliver denominator deref derive descendants disj disj! "\
        "dissoc dissoc! distinct distinct? do doall dorun doseq dosync dotimes doto "\
        "double double-array double? doubles drop drop-last drop-while eduction "\
        "empty empty? ensure ensure-reduced enumeration-seq error-handler "\
        "error-mode eval even? every-pred every? ex-data ex-info extend "\
        "extend-protocol extend-type extenders extends? false? ffirst file-seq "\
        "filter filterv finally find find-keyword find-ns find-var first flatten "\
        "float float-array float? floats flush fn? fnext fnil for force format "\
        "frequencies future future-call future-cancel future-cancelled? "\
        "future-done? future? gen-class gen-interface gensym get get-in get-method "\
        "get-proxy-class get-thread-bindings get-validator group-by halt-when hash "\
        "hash-map hash-ordered-coll hash-set hash-unordered-coll ident? identical? "\
        "identity ifn? import in-ns inc inc' indexed? init-proxy inst-ms inst? "\
        "instance? int int-array int? integer? interleave intern interpose into "\
        "into-array ints io! isa? iterate iterator-seq juxt keep keep-indexed key "\
        "keys keyword keyword? last lazy-cat lazy-seq line-seq list list* list? "\
        "load load-file load-reader load-string loaded-libs locking long long-array "\
        "longs loop macroexpand macroexpand-1 make-array make-hierarchy map "\
        "map-entry? map-indexed map? mapcat mapv max max-key memfn memoize merge "\
        "merge-with meta methods min min-key mix-collection-hash mod monitor-enter "\
        "monitor-exit name namespace namespace-munge nat-int? neg-int? neg? newline "\
        "next nfirst nil? nnext not not-any? not-empty not-every? not= ns-aliases "\
        "ns-imports ns-interns ns-map ns-name ns-publics ns-refers ns-resolve "\
        "ns-unalias ns-unmap nth nthnext nthrest num number? numerator object-array "\
        "odd? or parents partial partition partition-all partition-by pcalls peek "\
        "persistent! pmap pop pop! pop-thread-bindings pos-int? pos? pr pr-str "\
        "prefer-method prefers print print-str printf println println-str prn "\
        "prn-str promise proxy proxy-mappings proxy-super push-thread-bindings "\
        "pvalues qualified-ident? qualified-keyword? qualified-symbol? quot quote "\
        "rand rand-int rand-nth random-sample range ratio? rational? rationalize "\
        "re-find re-groups re-matcher re-matches re-pattern re-seq read read-line "\
        "read-string reader-conditional reader-conditional? realized? record? recur "\
        "reduce reduce-kv reduced reduced? reductions ref ref-history-count "\
        "ref-max-history ref-min-history ref-set refer refer-clojure reify "\
        "release-pending-sends rem remove remove-all-methods remove-method "\
        "remove-ns remove-watch repeat repeatedly replace replicate require reset! "\
        "reset-meta! reset-vals! resolve rest restart-agent resultset-seq reverse "\
        "reversible? rseq rsubseq run! satisfies? second select-keys send send-off "\
        "send-via seq seq? seqable? seque sequence sequential? set set! "\
        "set-agent-send-executor! set-agent-send-off-executor! set-error-handler! "\
        "set-error-mode! set-validator! set? short short-array shorts shuffle "\
        "shutdown-agents simple-ident? simple-keyword? simple-symbol? slurp some "\
        "some-> some->> some-fn some? sort sort-by sorted-map sorted-map-by "\
        "sorted-set sorted-set-by sorted? special-symbol? spit split-at split-with "\
        "str string? struct struct-map subs subseq subvec supers swap! swap-vals! "\
        "symbol symbol? sync tagged-literal tagged-literal? take take-last take-nth "\
        "take-while test the-ns thread-bound? throw time to-array to-array-2d "\
        "trampoline transduce transient tree-seq true? try type unchecked-add "\
        "unchecked-add-int unchecked-byte unchecked-char unchecked-dec "\
        "unchecked-dec-int unchecked-divide-int unchecked-double unchecked-float "\
        "unchecked-inc unchecked-inc-int unchecked-int unchecked-long "\
        "unchecked-multiply unchecked-multiply-int unchecked-negate "\
        "unchecked-negate-int unchecked-remainder-int unchecked-short "\
        "unchecked-subtract unchecked-subtract-int underive unreduced "\
        "unsigned-bit-shift-right update update-in update-proxy uri? use uuid? val "\
        "vals var var-get var-set var? vary-meta vec vector vector-of vector? "\
        "volatile! volatile? vreset! vswap!  while with-bindings with-bindings* "\
        "with-in-str with-local-vars with-meta with-open with-out-str "\
        "with-precision with-redefs with-redefs-fn xml-seq zero? zipmap", core_fns);

        split( \
        "*1 *2 *3 *agent* *clojure-version* *command-line-args* *compile-files* "\
        "*compile-path* *compiler-options* *data-readers* *default-data-reader-fn* "\
        "*e *err* *file* *flush-on-newline* *in* *ns* *out* *print-dup* "\
        "*print-length* *print-level* *print-meta* *print-namespace-maps* "\
        "*print-readably* *read-eval* *unchecked-math* *warn-on-reflection*", core_vars);
    }
    function print_word_highlighter(words, face, first) {
        printf("add-highlighter shared/clojure/code/ regex (?<!%s)%s(", \
               symbol_char, in_core);
        first = 1;
        for (i in words) {
            if (!first) { printf("|"); }
            printf("\\Q%s\\E", words[i]);
            first = 0;
        }
        printf(")(?!%s) 0:%s\n", symbol_char, face);
    }
    function print_static_words(words) {
        for (i in words) {
            printf("%s clojure.core/%s ", words[i], words[i]);
        }
    }
    BEGIN{
        # Keywords
        printf("add-highlighter shared/clojure/code/ regex ::?(%s+/)?%s+ 0:value\n", symbol_char, symbol_char);

        # Numbers
        printf("add-highlighter shared/clojure/code/ regex (?<!%s)[-+]?(?:0(?:[xX][0-9a-fA-F]+|[0-7]*)|[1-9]\\d*)N? 0:value\n", symbol_char);
        printf("add-highlighter shared/clojure/code/ regex (?<!%s)[-+]?(?:0|[1-9]\\d*)(?:\\.\\d*)(?:M|[eE][-+]?\\d+)? 0:value\n", symbol_char);
        printf("add-highlighter shared/clojure/code/ regex (?<!%s)[-+]?(?:0|[1-9]\\d*)/(?:0|[1-9]\\d*) 0:value\n", symbol_char);

        print_word_highlighter(keywords, "keyword");
        print_word_highlighter(core_fns, "function");
        print_word_highlighter(core_vars, "variable");

        printf("declare-option str-list clojure_static_words ")
        print_static_words(keywords);
        print_static_words(core_fns);
        print_static_words(core_vars);
        printf("\n");
    }
EOF
}

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden clojure-configure-window %{
    set-option window static_words %opt{clojure_static_words}

    hook window ModeChange pop:insert:.* -group clojure-trim-indent clojure-trim-indent
    hook window InsertChar \n -group clojure-indent clojure-indent-on-new-line

    set-option buffer extra_word_chars '_' . / * ? + - < > ! : "'"
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window clojure-.+ }
}

define-command -hidden clojure-trim-indent lisp-trim-indent

declare-option \
    -docstring 'regex matching the head of forms which have options *and* indented bodies' \
    regex clojure_special_indent_forms \
    '(?:def.*|doseq|for|fn\*?|if(-.*|)|let.*|loop|ns|testing|with-.*|when(-.*|))'

define-command -hidden clojure-indent-on-new-line %{
    # registers: i = best align point so far; w = start of first word of form
    evaluate-commands -draft -save-regs '/"|^@iw' -itersel %{
        execute-keys -draft 'gk"iZ'
        try %{
            execute-keys -draft '[bl"i<a-Z><gt>"wZ'

            try %{
                # If a special form, indent another (indentwidth - 1) spaces
                execute-keys -draft '"wze<a-K>[\s()\[\]\{\}]<ret><a-k>\A' %opt{clojure_special_indent_forms} '\z<ret>'
                execute-keys -draft '"wze<a-L>s.{' %sh{printf $(( kak_opt_indentwidth - 1 ))} '}\K.*<ret><a-;>;"i<a-Z><gt>'
            } catch %{
                # If not special and parameter appears on line 1, indent to parameter
                execute-keys -draft '"wz<a-K>[()[\]{}]<ret>e<a-K>[\s()\[\]\{\}]<ret><a-l>s\h\K[^\s].*<ret><a-;>;"i<a-Z><gt>'
            }
        }
        try %{ execute-keys -draft '[rl"i<a-Z><gt>' }
        try %{ execute-keys -draft '[Bl"i<a-Z><gt>' }
        execute-keys -draft ';"i<a-z>a&,'
    }
}

declare-option -docstring %{
    top-level directories which can contain clojure files
    e.g. '(src|test|dev)'
} regex clojure_source_directories '(src|test|dev)'

define-command -docstring %{clojure-insert-ns: Insert namespace directive at top of Clojure source file} \
    clojure-insert-ns %{
    evaluate-commands -draft %{
        execute-keys -save-regs '' 'gk\O' "%val{bufname}" '<esc>giZ'
        try %{ execute-keys 'z<a-l>s\.clj[csx]?$<ret><a-d>' }
        try %{ execute-keys 'z<a-l>s^' "%opt{clojure_source_directories}" '/<ret><a-d>' }
        try %{ execute-keys 'z<a-l>s/<ret>r.' }
        try %{ execute-keys 'z<a-l>s_<ret>r-' }
        execute-keys 'z<a-l>\c(ns <c-r>")<ret><esc>'
    }
}

}
