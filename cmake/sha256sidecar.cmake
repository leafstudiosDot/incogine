# Incogine — SHA-256 sidecar writer for Studio dev-build binding.
# Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#
# Invoked as:
#   cmake -DEXE=<game executable> -DOUT=<sidecar path> -P cmake/sha256sidecar.cmake
# Writes "<lowercase-hex>  <exe>\n" so Studio can verify the binary it is
# about to preview is byte-identical to this development build.
if(NOT DEFINED EXE OR NOT DEFINED OUT)
    message(FATAL_ERROR "sha256sidecar.cmake requires -DEXE and -DOUT")
endif()
file(SHA256 "${EXE}" HASH)
file(WRITE "${OUT}" "${HASH}  ${EXE}\n")
