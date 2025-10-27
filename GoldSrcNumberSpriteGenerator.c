#include <stdio.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define NOB_IMPLEMENTATION
#include "nob.h"

#define BASE_TEN_DIGIT_COUNT 10
#define stbtt_BakeFontBitmap_PADDING 1
#define OUTPUT_NAME "digits"
#define OUTPUT_FILE OUTPUT_NAME".spr"
#define SPRITE_PREFIX "digits_number_"

/* TODO: Make y res editable */
#define Y_RES 640

#define SPRITE_VERSION 2
#define VP_PARALLEL 2
#define SPR_ADDITIVE 2
#define PALLETE_LENGTH 256

/* From https://developer.valvesoftware.com/wiki/SPR#Technical */

#pragma pack(push, 1)
typedef struct sprite_header_s
{
    char        id[4]; /* int on the website */          // format ID, "IDSP" (0x49 0x44 0x53 0x50)
    int         version;        // Format version number. HL1 SPRs are version 2 (0x02,0x00,0x00,0x00)
    int         spriteType;     // Orientation method
    int         textFormat;     // Translucency/Transparency method
    float       boundingRadius;
    int         maxWidth;
    int         maxHeight;
    int         frameNum;          // number of frames the sprite contains
    float       beamLength;
    int         synchType;

    short       paletteColorCount; // number of colors in the palette; should be 256
    char        color_pallete[PALLETE_LENGTH * 3]; /* Not in the wiki but required */
} sprite_header_t;

/*
* Inspecting the sprite "1280\hud_number_0.spr" I gathered:
* id:               "IDSP"
* version:          2
* spriteType:       2
* textFormat:       1
* boundingRadius:   45.2548332214355
* maxWidth:         64
* maxHeight:        64
* frameNum:         1
* beamLength:       0
* synchType:        0
* paletteColorCount:256
*/

typedef struct sprite_frame_header_s
{
    int         group;
    int         originX;        // not sure about this one, it always huge for sprites in HL1
    int         originY;
    int         width;
    int         height;

    // Right after this, the paletted image data comes, each byte is a pixel.
    // The image size is given in the frame header, so the entire data for 1 frame is width*height bytes.
} sprite_frame_header_t;
#pragma pack(pop)

typedef enum sprite_generation_strategy_e {
    SPRITE_SINGLE = 0, /* FIXME */
    SPRITE_PER_DIGIT = 1
} sprite_generation_strategy_t;

int main(int argc, char *argv[])
{
    if (argc != 4) {
        usage:
        nob_log(NOB_ERROR, "Usage: %s <TrueType Font> <sprite_height> <sprite_kerning_px>", argv[0]);
        return 1;
    }
    float font_height;
    int sprite_kerning;
    const char* font_path = argv[1];
    if (sscanf(argv[2], "%f", &font_height) != 1) {
        nob_log(NOB_ERROR, "Invalid sprite height provided");
        goto usage;
    }
    if (sscanf(argv[3], "%d", &sprite_kerning) != 1) {
        nob_log(NOB_ERROR, "Invalid kerning size provided");
        goto usage;
    }

    Nob_String_Builder sb = { 0 };
    if (!nob_read_entire_file(font_path, &sb)) return 1;

    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, (unsigned char*)sb.items, 0)) {
        nob_log(NOB_ERROR, "Failed setting up the font");
        return -1;
    }

    /* TODO: make it so that the user selects the strategy after fixing SPRITE_SINGLE */
    sprite_generation_strategy_t generation_strategy = SPRITE_PER_DIGIT;

    int max_width = 0;
    int max_height = 0;
    float scale = stbtt_ScaleForPixelHeight(&font, font_height);

    FILE* hud_txt = fopen("append_me_to_hud.txt", "wb");
    if (generation_strategy == SPRITE_PER_DIGIT) {
        for (int i = 0; i < BASE_TEN_DIGIT_COUNT; ++i) {
            int x0, y0, x1, y1;
            int glyph_index = stbtt_FindGlyphIndex(&font, '0' + i);
            stbtt_GetGlyphBitmapBox(&font, glyph_index, scale, scale, &x0, &y0, &x1, &y1);
            int current_width = x1 - x0 + stbtt_BakeFontBitmap_PADDING + sprite_kerning;
            int current_height = y1 - y0 + stbtt_BakeFontBitmap_PADDING;

            fprintf(hud_txt, "number_%d\t%d\t%s%d\t%d\t%d\t%d\t%d\n", i, Y_RES, SPRITE_PREFIX, i, stbtt_BakeFontBitmap_PADDING, 0,
                current_width, current_height);

            max_height = fmax(max_height, current_height);
            max_width = fmax(max_width, current_width);
        }
    }
    else if (generation_strategy == SPRITE_SINGLE) {
        for (int i = 0; i < BASE_TEN_DIGIT_COUNT; ++i) {
            int x0, y0, x1, y1;
            int glyph_index = stbtt_FindGlyphIndex(&font, '0' + i);
            stbtt_GetGlyphBitmapBox(&font, glyph_index, scale, scale, &x0, &y0, &x1, &y1);
            int current_width = x1 - x0 + stbtt_BakeFontBitmap_PADDING + sprite_kerning;
            int current_height = y1 - y0 + stbtt_BakeFontBitmap_PADDING;

            fprintf(hud_txt, "number_%d\t%d\t%s\t%d\t%d\t%d\t%d\n", i, Y_RES, OUTPUT_NAME, max_width + stbtt_BakeFontBitmap_PADDING, 0,
                current_width, current_height);

            max_height = fmax(max_height, current_height);
            max_width += current_width;
        }
    }
    fclose(hud_txt);

    /* All textures must be in dimensions that are multiples of 16 and the total size must be less than 10752 */
    /* Round up to nearest multiple of 16 */
    max_width = (max_width + 15) & ~15u;
    max_height = (max_height + 15) & ~15u;

    if (max_width > 512 || max_height > 512) {
        nob_log(NOB_ERROR, "GoldSRC doesn't support sprites bigger than 512x512 which is lower than the genearated texture, try a lower font height or per digit sprite mode.");
    }


    /* Assert total_size is within the allowed limit */
    if (max_width * max_height > 10752) {
        nob_log(NOB_ERROR, "The size exceeds the GoldSrc limitaitions, try a lower font height or per digit sprite mode.");
        return 3;
    }

    float corner_distance = sqrtf(max_width * 2 * 2); /* sqrt(total_width^2 + total_width^2) */

    sprite_header_t sprite_header = (sprite_header_t){
        .id = {'I', 'D', 'S', 'P'},
        .version = SPRITE_VERSION,
        .spriteType = SPR_ADDITIVE,
        .textFormat = VP_PARALLEL,
        .boundingRadius = corner_distance,
        .maxWidth = max_width,
        .maxHeight = max_height,
        .frameNum = 1,
        .beamLength = 0,
        .synchType = 1,
        .paletteColorCount = PALLETE_LENGTH
    };

    for (int i = 0; i < PALLETE_LENGTH * 3 - 2; i += 3) {
        sprite_header.color_pallete[i + 0] = i / 3;
        sprite_header.color_pallete[i + 1] = i / 3;
        sprite_header.color_pallete[i + 2] = i / 3;
    }

    /* There's almost no docs about this */
    sprite_frame_header_t frame_header = (sprite_frame_header_t){
        .group = 0,
        .originX = -(max_width / 2), /* There's a pattern with the origin for the official sprites */
        .originY = max_height / 2,
        .width = max_width,
        .height = max_height
    };

    int bitmap_size = max_width * max_height;
    unsigned char* digits_sprite_bitmap = malloc(bitmap_size);
    stbtt_bakedchar cdata[10];


    if (generation_strategy == SPRITE_PER_DIGIT) {
        for (int i = 0; i < 10; ++i) {
            /* stbtt_BakeFontBitmap's implementation does the memset for us */
            int res = stbtt_BakeFontBitmap((unsigned char*)sb.items, 0, font_height, digits_sprite_bitmap, max_width, max_height, '0' + i, 1, cdata);
            if (0 > res) {
                nob_log(NOB_ERROR, "Failed creating the font bitmap. stbtt_BakeFontBitmap returned: %d", res);
                return 4;
            }

            char sprite_file_name[] = SPRITE_PREFIX"x.spr";
            sprite_file_name[14] = i + '0';

            FILE* spr_file = fopen(sprite_file_name, "wb");
            fwrite(&sprite_header, sizeof(sprite_header), 1, spr_file);
            fwrite(&frame_header, sizeof(frame_header), 1, spr_file);
            fwrite(digits_sprite_bitmap, bitmap_size, 1, spr_file);
            fclose(spr_file);

            char png_file_name[] = SPRITE_PREFIX"x.png";
            png_file_name[14] = i + '0';
            stbi_write_png(png_file_name, max_width, max_height, 1, digits_sprite_bitmap, max_width * sizeof(digits_sprite_bitmap[0]));
        }
    }
    else if (generation_strategy == SPRITE_SINGLE) {
        /* stbtt_BakeFontBitmap's implementation does the memset for us */
        int res = stbtt_BakeFontBitmap((unsigned char*)sb.items, 0, font_height, digits_sprite_bitmap, max_width, max_height, '0', 10, cdata);
        if (0 >= res) {
            nob_log(NOB_ERROR, "Failed creating the font bitmap. stbtt_BakeFontBitmap returned: %d", res);
            return 4;
        }

        FILE* spr_file = fopen(OUTPUT_FILE, "wb");
        fwrite(&sprite_header, sizeof(sprite_header), 1, spr_file);
        fwrite(&frame_header, sizeof(frame_header), 1, spr_file);
        fwrite(digits_sprite_bitmap, bitmap_size, 1, spr_file);
        fclose(spr_file);
        stbi_write_png("fontatlas.png", max_width, max_height, 1, digits_sprite_bitmap, max_width * sizeof(digits_sprite_bitmap[0]));
    }

    /* Don't really need this but w/e */
    free(digits_sprite_bitmap);
    NOB_FREE(sb.items);

	return 0;
}
