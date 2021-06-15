# eRuby
# http://www2a.biglobe.ne.jp/~seki/ruby/erb.html

hook global BufCreate '.*\.erb' %{
  set-option buffer filetype eruby
}

hook global WinSetOption filetype=eruby %{
  require-module eruby
  add-highlighter window/eruby ref eruby
  hook -group eruby-indent window InsertChar '\n' html-indent-on-new-line
  hook -always -once window WinSetOption filetype=.* %{
    remove-highlighter window/eruby
    remove-hooks window eruby-.+
  }
}

provide-module eruby %{
  require-module ruby
  require-module html
  add-highlighter shared/eruby regions
  add-highlighter shared/eruby/html default-region ref html
  add-highlighter shared/eruby/simple-expression-tag region '<%=' '%>' ref ruby
  add-highlighter shared/eruby/simple-execution-tag region '<%' '%>' ref ruby
  add-highlighter shared/eruby/simple-comment-tag region '<%#' '%>' fill comment
}
