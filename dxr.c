#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
//#include <strings.h>
#include <assert.h>

#include <png.h>

int save_png(const char* filename,
             int width, int height, int bitdepth, int colortype,
             unsigned char* data, int pitch,
             int transform);


#define Bayer_R   0x1
#define Bayer_Gr  0x2
#define Bayer_Gb  0x4
#define Bayer_B   0x8
#define Bayer_ALL (Bayer_R | Bayer_Gr | Bayer_B | Bayer_Gb)

// From :
//   https://goproinc.atlassian.net/wiki/spaces/ISG/pages/18819332/DXR+file+format+specification
struct DxrHeader {
    char     magic[4];        // 'DXR '
    char     type[8];         // hint for interpreting the raw data
    uint32_t rsv1;            // reserved, 0
    uint32_t rsv2;            // reserved, 0
    uint32_t width;           // image width
    uint32_t height;          // image height
    uint8_t  precision;       // precision, i.e. number of bits per sample
    uint8_t  rsv3;            // reserved, 0
    uint16_t rsv4;            // reserved, 0
    uint8_t  sampleType;      // 0:unsigned, 1:signed, 2:UINT8, …
    uint8_t  comp;            // 0:uncompressed, 1:packed
    uint16_t rsv5;            // reserved, 0
    uint8_t  channels;        // number of channels
    uint8_t  planarity;       // 0:planar, 1:coplanar, 2:semi-planar
    uint16_t rsv6;            // reserved, 0
    uint32_t rsv7;            // reserved, 0
    uint32_t pedestal;        // value representing a black pixel
    uint32_t stride;          // 0:contiguous, >0:line stride
    uint32_t rsv8;            // reserved, 0
    uint64_t rsv9;            // reserved, 0
};

enum dxr_planarity {
    DXR_PLAN_PLANAR,
    DXR_PLAN_COPLANAR,
    DXR_PLAN_SEMIPLANAR
};

enum dxr_sample_types {
    DXR_SAMPLE_UNSIGNED = 0,
    DXR_SAMPLE_SIGNED   = 1,
    DXR_SAMPLE_UINT8    = 2,
    DXR_SAMPLE_INT8     = 3,
    DXR_SAMPLE_UINT16   = 4,
    DXR_SAMPLE_INT16    = 5,
    DXR_SAMPLE_UINT32   = 6,
    DXR_SAMPLE_INT32    = 7,
    DXR_SAMPLE_UINT64   = 8,
    DXR_SAMPLE_INT64    = 9,
    DXR_SAMPLE_FLOAT16  = 16,
    DXR_SAMPLE_FLOAT32  = 17,
    DXR_SAMPLE_FLOAT64  = 18,
};

static const char* sample_types[] = {
    [DXR_SAMPLE_UNSIGNED] = "unsigned",
    [DXR_SAMPLE_SIGNED]   = "signed",
    [DXR_SAMPLE_UINT8]    = "uint8",
    [DXR_SAMPLE_INT8]     = "int8",
    [DXR_SAMPLE_UINT16]   = "uint16",
    [DXR_SAMPLE_INT16]    = "int16",
    [DXR_SAMPLE_UINT32]   = "uint32",
    [DXR_SAMPLE_INT32]    = "int32",
    [DXR_SAMPLE_UINT64]   = "uint64",
    [DXR_SAMPLE_INT64]    = "int64",
    [DXR_SAMPLE_FLOAT16]  = "float16",
    [DXR_SAMPLE_FLOAT32]  = "float32",
    [DXR_SAMPLE_FLOAT64]  = "float64",
};

static const char* planarity[] = {
    [DXR_PLAN_PLANAR]     = "planar",
    [DXR_PLAN_COPLANAR]   = "coplanar",
    [DXR_PLAN_SEMIPLANAR] = "semi-planar"
};

/*
struct dxr_types {
    const char* type;
} dxr_types[] = {
    { "" },
    { "Bayer0", },
    { "Bayer1", },
    { "Bayer2", },
    { "Bayer3", },
    { "Gray", },
    { "RGB", },
    { "YUV", },
    { "YUV422", },
    { "YUV420" }
};
*/

static FILE* fd;
static uint8_t buf;

uint16_t read_pix(uint32_t bit_offset, int precision) {
    uint32_t val = 0;

    for (int i = 0; i < precision; i++) {

        if ((bit_offset + i) % 8 == 0) {
            fread(&buf, 1, 1, fd);
        }

        uint32_t b = !!(buf & (1 << (bit_offset + i) % 8));

        val |= b << i;
    }
    //printf("0x%02x %d\n", val, val);

    return val;
}


int main(int argc, char*argv[]) {
    int n;
    struct DxrHeader hdr;
    unsigned int bits;
    uint8_t* png_data;
    uint8_t* px;
    uint8_t  planes;
    char* dxrpath;
    char* pngpath;
    bool binning = true;  // default mode
    int opt = 0;

    if (argc < 2) {
        fprintf(stderr, "ERROR: no file specified\n");
        return 1;
    }

    if (strcmp(argv[1], "--no-binning") == 0) {
       binning = false;
       opt++;
    }
    else if (strcmp(argv[1], "--help") == 0) {
        printf("usage: dxr2png [--no-binning] filename [R,Gr,Gb,B]\n");
        return 0;
    }
    else if (strncmp(argv[1], "-", 1) == 0) {
        fprintf(stderr, "ERROR: unknown option %s (try --help)\n", argv[1]);
        return 1;
    }

    if (argc + opt < 3) {
        fprintf(stderr, "INFO: no plane [R,Gr,Gb,B] specified, extracting all planes\n");
        planes = Bayer_ALL;
        binning = false;
    }
    else if (strcasecmp(argv[2 + opt], "R") == 0)  planes = Bayer_R;
    else if (strcasecmp(argv[2 + opt], "Gr") == 0) planes = Bayer_Gr;
    else if (strcasecmp(argv[2 + opt], "Gb") == 0) planes = Bayer_Gb;
    else if (strcasecmp(argv[2 + opt], "B") == 0)  planes = Bayer_B;
    else {
        fprintf(stderr, "ERROR: unknown plane\n");
        return 1;
    }

    dxrpath = argv[1 + opt];
    fd = fopen(dxrpath, "r");
    if (fd == NULL) {
        fprintf(stderr, "ERROR: failed to open file\n");
        return 1;
    }

    n = fread(&hdr, sizeof(hdr), 1, fd);
    assert(n == 1);

    if (strncmp(hdr.magic, "DXR ", 4)) {
        fprintf(stderr, "not a DXR file\n");
        fclose(fd);
        return 1;
    }

    printf("type:       %s\n"
           "geometry:   %d x %d\n"
           "precision:  %d\n"
           "sample:     %s\n"
           "compressed: %s\n"
           "channels:   %d\n"
           "planarity:  %s\n"
           "stride:     %u\n"
           "black:      0x%x\n"
           ,
           hdr.type,
           hdr.width,
           hdr.height,
           hdr.precision,
           sample_types[hdr.sampleType],
           hdr.comp ? "yes" : "no",
           hdr.channels,
           planarity[hdr.planarity],
           hdr.stride,
           hdr.pedestal
        );

    // Lot of things are hardcoded/expected
    assert(strcmp(hdr.type, "Bayer0") == 0);
    assert(hdr.precision == 12 || hdr.precision == 10);
    assert((hdr.comp != 0 && hdr.sampleType == DXR_SAMPLE_UNSIGNED) ||
           (hdr.comp == 0 && hdr.sampleType == DXR_SAMPLE_UINT16));
    assert(hdr.planarity == DXR_PLAN_COPLANAR);

    // sizeof(header) + (planes * (width * height * bpp) / 8)
    //             64 + (     4 * ( 2784 *   2463 * 12 ) / 8)
    // => 41142016   which is the size DXR files from Sultans
    bits = hdr.width * hdr.height * hdr.precision;
    assert(bits % 8 == 0);

    png_data = (uint8_t*)malloc(hdr.width * hdr.height * sizeof(uint8_t) * ((binning) ? 1 : 4));
    assert(png_data);

    px = png_data;

    uint16_t precision = (hdr.comp) ? hdr.precision : 16;

    bits = 0;
    for (unsigned int lines = 0; lines < hdr.height * 2; lines++) {

        for (unsigned off = 0; off < hdr.width * 2; off += 2) {
            uint16_t b1, b2;

            b1 = read_pix(off * precision, precision);
            b2 = read_pix((off + 1) * precision, precision);

            // convert to 8-bit
            b1 = b1 >> (hdr.precision - 8);
            b2 = b2 >> (hdr.precision - 8);

#if 0
            printf("%5d: %d: %02x %02x %02x : %03x %03x\n",
                   off,
                   (off % (4 * hdr.width)) / (2 * hdr.width),
                   0, 0, 0, b1, b2);
#endif
            if ((bits/8 + off) % (2 * hdr.stride) < hdr.stride) {
                // Gb and B
                if (planes & Bayer_Gb || !binning) { *px++ = (planes & Bayer_Gb) ? b1 : 0; }
                if (planes & Bayer_B  || !binning) { *px++ = (planes & Bayer_B)  ? b2 : 0; }
            }
            else {
                // R and Gr
                if (planes & Bayer_R  || !binning) { *px++ = (planes & Bayer_R)  ? b1 : 0; }
                if (planes & Bayer_Gr || !binning) { *px++ = (planes & Bayer_Gr) ? b2 : 0; }
            }
        }

        bits += hdr.stride * 8;

        // jump to start of next line, and over remaining bits
        fseek(fd, sizeof(struct DxrHeader) + (bits / 8), SEEK_SET);
    }

    fclose(fd);

    //
    // Save to PNG
    //
    size_t len = strlen(dxrpath);
    assert(len >= 4);

    pngpath = malloc(len + 1 + 5);
    assert(pngpath);

    strcpy(pngpath, dxrpath);
    if (strcasecmp(pngpath + len - 4, ".DXR")) {
        fprintf(stderr, "ERROR: RAW file is not a .DXR file\n");
        return 1;
    }
    if ((planes & Bayer_ALL) == Bayer_ALL)
        sprintf(pngpath + len - 4, ".png");
    else
        sprintf(pngpath + len - 4, "-%s.png", argv[2 + opt]);

    printf("Saving to PNG...\n");

    int png_width  = hdr.width  * ((binning) ? 1 : 2);
    int png_height = hdr.height * ((binning) ? 1 : 2);

    save_png(pngpath,
             png_width,
             png_height,
             8,
             PNG_COLOR_TYPE_GRAY,
             png_data,
             png_width, // pitch/stride
             PNG_TRANSFORM_IDENTITY);

    return 0;
}
