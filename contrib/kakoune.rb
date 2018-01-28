class Kakoune < Formula
  homepage "https://github.com/mawww/kakoune"
  head "https://github.com/mawww/kakoune.git"

  depends_on "docbook-xsl" => :build
  depends_on "ncurses" => [:build, :recommended]
  depends_on "asciidoc" => :build

  unless OS.mac?
    depends_on "libxslt" => :build
    depends_on "pkg-config" => :build
  end

  def install
    ENV["XML_CATALOG_FILES"] = "#{etc}/xml/catalog"

    cd "src" do
      system "make", "install", "PREFIX=#{prefix}", "debug=no"
    end
  end
end
