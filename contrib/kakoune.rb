require "formula"

class Kakoune < Formula
  homepage "https://github.com/mawww/kakoune"
  head "https://github.com/mawww/kakoune.git"

  depends_on 'icu4c'
  depends_on 'boost' => 'with-icu4c'
  depends_on 'asciidoc'

  def install
    cd 'src' do
      system "make", "install", "PREFIX=#{prefix}"
    end
  end
end
