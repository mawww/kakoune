PATH="${gnumake}/bin:${clang}/bin:/usr/bin:/bin"

cp -r $src/* .
chmod -R 774 .
cd src
# TODO: it seems most nix projects use -j $NIX_BUILD_CORES
# However by default it is 0, which makes make die
make -j
PREFIX=$out make install
