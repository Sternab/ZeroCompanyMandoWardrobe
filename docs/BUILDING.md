# Building from source

The v0.4.0 DLL was built on Windows with Visual Studio 2022, CMake 3.22 or newer, and the RE-UE4SS source API at commit:

```text
a1e7f571c789f63f3de6773d056be6f778c14dc8
```

Clone UE4SS recursively and check out that commit:

```powershell
git clone --recursive https://github.com/UE4SS-RE/RE-UE4SS.git C:\src\RE-UE4SS
git -C C:\src\RE-UE4SS checkout a1e7f571c789f63f3de6773d056be6f778c14dc8
git -C C:\src\RE-UE4SS submodule update --init --recursive
```

Configure and build:

```powershell
cmake -S . -B build -A x64 -DRE_UE4SS_SOURCE_DIR=C:\src\RE-UE4SS
cmake --build build --config Game__Shipping__Win64 --target ZeroCompanyMandoWardrobe
```

The output DLL will be under:

```text
build\Game__Shipping__Win64\ZeroCompanyMandoWardrobe.dll
```

Depending on the generator, CMake may place it in a target-specific subdirectory. Rename it to `main.dll` when installing it as a UE4SS C++ mod.

The public release intentionally excludes PDB files because local source paths may be embedded in them. The release DLL is pinned to Zero Company Steam build 24874058 and is expected to refuse other executable builds.
