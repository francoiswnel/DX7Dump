/* Yamaha DX7 Sysex Dump
 * Copyright 2012, Ted Felix
 * License: GPLv3+
 *
 * Updated in 2019 by Francois W. Nel.
 *
 * Takes a Yamaha DX7 sysex file and formats it as human readable text.
 * The format is also conducive to using diff (or meld) to examine differences
 * between patches.
 *
 * Based on info from:
 * http://homepages.abdn.ac.uk/mth192/pages/dx7/sysex-format.txt
 * FWN 2019: Note: The original file is no longer available, but has been retrieved from
 * web.archive.org and included in this updated repository.
 *
 * Build:
 *   g++ -o dx7dump dx7dump.cpp
 */

#include <getopt.h>
#include <math.h>
#include <unistd.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

// ***************************************************************************

struct OperatorPacked {
    unsigned char egRate1;
    unsigned char egRate2;
    unsigned char egRate3;
    unsigned char egRate4;
    unsigned char egLevel1;
    unsigned char egLevel2;
    unsigned char egLevel3;
    unsigned char egLevel4;
    unsigned char levelScaleBreakPoint;
    unsigned char levelScaleLeftDepth;
    unsigned char levelScaleRightDepth;
    unsigned char levelScaleLeftCurve : 2;
    unsigned char levelScaleRightCurve : 2;
    unsigned char : 4;
    unsigned char oscillatorRateScale : 3;
    unsigned char detune : 4;
    unsigned char : 1;
    unsigned char amplitudeModulationSensitivity : 2;
    unsigned char keyVelocitySensitivity : 3;
    unsigned char : 3;
    unsigned char outputLevel;
    unsigned char oscillatorMode : 1;
    unsigned char frequencyCoarse : 5;
    unsigned char : 2;
    unsigned char frequencyFine;
};

struct VoicePacked {
    OperatorPacked operators[6];
    unsigned char pitchEgRate1;
    unsigned char pitchEgRate2;
    unsigned char pitchEgRate3;
    unsigned char pitchEgRate4;
    unsigned char pitchEgLevel1;
    unsigned char pitchEgLevel2;
    unsigned char pitchEgLevel3;
    unsigned char pitchEgLevel4;
    unsigned char algorithm : 5;
    unsigned char : 3;
    unsigned char feedback : 3;
    unsigned char oscillatorKeySync : 1;
    unsigned char : 4;
    unsigned char lfoRate;
    unsigned char lfoDelay;
    unsigned char lfoPitchModulationDepth;
    unsigned char lfoAmplitudeModulationDepth;
    unsigned char lfoKeySync : 1;
    unsigned char lfoWave : 3;
    unsigned char lfoPitchModulationSensitivity : 4;
    unsigned char transpose;
    unsigned char name[10];
};

struct DX7Sysex {
    unsigned char sysexBeginF0;
    unsigned char yamaha43;
    unsigned char subStatusAndChannel;
    unsigned char format9;
    unsigned char sizeMsb;
    unsigned char sizeLsb;
    VoicePacked voices[32];
    unsigned char checksum;
    unsigned char sysexEndF7;
};

// ***************************************************************************

const char *onOff(unsigned x) {
    if (x > 1) return "*out of range*";

    const char *onOff[] = {"Off", "On"};

    return onOff[x];
}

const char *curve(unsigned x) {
    if (x > 3) return "*out of range*";

    const char *curves[] = {"-LIN", "-EXP", "+EXP", "+LIN"};

    return curves[x];
}

const char *lfoWave(unsigned x) {
    if (x > 5) return "*out of range*";

    const char *waves[] = {"Triangle", "Sawtooth Down", "Sawtooth Up", "Square", "Sine", "Sample and Hold"};

    return waves[x];
}

const char *mode(unsigned x) {
    if (x > 1) return "*out of range*";

    const char *modes[] = {"Ratio", "Fixed"};

    return modes[x];
}

const char *note(unsigned x) {
    const char *notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    x = x % 12;

    return notes[x];
}

std::string transpose(unsigned x) {
    if (x > 48) return "*out of range*";

    // Anything's better than sstream!
    char buffer[10];
    snprintf(buffer, 9, "%s%u", note(x), x / 12 + 1);

    return buffer;
}

std::string breakPoint(unsigned x) {
    if (x > 99) return "*out of range*";

    char buffer[10];

    // Playing some tricks (add 12 tones, subtract one octave) to avoid
    // the fact that -3/12 rounds to 0 and messes everything up.  Shifting
    // everything up an octave and then subtracting one solves the problem.
    const int octave = ((int) x - 3 + 12) / 12 - 1;

    // Anything's better than sstream!
    snprintf(buffer, 9, "%s%d", note(x + 9), octave);

    return buffer;
}

// ***************************************************************************

int processOptions(
        int *argc,
        char ***argv,
        const char *version,
        bool &displayLongListing,
        bool &shouldFindDuplicates,
        int &patchToDisplay) {
    struct option opts[] = {
            {"long",            0, nullptr, 'l'},
            {"find-duplicates", 0, nullptr, 'f'},
            {"patch",           1, nullptr, 'p'},
            {"version",         0, nullptr, 'v'},
            {"help",            0, nullptr, 'h'},
            {nullptr,           0, nullptr, 0},
    };

    while (true) {
        int i = getopt_long(*argc, *argv, "lfp:vh", opts, nullptr);

        if (i == -1) break;

        switch (i) {
            case 'l':
                displayLongListing = true;
                break;
            case 'f':
                shouldFindDuplicates = true;
                break;
            case 'p':
                displayLongListing = true;
                patchToDisplay = strtol(optarg, nullptr, 0);
                break;
            case 'v':
                std::cout << "dx7dump " << version << std::endl;
                std::cout << "Yamaha DX7 Sysex Dump" << std::endl;
                std::cout << "Copyright 2012, Ted Felix (GPLv3+)" << std::endl;
                std::cout << "Updated in 2019 by Francois W. Nel" << std::endl;
                exit(0);
            case 'h':
                std::cout << "Usage: dx7dump [OPTIONS] filename" << std::endl;
                std::cout << std::endl;
                std::cout << "Options:\t-l\tList all parameters for all 32 patches." << std::endl;
                std::cout << "\t\t-p n\tList all parameters for the specified patch." << std::endl;
                std::cout << "\t\t-v\tDisplay version information." << std::endl;
                std::cout << "\t\t-h\tDisplay this help information." << std::endl;
                exit(0);
            default:
                std::cout << "Unexpected option. Try -h for help." << std::endl;
                exit(1);
        }
    }

    // Bump to the end of the options.
    *argc -= optind;
    *argv += optind;

    return 0;
}

// ***************************************************************************

int verify(const DX7Sysex *sysex) {
    if (sysex->sysexBeginF0 != 0xF0) {
        std::cout << "Did not find sysex start 0xF0." << std::endl;
        return 1;
    }

    if (sysex->yamaha43 != 0x43) {
        std::cout << "Did not find Yamaha 0x43." << std::endl;
        return 1;
    }

    if (sysex->subStatusAndChannel != 0) {
        std::cout << "Did not find substatus 0 and channel 1." << std::endl;
        return 1;
    }

    if (sysex->format9 != 0x09) {
        std::cout << "Did not find format 9 (32 voices)." << std::endl;
        return 1;
    }

    if (sysex->sizeMsb != 0x20 || sysex->sizeLsb != 0) {
        std::cout << "Did not find size 4096." << std::endl;
        return 1;
    }

    if (sysex->sysexEndF7 != 0xF7) {
        std::cout << "Did not find sysex end 0xF7." << std::endl;
        return 1;
    }

    const unsigned char *p = (unsigned char *) sysex->voices;
    unsigned char sum = 0;

    for (unsigned i = 0; i < 4096; ++i) {
        sum += (p[i] & 0x7F);
    }

    // Two's complement.
    sum = static_cast<unsigned char>((~sum) + 1);

    // Mask to 7 bits.
    sum &= 0x7F;

    if (sum != sysex->checksum) {
        std::cout << "Checksum failed: Should have been 0x" << +sum << std::endl;
        return 1;
    }

    return 0;
}

// ***************************************************************************

void format(const DX7Sysex *sysex, const char *filename, bool displayLongListing, long patchToDisplay) {
    for (unsigned voiceNumber = 0; voiceNumber < 32; ++voiceNumber) {
        if (patchToDisplay >= 0 && patchToDisplay != voiceNumber + 1) continue;

        const VoicePacked *voice = &(sysex->voices[voiceNumber]);

        char name[11];
        memcpy(name, voice->name, 10);
        name[10] = 0;

        if (!displayLongListing) {
            std::cout << std::setfill('0') << std::setw(2) << voiceNumber + 1 << ": " << name << std::endl;
            continue;
        }

        std::cout << std::endl;
        std::cout << "Filename: " << filename << std::endl;
        std::cout << "Voice: " << std::setfill('0') << std::setw(2) << voiceNumber + 1 << std::endl;
        std::cout << "Name: " << name << std::endl;
        std::cout << std::endl;
        std::cout << "Algorithm: " << std::setfill('0') << std::setw(2) << voice->algorithm + 1 << std::endl;
        std::cout << "Pitch Envelope Generator:" << std::endl;
        std::cout << "  Rate 1: " << std::setfill('0') << std::setw(2) << +voice->pitchEgRate1 << std::endl;
        std::cout << "  Rate 2: " << std::setfill('0') << std::setw(2) << +voice->pitchEgRate2 << std::endl;
        std::cout << "  Rate 3: " << std::setfill('0') << std::setw(2) << +voice->pitchEgRate3 << std::endl;
        std::cout << "  Rate 4: " << std::setfill('0') << std::setw(2) << +voice->pitchEgRate4 << std::endl;
        std::cout << "  Level 1: " << std::setfill('0') << std::setw(2) << +voice->pitchEgLevel1 << std::endl;
        std::cout << "  Level 2: " << std::setfill('0') << std::setw(2) << +voice->pitchEgLevel2 << std::endl;
        std::cout << "  Level 3: " << std::setfill('0') << std::setw(2) << +voice->pitchEgLevel3 << std::endl;
        std::cout << "  Level 4: " << std::setfill('0') << std::setw(2) << +voice->pitchEgLevel4 << std::endl;
        std::cout << "Feedback: " << std::setfill('0') << std::setw(2) << +voice->feedback << std::endl;
        std::cout << "Oscillator Key Sync: " << std::setfill('0') << std::setw(2) << +voice->oscillatorKeySync;
        std::cout << " (" << onOff(voice->oscillatorKeySync) << ")" << std::endl;
        std::cout << "LFO:" << std::endl;
        std::cout << "  Rate: " << std::setfill('0') << std::setw(2) << +voice->lfoRate << std::endl;
        std::cout << "  Delay: " << std::setfill('0') << std::setw(2) << +voice->lfoDelay << std::endl;
        std::cout << "  Amp Mod Depth: " << std::setfill('0') << std::setw(2) << +voice->lfoAmplitudeModulationDepth;
        std::cout << std::endl;
        std::cout << "  Pitch Mod Depth: " << std::setfill('0') << std::setw(2) << +voice->lfoPitchModulationDepth;
        std::cout << std::endl;
        std::cout << "  Key Sync: " << std::setfill('0') << std::setw(2) << +voice->lfoKeySync;
        std::cout << " (" << onOff(voice->lfoKeySync) << ")" << std::endl;
        std::cout << "  Wave: " << std::setfill('0') << std::setw(2) << +voice->lfoWave;
        std::cout << " (" << lfoWave(voice->lfoWave) << ")" << std::endl;
        std::cout << "Pitch Mod Sense: " << std::setfill('0') << std::setw(2) << +voice->lfoPitchModulationSensitivity;
        std::cout << std::endl;
        std::cout << "Transpose: " << std::setfill('0') << std::setw(2) << +voice->transpose;
        std::cout << " (" << transpose(voice->transpose).c_str() << ")" << std::endl;

        for (unsigned i = 0; i < 6; ++i) {
            std::cout << std::endl << "Operator " << std::setfill('0') << std::setw(2) << i + 1 << ": " << std::endl;

            // Operators are stored in backwards order.
            const unsigned j = 5 - i;
            const OperatorPacked &op = voice->operators[j];

            std::cout << "  Envelope Generator:" << std::endl;
            std::cout << "    Rate 1: " << std::setfill('0') << std::setw(2) << +op.egRate1 << std::endl;
            std::cout << "    Rate 2: " << std::setfill('0') << std::setw(2) << +op.egRate2 << std::endl;
            std::cout << "    Rate 3: " << std::setfill('0') << std::setw(2) << +op.egRate3 << std::endl;
            std::cout << "    Rate 4: " << std::setfill('0') << std::setw(2) << +op.egRate4 << std::endl;
            std::cout << "    Level 1: " << std::setfill('0') << std::setw(2) << +op.egLevel1 << std::endl;
            std::cout << "    Level 2: " << std::setfill('0') << std::setw(2) << +op.egLevel2 << std::endl;
            std::cout << "    Level 3: " << std::setfill('0') << std::setw(2) << +op.egLevel3 << std::endl;
            std::cout << "    Level 4: " << std::setfill('0') << std::setw(2) << +op.egLevel4 << std::endl;
            std::cout << "  Level Scale:" << std::endl;
            std::cout << "    Break Point: " << std::setfill('0') << std::setw(2) << +op.levelScaleBreakPoint;
            std::cout << " (" << breakPoint(op.levelScaleBreakPoint).c_str() << ")" << std::endl;
            std::cout << "    Left Depth: " << std::setfill('0') << std::setw(2) << +op.levelScaleLeftDepth;
            std::cout << std::endl;
            std::cout << "    Right Depth: " << std::setfill('0') << std::setw(2) << +op.levelScaleRightDepth;
            std::cout << std::endl;
            std::cout << "    Left Curve: " << std::setfill('0') << std::setw(2) << +op.levelScaleLeftCurve;
            std::cout << " (" << curve(op.levelScaleLeftCurve) << ")" << std::endl;
            std::cout << "    Right Curve: " << std::setfill('0') << std::setw(2) << +op.levelScaleRightCurve;
            std::cout << " (" << curve(op.levelScaleRightCurve) << ")" << std::endl;
            std::cout << "  Oscillator Rate Scale: " << std::setfill('0') << std::setw(2) << +op.oscillatorRateScale;
            std::cout << std::endl;
            std::cout << "  Amp Mod Sense: " << std::setfill('0') << std::setw(2) << +op.amplitudeModulationSensitivity;
            std::cout << std::endl;
            std::cout << "  Key Velocity Sense: " << std::setfill('0') << std::setw(2) << +op.keyVelocitySensitivity;
            std::cout << std::endl;
            std::cout << "  Output Level: " << std::setfill('0') << std::setw(2) << +op.outputLevel << std::endl;
            std::cout << "  Oscillator Mode: " << std::setfill('0') << std::setw(2) << +op.oscillatorMode;
            std::cout << " (" << mode(op.oscillatorMode) << ")" << std::endl;
            std::cout << "  Frequency Course: ";

            if (op.oscillatorMode == 0) {
                // Ratio mode.
                std::cout << std::setfill('0') << std::setw(2) << +op.frequencyCoarse << std::endl;
            } else {
                // Fixed mode.
                const double power = static_cast<double>(op.frequencyCoarse % 4) +
                                     static_cast<double>(op.frequencyFine) / 100;
                const double frequency = pow(10, power);

                std::cout << frequency << " Hz" << std::endl;
            }

            std::cout << "  Frequency Fine: " << std::setfill('0') << std::setw(2) << +op.frequencyFine << std::endl;
            std::cout << "  Detune: " << std::setfill('0') << std::setw(2) << +op.detune << std::endl;
        }

        std::cout << std::endl << "-------------------------------------------------" << std::endl << std::endl;
    }
}

// ***************************************************************************

void findDuplicates(const DX7Sysex *sysex) {
    for (int i = 0; i < 31; ++i) {
        // For each patch after that patch.
        for (int j = i + 1; j < 32; ++j) {
            // Compare the patches. Subtract 10 to remove the name from the diff.
            int rc = memcmp(&(sysex->voices[i]), &(sysex->voices[j]), sizeof(VoicePacked) - 10);

            if (rc == 0)
                std::cout << "Found duplicates: Voice " << i + 1 << " and voice " << j + 1 << "." << std::endl;
        }
    }
}

// ***************************************************************************

int main(int argc, char *argv[]) {
    const size_t sysexSize = 4104;
    const char *version = "1.01";
    bool displayLongListing = false; // "-l" for long listing.
    bool shouldFindDuplicates = false; // "-f" to find duplicate patches.
    int patchToDisplay = -1; // "-p" to specify patch. -1 means all.

    processOptions(&argc, &argv, version, displayLongListing, shouldFindDuplicates, patchToDisplay);

    if (argc == 0) {
        std::cout << "Error: Please specify a sysex file." << std::endl;
        return 1;
    }

    const char *filename = argv[0];

    std::ifstream input(filename, std::ios::binary);

    if (!input.is_open()) {
        std::cout << "Error: Can't open " << filename << ": " << strerror(errno) << std::endl;
        return 1;
    }

    std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(input), {});
    input.close();

    if (buffer.size() != sysexSize) {
        std::cout << "Error: " << filename << " does not match the expected size of a sysex file." << std::endl;
        return 1;
    }

    const DX7Sysex *sysex = reinterpret_cast<DX7Sysex *>(&buffer[0]);

    if (verify(sysex) != 0) {
        std::cout << "Error: " << filename << " is not a valid sysex file." << std::endl;
        return 1;
    }

    format(sysex, filename, displayLongListing, patchToDisplay);

    if (shouldFindDuplicates)
        findDuplicates(sysex);

    return 0;
}

