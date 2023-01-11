with (import <nixpkgs> {});
derivation {
    name = "kakoune";
    system = builtins.currentSystem;

    builder = "${bash}/bin/bash";
    args = [ ./build.sh ];
    inherit gnumake clang;
    src = ./. ;
}
