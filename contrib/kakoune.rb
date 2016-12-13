require "formula"

class Kakoune < Formula
  homepage "https://github.com/mawww/kakoune"
  head "https://github.com/mawww/kakoune.git"

  depends_on 'boost'
  depends_on 'docbook-xsl' => :build
  depends_on 'asciidoc' => [:build, 'with-docbook-xsl']

  def install
    ENV["XML_CATALOG_FILES"] = "#{etc}/xml/catalog"

    cd 'src' do
      system "make", "install", "PREFIX=#{prefix}"
    end
  end
end
