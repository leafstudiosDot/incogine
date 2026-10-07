// Stub for the `miniz_export.h` header that upstream miniz generates with
// CMake. Incogine builds miniz as a static library that is never a DLL, so
// the export macro is intentionally empty. If miniz is ever built shared,
// regenerate this file from upstream's CMake instead of hand-editing it.
#ifndef MINIZ_EXPORT_H
#define MINIZ_EXPORT_H

#define MINIZ_EXPORT
#define MINIZ_NO_EXPORT

#endif // MINIZ_EXPORT_H
