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

const char* sample_types[] = {
    "unsigned", // 0
    "signed",
    "uint8",
    "int8",
    "uint16",
    "int16",
    "uint32",
    "int32",
    "uint64",
    "int64",   // 9
    ""         // 10
    ""         // 11
    ""         // 12
    ""         // 13
    ""         // 14
    ""         // 15
    "float16", // 16
    "float32",
    "float64",
};

const char* planarity[] = {
    "planar",
    "coplanar",
    "semi-planar"
};

int main(int argc, char*argv[]) {
    int n;
    struct DxrHeader hdr;
    unsigned int bits;
    uint8_t* png_data;
    uint8_t* px;
    uint8_t  planes;
    FILE* fd;
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
        printf("usage: dxr2png [--no-binning] filename {R,Gr,Gb,B}\n");
        return 0;
    }
    else if (strncmp(argv[1], "--", 2) == 0) {
        fprintf(stderr, "ERROR: unknown option %s\n", argv[1]);
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
        fclose(fd);
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
    assert(hdr.precision == 12);
    assert(hdr.comp == 1);       // compressed
    assert(hdr.planarity == 1);  // coplanar
    assert(strcmp(hdr.type, "Bayer0") == 0);

    // sizeof(header) + (planes * (width * height * bpp) / 8)
    //             64 + (     4 * ( 2784 *   2463 * 12 ) / 8)
    // => 41142016   which is the size DXR files from Sultans
    bits = hdr.width * hdr.height * hdr.precision;
    assert(bits % 8 == 0);

    png_data = (uint8_t*)malloc(hdr.width * hdr.height * sizeof(uint8_t) * ((binning) ? 1 : 4));
    assert(png_data);

    px = png_data;

    for (unsigned int off = 0; off < hdr.width * hdr.height * 2; off++) {
        uint8_t buf[3];
        uint16_t b1, b2;

        n = fread(buf, 3, 1, fd);
        assert(n == 1);

        // order is determined by type (here Bayer0 is Gb, B, R, Gr)
        b1 = (uint16_t)buf[0] | ((uint16_t)buf[1] & 0x000fU) << 8;
        b2  = ((uint16_t)buf[1] >> 4) | ((uint16_t)buf[2] << 4);

        // convert to 8 -bit
        b1 = b1 >> 4;
        b2 = b2 >> 4;
#if 0
        printf("%5d: %d: %02x %02x %02x : %03x %03x\n",
               off,
               (off % (2 * hdr.width)) / hdr.width,
               buf[0] , buf[1], buf[2], b1, b2);
#endif
        if (off % (2 * hdr.width) < hdr.width) {
            // Gb and B
            if (planes & Bayer_Gb || !binning) { *px++ = (planes & Bayer_Gb) ? b1 : 0; }
            if (planes & Bayer_B  || !binning) { *px++ = (planes & Bayer_B)  ? b2 : 0; }
        }
        else {
            // R and Gr
            if (planes & Bayer_R  || !binning) { *px++ = (planes & Bayer_R) ? b1 : 0; }
            if (planes & Bayer_Gr || !binning) { *px++ = (planes & Bayer_R) ? b2 : 0; }
        }
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

    int png_width  = hdr.width * ((binning) ? 1 : 2);
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
