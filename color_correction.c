
/**
*implmentation of Local Color Correction using Non-Linear Masking published by Nathan Moroney Hewlett-Packard Laboratories, Palo Alto, California.
 **/
#include "browse.h"

#define USE_SHELL_OPEN

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION

#include "stb_image.h"
/* ref:https://github.com/nothings/stb/blob/master/stb_image.h */
#define TJE_IMPLEMENTATION

#include "tiny_jpeg.h"
/* ref:https://github.com/serge-rgb/TinyJPEG/blob/master/tiny_jpeg.h */
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include "timing.h"
#include <stdint.h>
#include <assert.h>

#ifndef _MAX_DRIVE
#define _MAX_DRIVE 3
#endif
#ifndef _MAX_FNAME
#define _MAX_FNAME 256
#endif
#ifndef _MAX_EXT
#define _MAX_EXT 256
#endif
#ifndef _MAX_DIR
#define _MAX_DIR 256
#endif
#ifndef MIN
#define MIN(a, b)    ( (a) > (b) ? (b) : (a) )
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif
char saveFile[1024];

unsigned char *loadImage(const char *filename, int *Width, int *Height, int *Channels) {
    return (stbi_load(filename, Width, Height, Channels, 0));
}


void saveImage(const char *filename, int Width, int Height, int Channels, unsigned char *Output) {
    memcpy(saveFile + strlen(saveFile), filename, strlen(filename));
    *(saveFile + strlen(saveFile) + 1) = 0;

    if (!tje_encode_to_file(saveFile, Width, Height, Channels, true, Output)) {
        fprintf(stderr, "save JPEG fail.\n");
        return;
    }
#ifdef USE_SHELL_OPEN
    browse(saveFile);
#endif
}


void splitpath(const char *path, char *drv, char *dir, char *name, char *ext) {
    const char *end;
    const char *p;
    const char *s;
    if (path[0] && path[1] == ':') {
        if (drv) {
            *drv++ = *path++;
            *drv++ = *path++;
            *drv = '\0';
        }
    } else if (drv)
        *drv = '\0';
    for (end = path; *end && *end != ':';)
        end++;
    for (p = end; p > path && *--p != '\\' && *p != '/';)
        if (*p == '.') {
            end = p;
            break;
        }
    if (ext)
        for (s = end; (*ext = *s++);)
            ext++;
    for (p = end; p > path;)
        if (*--p == '\\' || *p == '/') {
            p++;
            break;
        }
    if (name) {
        for (s = p; s < end;)
            *name++ = *s++;
        *name = '\0';
    }
    if (dir) {
        for (s = path; s < p;)
            *dir++ = *s++;
        *dir = '\0';
    }
}

void getCurrentFilePath(const char *filePath, char *saveFile) {
    char drive[_MAX_DRIVE];
    char dir[_MAX_DIR];
    char fname[_MAX_FNAME];
    char ext[_MAX_EXT];
    splitpath(filePath, drive, dir, fname, ext);
    size_t n = strlen(filePath);
    memcpy(saveFile, filePath, n);
    char *cur_saveFile = saveFile + (n - strlen(ext));
    cur_saveFile[0] = '_';
    cur_saveFile[1] = 0;
}

int GetMirrorPos(int Length, int Pos) {
    if (Pos < 0)
        return -Pos;
    else if (Pos >= Length)
        return Length + Length - Pos - 2;
    else
        return Pos;
}

unsigned char ClampToByte(int Value) {
    if (Value < 0)
        return 0;
    else if (Value > 255)
        return 255;
    else
        return (unsigned char) Value;
}

void FillLeftAndRight_Mirror(int *Array, int Length, int Radius) {
    for (int X = 0; X < Radius; X++) {
        Array[X] = Array[Radius + Radius - X];
        Array[Radius + Length + X] = Array[Radius + Length - X - 2];
    }
}

int SumOfArray(const int *Array, int Length) {
    int Sum = 0;
    for (int X = 0; X < Length; X++) {
        Sum += Array[X];
    }
    return Sum;
}

void BoxBlurGrayscale(unsigned char *input, unsigned char *output, int Width, int Height, int Radius) {
    if ((input == NULL) || (output == NULL)) return;
    if ((Width <= 0) || (Height <= 0) || (Radius <= 0)) return;
    if (Radius < 1) return;
    Radius = MIN(MIN(Radius, Width - 1), Height - 1);
    int SampleAmount = (2 * Radius + 1) * (2 * Radius + 1);
    float Inv = 1.0f / SampleAmount;

    int *ColValue = (int *) malloc((Width + Radius + Radius) * sizeof(int));
    int *ColOffset = (int *) malloc((Height + Radius + Radius) * sizeof(int));
    if ((ColValue == NULL) || (ColOffset == NULL)) {
        if (ColValue != NULL) free(ColValue);
        if (ColOffset != NULL) free(ColOffset);
        return;
    }
    for (int Y = 0; Y < Height + Radius + Radius; Y++)
        ColOffset[Y] = GetMirrorPos(Height, Y - Radius);
    {
        for (int Y = 0; Y < Height; Y++) {
            unsigned char *scanLineOut = output + Y * Width;
            if (Y == 0) {
                memset(ColValue + Radius, 0, Width * sizeof(int));
                for (int Z = -Radius; Z <= Radius; Z++) {
                    unsigned char *scanLineIn = input + ColOffset[Z + Radius] * Width;
                    for (int X = 0; X < Width; X++) {
                        ColValue[X + Radius] += scanLineIn[X];
                    }
                }
            } else {
                unsigned char *RowMoveOut = input + ColOffset[Y - 1] * Width;
                unsigned char *RowMoveIn = input + ColOffset[Y + Radius + Radius] * Width;
                for (int X = 0; X < Width; X++) {
                    ColValue[X + Radius] -=
                            RowMoveOut[X] - RowMoveIn[X];
                }
            }
            FillLeftAndRight_Mirror(ColValue, Width, Radius);
            int LastSum = SumOfArray(ColValue, Radius * 2 + 1);
            scanLineOut[0] = ClampToByte((int) (LastSum * Inv));
            for (int X = 0 + 1; X < Width; X++) {
                int NewSum = LastSum - ColValue[X - 1] + ColValue[X + Radius + Radius];
                scanLineOut[X] = ClampToByte((int) (NewSum * Inv));
                LastSum = NewSum;
            }
        }
    }
    free(ColValue);
    free(ColOffset);
}

void Grayscale(unsigned char *Input, unsigned char *Output, int Width, int Height, int Channels) {
    if (Channels == 1) {
        for (unsigned int Y = 0; Y < Height; Y++) {
            unsigned char *pOutput = Output + (Y * Width);
            unsigned char *pInput = Input + (Y * Width);
            for (unsigned int X = 0; X < Width; X++) {
                pOutput[X] = ClampToByte(pInput[X]);
            }
        }
    } else {
        for (unsigned int Y = 0; Y < Height; Y++) {
            unsigned char *pOutput = Output + (Y * Width);
            unsigned char *pInput = Input + (Y * Width * Channels);
            for (unsigned int X = 0; X < Width; X++) {
                pOutput[X] = ClampToByte(((21842 * pInput[0] + 21842 * pInput[1] + 21842 * pInput[2]) >> 16));
                pInput += Channels;
            }
        }
    }
}

void LocalColorCorrection(unsigned char *Input, unsigned char *Output, int Width, int Height, int Channels) {
    unsigned char *Mask = (unsigned char *) malloc(Width * Height * sizeof(unsigned char));
    if (Mask == NULL)
        return;
    unsigned char LocalLut[256 * 256];
    for (int mask = 0; mask < 256; mask++) {
        unsigned char *pLocalLut = LocalLut + (mask << 8);
        for (int pix = 0; pix < 256; pix++) {
            pLocalLut[pix] = ClampToByte(255.0f * powf(pix / 255.0f, powf(2.0f, (128.0f - (255.0f - mask)) / 128.0f)));
        }
    }
    Grayscale(Input, Output, Width, Height, Channels);
    int Radius = (MAX(Width, Height) / 512) + 1;
    BoxBlurGrayscale(Output, Mask, Width, Height, Radius);
    for (int Y = 0; Y < Height; Y++) {
        unsigned char *pOutput = Output + (Y * Width * Channels);
        unsigned char *pInput = Input + (Y * Width * Channels);
        unsigned char *pMask = Mask + (Y * Width);

        for (int X = 0; X < Width; X++) {
            unsigned char *pLocalLut = LocalLut + (pMask[X] << 8);
            for (int C = 0; C < Channels; C++) {
                pOutput[C] = pLocalLut[pInput[C]];
            }
            pOutput += Channels;
            pInput += Channels;
        }
    }
    free(Mask);
}

void LocalExponentialCorrection(unsigned char *Input, unsigned char *Output, int Width, int Height, int Channels) {
    unsigned char *Luminance = (unsigned char *) malloc(Width * Height * 2 * sizeof(unsigned char));
    unsigned char *Mask = Luminance + (Width * Height);
    if (Luminance == NULL)
        return;
    unsigned char LocalLut[256 * 256];
    for (int mask = 0; mask < 256; mask++) {
        unsigned char *pLocalLut = LocalLut + (mask << 8);
        for (int pix = 0; pix < 256; pix++) {
            pLocalLut[pix] = ClampToByte(255.0f * powf(pix / 255.0f, powf(2.0f, (128.0f - (255.0f - mask)) / 128.0f)));
        }
    }
    Grayscale(Input, Luminance, Width, Height, Channels);
    int Radius = (MAX(Width, Height) / 512) + 1;
    BoxBlurGrayscale(Luminance, Mask, Width, Height, Radius);
    for (int Y = 0; Y < Height; Y++) {
        unsigned char *pOutput = Output + (Y * Width * Channels);
        unsigned char *pInput = Input + (Y * Width * Channels);
        unsigned char *pMask = Mask + (Y * Width);
        unsigned char *pLuminance = Luminance + (Y * Width);
        for (int X = 0; X < Width; X++) {
            unsigned char *pLocalLut = LocalLut + (pMask[X] << 8);
            for (int C = 0; C < Channels; C++) {
                pOutput[C] = ClampToByte(
                        (pLocalLut[pLuminance[X]] * (pInput[C] + pLuminance[X]) / (pLuminance[X] + 1) + pInput[C] -
                         pLuminance[X]) >> 1);
            }
            pOutput += Channels;
            pInput += Channels;
        }
    }
    free(Luminance);
}

int main(int argc, char **argv) {
    printf("Local Color Correction demo\n ");
    printf("blog:http://cpuimage.cnblogs.com/ \n ");

    if (argc < 2) {
        printf("usage: %s   image \n ", argv[0]);
        printf("eg: %s   d:\\image.jpg \n ", argv[0]);

        return (0);
    }
    char *szfile = argv[1];

    getCurrentFilePath(szfile, saveFile);

    int Width = 0;
    int Height = 0;
    int Channels = 0;
    unsigned char *inputImage = NULL;

    double startTime = now();
    inputImage = loadImage(szfile, &Width, &Height, &Channels);

    double nLoadTime = calcElapsed(startTime, now());
    printf("load time: %d ms.\n ", (int) (nLoadTime * 1000));
    if ((Channels != 0) && (Width != 0) && (Height != 0)) {
        unsigned char *outputImg = (unsigned char *) stbi__malloc(Width * Channels * Height * sizeof(unsigned char));
        if (inputImage) {
            memcpy(outputImg, inputImage, (size_t) (Width * Channels * Height));
        } else {
            printf("load: %s fail!\n ", szfile);
        }
        startTime = now();
        //  LocalColorCorrection(inputImage, outputImg, Width, Height, Channels);
        LocalExponentialCorrection(inputImage, outputImg, Width, Height, Channels);
        double nProcessTime = calcElapsed(startTime, now());

        printf("process time: %d ms.\n ", (int) (nProcessTime * 1000));

        startTime = now();

        saveImage("done.jpg", Width, Height, Channels, outputImg);
        double nSaveTime = calcElapsed(startTime, now());

        printf("save time: %d ms.\n ", (int) (nSaveTime * 1000));

        if (outputImg) {
            stbi_image_free(outputImg);
        }

        if (inputImage) {
            stbi_image_free(inputImage);
        }
    } else {
        printf("load: %s fail!\n", szfile);
    }

    getchar();
    printf("press any key to exit. \n");

    return (EXIT_SUCCESS);
}