{
  pkgs ? import <nixpkgs> { },
  lib ? pkgs.lib,
  stdenv ? pkgs.stdenv,
}:

stdenv.mkDerivation {
  pname = "ceph-bluestore-parser";
  version = "0-unstable-2025-07-28";

  src = lib.cleanSource ./.;

  buildInputs = [
    pkgs.boost
    pkgs.tomlplusplus
  ];

  nativeBuildInputs = [
    pkgs.pkg-config
    pkgs.meson
    pkgs.ninja
  ];

  meta = {
    description = "Convert between Ceph BlueStore on-disk format and TOML.";
    mainProgram = "ceph-bluestore-parser";
    license = lib.licenses.agpl3Only;
    maintainers = with lib.maintainers; [ nagy ];
  };
}
