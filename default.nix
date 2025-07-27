{
  pkgs ? import <nixpkgs> { },
  lib ? pkgs.lib,
  stdenv ? pkgs.stdenv,
}:

stdenv.mkDerivation {
  pname = "ceph-bluestore-parser";
  version = "0-unstable-2025-07-26";

  src = lib.cleanSource ./.;

  buildInputs = [
    pkgs.tomlplusplus
  ];

  buildPhase = ''
    $CXX main.cpp -DTOML_HEADER_ONLY=0 -ltomlplusplus -fdiagnostics-color=always -Wall -Wextra -ltomlplusplus -o ceph-bluestore-parser
  '';

  installPhase = ''
    install -Dm755 -t $out/bin/ ceph-bluestore-parser
  '';

  meta = {
    description = "Convert between Ceph BlueStore on-disk format and TOML.";
    mainProgram = "ceph-bluestore-parser";
    license = lib.licenses.agpl3Only;
    maintainers = with lib.maintainers; [ nagy ];
  };
}
