Selections
==========

Selecting text with H, J, K, L
------------------------------

A selection is an oriented range of characters of the current buffer. The
starting character of the selection is named the anchor. The ending character
is the cursor.

In Kakoune, there is always at least one selection. Most of time it's one 
character long, it's what you see as a cursor. In fact, the anchor and the
cursor overlap, and the anchor moves with the cursor in normal moves.

Editing commands, like `d`, `i`, and even `A` are applied to the whole
selection:

 - `d` deletes the selection
 - `i` inserts before the selection
 - `A` moves the selection to the last character of the line and append text

Some move commands perform selecting moves, i.e. the anchor is fixed and only
the cursor moves, effectively selecting a portion of text. Generaly a
selecting move is performed pressing shift on the non-selecting counterpart.

  1. Move somewhere in the buffer using `h`, `j`, `k`, `l`

  2. Move using `H`, `J`, `K`, `L`

  3. Try `d`, `i`, and `A` on the selected text

  4. Move to the the 'a' of 'all' on the first line marked --->

  5. Select 'all' using `L`

  6. Press `i` to insert 'are ' before 'all', then <esc> to return to normal
     mode.
     
  7. Press `d` to delete 'are ' and insert (`i`) the missing text, then <esc>

  8. When comfortable, go to the next lesson

...> There all words
...> There are missing words here.

NOTE: Kakoune also supports multiple selection, as we will see in a future
      lesson
      
NOTE: Remember to use `<esc>,r` to reload the page
