{
  description = "Raylib in C";

  inputs = {
    nixpkgs.url = "nixpkgs";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs { inherit system; };
        ffmpegSmall = pkgs.ffmpeg_7.override {
          ffmpegVariant = "small";
          withVpx = true; # VP9 encoder
          withAom = true; # AV1 encoder (optional)
          withSvtav1 = false; # or true if you prefer SVT-AV1
        };
      in
      {
        devShells.default = pkgs.mkShell {
          nativeBuildInputs = with pkgs; [
            gcc
            gdb
            clang-tools
            cmake
            cmake-language-server
            bear
            pkg-config
          ];

          buildInputs = with pkgs; [
            libGL

            wayland
            wayland-protocols
            libxkbcommon

            raylib

            ffmpegSmall
          ];
        };
      }
    );
}
