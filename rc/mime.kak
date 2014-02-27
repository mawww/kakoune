decl str mimetype "text/plain"

hook global BufOpen .* %{
     set buffer mimetype %sh{file -b --mime-type ${kak_buffile} }
}
