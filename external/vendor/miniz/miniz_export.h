#ifndef MINIZ_EXPORT_H
#define MINIZ_EXPORT_H

/* Static vendored build: public symbols need no platform annotations. */
#define MINIZ_EXPORT
#define MINIZ_NO_EXPORT
#define MINIZ_DEPRECATED
#define MINIZ_DEPRECATED_EXPORT MINIZ_EXPORT MINIZ_DEPRECATED
#define MINIZ_DEPRECATED_NO_EXPORT MINIZ_NO_EXPORT MINIZ_DEPRECATED

#endif
