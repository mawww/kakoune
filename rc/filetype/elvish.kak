# Elvish
# https://elv.sh

hook global BufCreate '.*\.elv' %{
  set-option buffer filetype elvish
}

hook global WinSetOption filetype=elvish %{
  require-module elvish
  evaluate-commands set-option window static_words %opt{elvish_keywords}
  add-highlighter window/elvish ref elvish
  hook -group elvish window InsertChar '\n' elvish-new-line-inserted
  set-option window extra_word_chars '$' '~' '-'
  hook -always -once window WinSetOption filetype=.* %{
    remove-highlighter window/elvish
    remove-hooks window elvish
  }
}

provide-module elvish %{
  declare-option -hidden str-list elvish_keywords 'all' 'assoc' 'base' 'bool' 'break' 'cd' 'chr' 'constantly' 'continue' 'count' 'dir-history' 'dissoc' 'drop' 'each' 'eawk' 'echo' 'eq' 'esleep' 'eval-symlinks' 'exec' 'exit' 'explode' 'external' 'fail' 'fclose' 'fg' 'float64' 'fopen' 'from-json' 'from-lines' '-gc' 'get-env' 'has-env' 'has-external' 'has-key' 'has-prefix' 'has-suffix' 'has-value' '-ifaddrs' 'is' '-is-dir' 'joins' 'keys' 'kind-of' '-log' 'make-map' 'multi-error' 'nop' 'not' 'not-eq' 'ns' 'one' 'only-bytes' 'only-values' 'ord' 'order' '-override-wcwidth' 'path-abs' 'path-base' 'path-clean' 'path-dir' 'path-ext' 'peach' 'pipe' 'pprint' 'prclose' 'print' 'put' 'pwclose' 'rand' 'randint' 'range' 'read-line' 'read-upto' 'repeat' 'replaces' 'repr' 'resolve' 'return' 'run-parallel' 'search-external' 'set-env' 'slurp' '-source' 'splits' 'src' '-stack' 'styled' 'styled-segment' 'take' 'tilde-abbr' '-time' 'time' 'to-json' 'to-lines' 'to-string' 'unset-env' 'wcswidth'

  add-highlighter shared/elvish regions
  add-highlighter shared/elvish/code default-region group

  # Comments
  # https://elv.sh/ref/language.html#syntax-convention
  add-highlighter shared/elvish/comment region '#' '$' fill comment

  # String
  # https://elv.sh/ref/language.html#string
  add-highlighter shared/elvish/double_string region '"' '(?<!\\)"' regions
  add-highlighter shared/elvish/single_string region -recurse "(?<!')('')+(?!')" "(^|\h)\K'" "'(?!')" regions

  # Variables
  # https://elv.sh/ref/language.html#variable
  add-highlighter shared/elvish/code/variable regex '\$@?(?:[\w-]+:)*[\w-]+' 0:variable

  evaluate-commands %sh{
    # Keywords
    eval "set -- $kak_quoted_opt_elvish_keywords"
    regex="\\b(?:\\Q$1\\E"
    shift
    for keyword do
      regex="$regex|\\Q$keyword\\E"
    done
    regex="$regex)\\b"
    printf 'add-highlighter shared/elvish/code/keywords regex %s 0:keyword\n' "$regex"

    # String
    # https://elv.sh/ref/language.html#string
    for id in double_string single_string; do
      printf "
        add-highlighter shared/elvish/$id/fill default-region fill string
      "
    done
  }

  define-command -hidden elvish-new-line-inserted %{
    # Copy previous line indent
    try %{
      execute-keys -draft 'K<a-&>'
    }
    # Remove empty line indent
    try %{
      execute-keys -draft 'k<a-x>s^\h+$<ret>d'
    }
  }

  define-command -hidden elvish-fetch-keywords %{
    set-register dquote %sh{
      curl \
        https://raw.githubusercontent.com/elves/elvish/master/pkg/eval/builtin_fn_cmd.go \
        https://raw.githubusercontent.com/elves/elvish/master/pkg/eval/builtin_fn_container.go \
        https://raw.githubusercontent.com/elves/elvish/master/pkg/eval/builtin_fn_debug.go \
        https://raw.githubusercontent.com/elves/elvish/master/pkg/eval/builtin_fn_env.go \
        https://raw.githubusercontent.com/elves/elvish/master/pkg/eval/builtin_fn_flow.go \
        https://raw.githubusercontent.com/elves/elvish/master/pkg/eval/builtin_fn_fs.go \
        https://raw.githubusercontent.com/elves/elvish/master/pkg/eval/builtin_fn_io.go \
        https://raw.githubusercontent.com/elves/elvish/master/pkg/eval/builtin_fn_misc.go \
        https://raw.githubusercontent.com/elves/elvish/master/pkg/eval/builtin_fn_num.go \
        https://raw.githubusercontent.com/elves/elvish/master/pkg/eval/builtin_fn_pred.go \
        https://raw.githubusercontent.com/elves/elvish/master/pkg/eval/builtin_fn_str.go \
        https://raw.githubusercontent.com/elves/elvish/master/pkg/eval/builtin_fn_styled.go \
        https://raw.githubusercontent.com/elves/elvish/master/pkg/eval/builtin_ns.go |
      kak -f '%saddBuiltinFns<ret>glm1s^\h*"([\w-]{2,})":<ret>y%<a-R>a<ret><esc><a-_>a<del><esc>|sort<ret>'
    }
  }
}
