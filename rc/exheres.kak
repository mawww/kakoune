hook global BufCreate .*\.(exheres-0|exlib) %{
    set buffer filetype sh
}

