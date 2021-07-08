# cgc2wav

By Attila Grosz (29th July 2009)
<http://gaia.atilia.eu>

Colour Genie (CG) emulators often have their programs available as virtual cassette or CAS files. CAS files are small binary files with a `.cas` extension. These load quickly and are mostly a byte-by-byte representation of the program code, together with some formatting and checksum information.

Real Colour Genies however, take input in the form of cassette audio files. In the PC world these can be represented by 8-bit mono audio files in WAV format. These WAV files can be saved and loaded from real Colour Genies through a PC sound port using a WAV recorder/player to emulate a cassette player. WAV files are much larger than their CAS equivalents.

People with real Colour Genies may wish to archive their tapes in CAS format, or use them on one of the Colour Genie emulators. Alternatively, they may wish to source CG programs in CAS format from the Internet, and use those files on their real machines.

cgc2wav converts Colour Genie emulator CAS files to 8-bit PCM WAV files.

The program must be run from a shell or the Windows Command Prompt.

Usage is:

    Cgc2wav -i <input CAS filename> -o <output WAV filename>

Command line options include:

    -f <rate> : sample rate, must be 48000, 44100, 22050, 11025 or 8000 (default: 44100)
    -g <gain> : gain, must be between 1 and 7 (default 6)
    -h : prints this text
