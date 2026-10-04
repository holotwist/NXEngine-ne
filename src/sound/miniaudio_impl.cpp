#define STB_VORBIS_HEADER_ONLY
#if __has_include("std_vorbis.c")
#include "std_vorbis.c"
#elif __has_include("stb_vorbis.c")
#include "stb_vorbis.c"
#endif

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#undef STB_VORBIS_HEADER_ONLY
#if __has_include("std_vorbis.c")
#include "std_vorbis.c"
#elif __has_include("stb_vorbis.c")
#include "stb_vorbis.c"
#endif