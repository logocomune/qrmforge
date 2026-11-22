# AppImage type 2 runtime

The x86-64 runtime pinned by `packaging/build_appimage.py` identifies itself
as [fedfad5](https://github.com/AppImage/type2-runtime/commit/fedfad5).
Its SHA-256 is `617aa77314d57c8b8e4fae9c3f0b35af7b30d5529cb69e98f64f92510ed98a46`.

| Component | Terms and upstream source | Evidence |
| --- | --- | --- |
| Runtime | [MIT](https://github.com/AppImage/type2-runtime/blob/fedfad5/LICENSE) | Embedded revision |
| libfuse 3.15.0 | [LGPL 2.1 for the library](https://github.com/libfuse/libfuse/tree/fuse-3.15.0) | Build recipe pins version and applies the retained mount patch |
| Squashfuse 0.5.2 | [BSD-2-Clause](https://github.com/vasi/squashfuse/blob/0.5.2/LICENSE) | Build recipe pins version |
| musl | [MIT and file-specific notices](https://github.com/ifduyue/musl/blob/v1.2.5/COPYRIGHT) | Alpine 3.21 recipe, reference text from v1.2.5 |
| Zstandard | [BSD-3-Clause option](https://github.com/facebook/zstd/blob/v1.5.6/LICENSE) | Static link in recipe, reference text from v1.5.6 |
| zlib | [zlib license](https://github.com/madler/zlib/blob/v1.3.1/LICENSE) | Static link in recipe, reference text from v1.3.1 |
| mimalloc | [MIT](https://github.com/microsoft/mimalloc/blob/v2.1.7/LICENSE) | Static link in recipe and allocator strings in ELF, reference text from v2.1.7 |

The recipe uses an Alpine 3.21 image and unversioned APK dependencies.
Reference texts for those packages are preliminary because the exact APK
revisions and distribution patches used in the binary remain unverified.
The tool used to create the AppImage is a separate build dependency.

## Release materials still needed

The [pinned source archive](https://codeload.github.com/AppImage/type2-runtime/tar.gz/fedfad5)
includes the build recipe and FUSE patch. The reviewed archive SHA-256 is
`21b78d2be9c81347880f0c8cb85cda890dc3d05c148e606f174acebb68aa364c`.
The [libfuse source release](https://github.com/libfuse/libfuse/releases/tag/fuse-3.15.0)
is the recipe's input. Before distribution, verify the precise build inputs
or rebuild from fully pinned sources. Supply the corresponding libfuse
source and patch, and the source or object files and build materials
needed to relink the runtime with a modified libfuse under LGPL 2.1
section 6. Verify the rebuilt runtime can be substituted when wrapping
the payload. This directory supplies attribution and provenance evidence.
