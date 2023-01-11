PATH="${gnumake}/bin:${clang}/bin:/usr/bin:/bin"

cp -r $src/* .
chmod -R 774 .
cd src
make -j $NIX_BUILD_CORES
PREFIX=$out make install
