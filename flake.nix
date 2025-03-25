{
  description = "Bluetooth transmitter project flake";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    nixpkgs-esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";
  };

  outputs = { self, nixpkgs, flake-utils, nixpkgs-esp-dev, ... }:
    flake-utils.lib.eachDefaultSystem (system:
    
      let
  	  overlays = [ (import nixpkgs-esp-dev.overlays.default) ];
  	  pkgs = import nixpkgs { inherit system overlays; };
      
      in {
        devShell = nixpkgs-esp-dev.devShells.${system}.esp-idf-full;
      }
    );
}
