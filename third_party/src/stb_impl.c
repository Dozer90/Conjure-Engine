/*
 * Single translation unit holding the stb implementations so the headers can be included
 * freely everywhere else without duplicate symbols.
 */

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STBI_NO_PNM
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
