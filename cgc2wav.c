#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "getopt.h"

/* WAV file header structure */
/* should be 1-byte aligned */
#pragma pack(1)
struct wav_header
{
    char riff[4];
    unsigned int rLen;
    char WAVEfmt[8];
    unsigned int fLen;         /* 0x1020 */
    unsigned short wFormatTag; /* 0x0001 */
    unsigned short nChannels;  /* 0x0001 */
    unsigned int nSamplesPerSec;
    unsigned int nAvgBytesPerSec;  // nSamplesPerSec*nChannels*(nBitsPerSample%8)
    unsigned short nBlockAlign;    /* 0x0001 */
    unsigned short nBitsPerSample; /* 0x0008 */
    char datastr[4];
    unsigned int data_size;
} wave = {
    'R', 'I', 'F', 'F',
    0,
    'W', 'A', 'V', 'E', 'f', 'm', 't', ' ',
    16,
    1,
    1,
    44100,
    44100,
    1,
    8,
    'd', 'a', 't', 'a',
    0};
#pragma pack()

static unsigned char p_gain = 6 * 0x0f;
const unsigned char p_silence = 0;
static unsigned int wav_sample_count = 0;
static unsigned char level;

static double bauds_to_samples(unsigned int bauds)
{
    return ((double)wave.nSamplesPerSec / bauds);
}

static unsigned int cycles_to_samples(unsigned int cycles)
{
    return (cycles * wave.nSamplesPerSec) / 2216750;
}

static void filter_output(unsigned char level, FILE *wavfile)
{
    static double hp_accu = 0, lp_accu = 0;
    double in = level * 1.0;
    double clipped = lp_accu - hp_accu;
    unsigned char out;

    if (clipped >= 127.0)
        clipped = 127.0;
    else if (clipped < -127.0)
        clipped = -127;

    out = ((unsigned char)(clipped)) ^ 0x80;
    lp_accu += ((double)in - lp_accu) * 0.5577;
    hp_accu += (lp_accu - hp_accu) * 0.0070984;
    fputc(out, wavfile);
}

static void dump_bit(FILE *fp, unsigned int bit)
{
    unsigned int j;
    unsigned int period = (unsigned int)(bauds_to_samples(1150) / (bit + 1));

    do
    {
        for (j = 0; j < period; j++)
        {
            filter_output(level ? p_silence : p_gain, fp);
        }
        level ^= 1;
    } while (bit--);
}

static void close_wav(FILE *outfile)
{
    int foo = ftell(outfile) - 8;
    fseek(outfile, 4, SEEK_SET);
    fwrite(&foo, sizeof(foo), 1, outfile);

    foo = foo - sizeof(wave) + 8;
    fseek(outfile, sizeof(wave) - sizeof(unsigned int), SEEK_SET);
    fwrite(&foo, sizeof(foo), 1, outfile);

    fclose(outfile);
}

static void output_wav_byte(FILE *wavfile, unsigned char byte)
{
    unsigned int bc = 7;

    do
    {
        dump_bit(wavfile, (byte >> bc) & 1);
    } while (bc--);
}

static void write_silence(FILE *wavfile)
{
    unsigned int j;
    for (j = 0; j < cycles_to_samples(15000); j++)
        filter_output(p_silence, wavfile);
}

static void init_wav(FILE *wavfile)
{
    fwrite(&wave, sizeof(wave), 1, wavfile);
    /* Lead in silence */
    write_silence(wavfile);
    level = 0;
}

static void write_cas_header(FILE *wavfile)
{
    unsigned int j;
    for (j = 0; j < 255; j++)
        output_wav_byte(wavfile, 0xAA);
}

size_t cg_get_header_size(FILE *cas)
{
    unsigned char byte;
    unsigned int size;
    char header_string[64];

    fseek(cas, 0, SEEK_SET);
    fread(header_string, 32, 1, cas);
    fseek(cas, 0, SEEK_SET);
    if (strncmp(header_string, "Colour Genie - Virtual Tape File", 32) != 0)
    {
        fprintf(stderr, "CAS file without header.\n");
        return 0;
    }

    do
    {
        byte = fgetc(cas);
    } while (byte != 0x00 && !feof(cas));

    byte = fgetc(cas);

    if (byte != 0x66)
    {
        return 0;
    }
    ungetc(byte, cas);
    size = ftell(cas);
    fprintf(stderr, "CAS header size %i bytes.", size);

    return size;
}

static void process_cas(FILE *input, FILE *output)
{
    unsigned char in;
    size_t cas_hdr_size;

    cas_hdr_size = cg_get_header_size(input);
    write_cas_header(output);
    fprintf(stderr, "Writing leader (AAh) bytes.\n");

    while (!feof(input))
    {
        fread(&in, 1, 1, input);
        output_wav_byte(output, in);
    }
    write_silence(output);
    cas_hdr_size = ftell(output) / 1024;
    fprintf(stderr, "%i kbytes written.\n", cas_hdr_size);
    fclose(input);
}

static void print_usage()
{
    printf("CGC2WAV v1.0a\n");
    printf("Colour Genie CAS to PCM wave file converter.\n");
    printf("Copyright 2009 by Attila Grosz\n");
    printf("Usage:\n");
    printf("cgc2wav -i <input_filename> -o <output_filename>\n");
    printf("Command line option:\n");
    printf("-f <rate> : sample rate, must be 48000, 44100, 22050, 11025 or 8000 (default: 44100)\n");
    printf("-g <gain> : gain, must be between 1 and 7 (default: 6)\n");
    printf("-h        : prints this text\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    int finished = 0;
    int arg1;
    FILE *htp = 0, *wav = 0;

    while (!finished)
    {
        switch (getopt(argc, argv, "?hf:i:o:g:"))
        {
        case -1:
        case ':':
            finished = 1;
            break;
        case '?':
        case 'h':
            print_usage();
            break;
        case 'f':
            if (!sscanf(optarg, "%i", &arg1))
            {
                fprintf(stderr, "Error parsing argument for '-f'.\n");
                exit(2);
            }
            else
            {
                if (arg1 != 48000 && arg1 != 44100 && arg1 != 22050 &&
                    arg1 != 11025 && arg1 != 8000)
                {
                    fprintf(stderr, "Unsupported sample rate: %i.\n", arg1);
                    fprintf(stderr, "Supported sample rates are: 48000, 44100, 22050, 11025 and 8000.\n");
                    exit(3);
                }
                wave.nSamplesPerSec = arg1;
                wave.nAvgBytesPerSec = wave.nSamplesPerSec * wave.nChannels * (wave.nBitsPerSample % 8);
            }
            break;
        case 'g':
            if (!sscanf(optarg, "%i", &arg1))
            {
                fprintf(stderr, "Error parsing argument for '-g'.\n");
                exit(2);
            }
            else
            {
                if (arg1 < 0 || arg1 > 7)
                {
                    fprintf(stderr, "Illegal gain value: %i.\n", arg1);
                    fprintf(stderr, "Gain must be between 1 and 7.\n");
                }
                p_gain = arg1 * 0x0f;
            }
            break;
        case 'i':
            if (!(htp = fopen(optarg, "rb")))
            {
                fprintf(stderr, "Error opening %s.\n", optarg);
                exit(4);
            }
            break;
        case 'o':
            if (!(wav = fopen(optarg, "wb")))
            {
                fprintf(stderr, "Error opening %s.\n", optarg);
                exit(4);
            }
            break;
        default:
            break;
        }
    }

    if (!htp)
    {
        print_usage();
    }
    else if (!wav)
    {
        print_usage();
    }
    else
    {
        init_wav(wav);
        process_cas(htp, wav);
        close_wav(wav);
    }

    return 0;
}
