# Crystal
# https://crystal-lang.org

hook global BufCreate '.*\.cr' %{
  set-option buffer filetype crystal
}

hook global WinSetOption filetype=crystal %{
  require-module crystal
  set-option window static_words %opt(crystal_keywords)
  add-highlighter window/ ref crystal
  hook -group crystal window InsertChar '\n' crystal-new-line-inserted
  hook -always -once window WinSetOption filetype=.* %{
    remove-highlighter window/crystal
    remove-hooks window crystal
  }
}

provide-module crystal %🐈
  declare-option -hidden str-list crystal_keywords 'abstract' 'alias' 'annotation' 'as' 'asm' 'begin' 'break' 'case' 'class' 'def' 'do' 'else' 'elsif' 'end' 'ensure' 'enum' 'extend' 'false' 'for' 'fun' 'if' 'include' 'instance_sizeof' 'is_a?' 'lib' 'macro' 'module' 'next' 'nil' 'nil?' 'of' 'offsetof' 'out' 'pointerof' 'private' 'protected' 'require' 'rescue' 'responds_to?' 'return' 'select' 'self' 'sizeof' 'struct' 'super' 'then' 'true' 'type' 'typeof' 'uninitialized' 'union' 'unless' 'until' 'verbatim' 'when' 'while' 'with' 'yield'

  add-highlighter shared/crystal regions
  add-highlighter shared/crystal/code default-region group

  # Comments
  # https://crystal-lang.org/reference/syntax_and_semantics/comments.html
  # Avoid string literals with interpolation
  add-highlighter shared/crystal/comment region '#(?!\{)' '$' fill comment

  # String
  # https://crystal-lang.org/reference/syntax_and_semantics/literals/string.html
  add-highlighter shared/crystal/string region '"' '(?<!\\)"' regions

  # Percent string literals
  # https://crystal-lang.org/reference/syntax_and_semantics/literals/string.html#percent-string-literals
  add-highlighter shared/crystal/parenthesis-string region -recurse '\(' '%Q?\(' '\)' regions
  add-highlighter shared/crystal/bracket-string region -recurse '\[' '%Q?\[' '\]' regions
  add-highlighter shared/crystal/brace-string region -recurse '\{' '%Q?\{' '\}' regions
  add-highlighter shared/crystal/angle-string region -recurse '<' '%Q?<' '>' regions
  add-highlighter shared/crystal/pipe-string region '%Q?\|' '\|' regions
  # Raw
  # https://crystal-lang.org/reference/syntax_and_semantics/literals/string.html#percent-string-literals
  # https://crystal-lang.org/reference/syntax_and_semantics/literals/string.html#percent-string-array-literal
  add-highlighter shared/crystal/raw-parenthesis-string region -recurse '\(' '%[qw]\(' '\)' fill string
  add-highlighter shared/crystal/raw-bracket-string region -recurse '\[' '%[qw]\[' '\]' fill string
  add-highlighter shared/crystal/raw-brace-string region -recurse '\{' '%[qw]\{' '\}' fill string
  add-highlighter shared/crystal/raw-angle-string region -recurse '<' '%[qw]<' '>' fill string
  add-highlighter shared/crystal/raw-pipe-string region '%[qw]\|' '\|' fill string

  # Here document
  # https://crystal-lang.org/reference/syntax_and_semantics/literals/string.html#heredoc
  add-highlighter shared/crystal/heredoc region -match-capture '<<-(\w+)' '^\h*(\w+)$' regions
  # Raw
  add-highlighter shared/crystal/raw-heredoc region -match-capture "<<-'(\w+)'" '^\h*(\w+)$' regions
  add-highlighter shared/crystal/raw-heredoc/fill default-region fill string
  add-highlighter shared/crystal/raw-heredoc/interpolation region -recurse '\{' '#\{' '\}' fill meta

  # Regular expressions
  # https://crystal-lang.org/reference/syntax_and_semantics/literals/regex.html
  # https://crystal-lang.org/reference/syntax_and_semantics/literals/regex.html#modifiers
  add-highlighter shared/crystal/regex region '/' '(?<!\\)/[imx]*' regions
  # Avoid unterminated regular expression
  add-highlighter shared/crystal/division region ' / ' '.\K' group

  # Percent regex literals
  # https://crystal-lang.org/reference/syntax_and_semantics/literals/regex.html#percent-regex-literals
  add-highlighter shared/crystal/parenthesis-regex region -recurse '\(' '%r?\(' '\)[imx]*' regions
  add-highlighter shared/crystal/bracket-regex region -recurse '\[' '%r?\[' '\][imx]*' regions
  add-highlighter shared/crystal/brace-regex region -recurse '\{' '%r?\{' '\}[imx]*' regions
  add-highlighter shared/crystal/angle-regex region -recurse '<' '%r?<' '>[imx]*' regions
  add-highlighter shared/crystal/pipe-regex region '%r?\|' '\|[imx]*' regions

  # Command
  # https://crystal-lang.org/reference/syntax_and_semantics/literals/command.html
  add-highlighter shared/crystal/command region '`' '(?<!\\)`' regions

  # Percent command literals
  add-highlighter shared/crystal/parenthesis-command region -recurse '\(' '%x?\(' '\)' regions
  add-highlighter shared/crystal/bracket-command region -recurse '\[' '%x?\[' '\]' regions
  add-highlighter shared/crystal/brace-command region -recurse '\{' '%x?\{' '\}' regions
  add-highlighter shared/crystal/angle-command region -recurse '<' '%x?<' '>' regions
  add-highlighter shared/crystal/pipe-command region '%x?\|' '\|' regions

  evaluate-commands %sh[
    # Keywords
    eval "set -- $kak_opt_crystal_keywords"
    regex="\\b(?:$1"
    shift
    for keyword do
      regex="$regex|\\Q$keyword\\E"
    done
    regex="$regex)\\b"
    printf 'add-highlighter shared/crystal/code/keywords regex %s 0:keyword\n' "$regex"

    # Interpolation
    # String
    # https://crystal-lang.org/reference/syntax_and_semantics/literals/string.html#interpolation
    for id in string parenthesis-string bracket-string brace-string angle-string pipe-string heredoc; do
      printf "
        add-highlighter shared/crystal/$id/fill default-region fill string
        add-highlighter shared/crystal/$id/interpolation region -recurse '\\{' '#\\{' '\\}' ref crystal
      "
    done

    # Regular expressions
    # https://crystal-lang.org/reference/syntax_and_semantics/literals/regex.html#interpolation
    for id in regex parenthesis-regex bracket-regex brace-regex angle-regex pipe-regex; do
      printf "
        add-highlighter shared/crystal/$id/fill default-region fill meta
        add-highlighter shared/crystal/$id/interpolation region -recurse '\\{' '#\\{' '\\}' ref crystal
      "
    done

    # Command
    for id in command parenthesis-command bracket-command brace-command angle-command pipe-command; do
      printf "
        add-highlighter shared/crystal/$id/fill default-region fill meta
        add-highlighter shared/crystal/$id/interpolation region -recurse '\\{' '#\\{' '\\}' ref crystal
      "
    done
  ]

  define-command -hidden crystal-new-line-inserted %{
    # Copy previous line indent
    try %(execute-keys -draft 'K<a-&>')
    # Remove empty line indent
    try %(execute-keys -draft 'k<a-x>s^\h+$<ret>d')
  }
  define-command -hidden crystal-fetch-keywords %{
    set-register dquote %sh{
      curl --location https://github.com/crystal-lang/crystal/raw/master/src/compiler/crystal/syntax/lexer.cr |
      kak -f '%1scheck_ident_or_keyword\(:(\w+\??), \w+\)<ret>y%<a-R>a<ret><esc><a-_>a<del><esc>|sort<ret>'
    }
  }
🐈
