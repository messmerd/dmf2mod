
#include "instruments.h"

const int16_t sqwSampleLength = 32;
const int8_t sqwSampleDuty[4][32] = {
    {127, 127, 127, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* Duty cycle = 12.5% */ 
    {127, 127, 127, 127, 127, 127, 127, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* Duty cycle = 25% */ 
    {127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* Duty cycle = 50% */ 
    {127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 0, 0, 0, 0, 0, 0, 0, 0} /* Duty cycle = 75% */ 
};
const char sqwSampleNames[4][22] = {"SQUARE - Duty 12.5\%   ", "SQUARE - Duty 25\%     ", "SQUARE - Duty 50\%     ", "SQUARE - Duty 75\%     "};


Instrument loadInstrument(FILE *filePointer, System systemType)
{
    Instrument inst; 

    uint8_t name_size = fgetc(filePointer); 
    inst.name = (char *)malloc(name_size * sizeof(char)); 
    fgets(inst.name, name_size + 1, filePointer);

    inst.mode = fgetc(filePointer); // 1 = FM; 0 = Standard 
    
    if (inst.mode == 1) // FM instrument 
    {
        inst.fmALG = fgetc(filePointer); 
        inst.fmFB = fgetc(filePointer); 
        inst.fmLFO = fgetc(filePointer); 
        inst.fmLFO2 = fgetc(filePointer); 

        int TOTAL_OPERATORS = 1;  // I'm not sure what toal operators is or where I'm supposed to get it from 
        for (int i = 0; i < TOTAL_OPERATORS; i++)
        {
            inst.fmAM = fgetc(filePointer); 
            inst.fmAR = fgetc(filePointer); 
            inst.fmDR = fgetc(filePointer); 
            inst.fmMULT = fgetc(filePointer); 
            inst.fmRR = fgetc(filePointer); 
            inst.fmSL = fgetc(filePointer); 
            inst.fmTL = fgetc(filePointer); 
            inst.fmDT2 = fgetc(filePointer); 
            inst.fmRS = fgetc(filePointer); 
            inst.fmDT = fgetc(filePointer); 
            inst.fmD2R = fgetc(filePointer); 
            inst.fmSSGMODE = fgetc(filePointer); 
        }
    }
    else if (inst.mode == 0) // Standard instrument 
    {
        if (strcmp(systemType.name, "GAMEBOY") != 0)  // Not a GameBoy  
        {
            // Volume macro 
            inst.stdVolEnvSize = fgetc(filePointer); 
            inst.stdVolEnvValue = (int32_t *)malloc(inst.stdVolEnvSize * sizeof(int32_t));
            for (int i = 0; i < inst.stdVolEnvSize; i++)
            {
                // 4 bytes, little-endian 
                inst.stdVolEnvValue[i] = fgetc(filePointer); 
                inst.stdVolEnvValue[i] |= fgetc(filePointer) << 8;
                inst.stdVolEnvValue[i] |= fgetc(filePointer) << 16;
                inst.stdVolEnvValue[i] |= fgetc(filePointer) << 24;
            }
            if (inst.stdVolEnvSize > 0) 
                inst.stdVolEnvLoopPos = fgetc(filePointer); 
        }

        // Arpeggio macro 
        inst.stdArpEnvSize = fgetc(filePointer); 
        inst.stdArpEnvValue = (int32_t *)malloc(inst.stdArpEnvSize * sizeof(int32_t));
        for (int i = 0; i < inst.stdArpEnvSize; i++)
        {
            // 4 bytes, little-endian 
            inst.stdArpEnvValue[i] = fgetc(filePointer); 
            inst.stdArpEnvValue[i] |= fgetc(filePointer) << 8;
            inst.stdArpEnvValue[i] |= fgetc(filePointer) << 16;
            inst.stdArpEnvValue[i] |= fgetc(filePointer) << 24;
        }

        if (inst.stdArpEnvSize > 0)
            inst.stdArpEnvLoopPos = fgetc(filePointer ); 
        inst.stdArpMacroMode = fgetc(filePointer); 

        // Duty/Noise macro 
        inst.stdDutyNoiseEnvSize = fgetc(filePointer); 
        inst.stdDutyNoiseEnvValue = (int32_t *)malloc(inst.stdDutyNoiseEnvSize * sizeof(int32_t));
        for (int i = 0; i < inst.stdDutyNoiseEnvSize; i++)
        {
            // 4 bytes, little-endian 
            inst.stdDutyNoiseEnvValue[i] = fgetc(filePointer); 
            inst.stdDutyNoiseEnvValue[i] |= fgetc(filePointer) << 8;
            inst.stdDutyNoiseEnvValue[i] |= fgetc(filePointer) << 16;
            inst.stdDutyNoiseEnvValue[i] |= fgetc(filePointer) << 24;
        }
        if (inst.stdDutyNoiseEnvSize > 0) 
            inst.stdDutyNoiseEnvLoopPos = fgetc(filePointer); 

        // Wavetable macro 
        inst.stdWavetableEnvSize = fgetc(filePointer); 
        inst.stdWavetableEnvValue = (int32_t *)malloc(inst.stdWavetableEnvSize * sizeof(int32_t));
        for (int i = 0; i < inst.stdWavetableEnvSize; i++)
        {
            // 4 bytes, little-endian 
            inst.stdWavetableEnvValue[i] = fgetc(filePointer); 
            inst.stdWavetableEnvValue[i] |= fgetc(filePointer) << 8;
            inst.stdWavetableEnvValue[i] |= fgetc(filePointer) << 16;
            inst.stdWavetableEnvValue[i] |= fgetc(filePointer) << 24;
        }
        if (inst.stdWavetableEnvSize > 0) 
            inst.stdWavetableEnvLoopPos = fgetc(filePointer); 

        // Per system data
        if (strcmp(systemType.name, "C64_SID_8580") == 0 || strcmp(systemType.name, "C64_SID_6581") == 0) // Using Commodore 64 
        {
            
            inst.stdC64TriWaveEn = fgetc(filePointer); 
            inst.stdC64SawWaveEn = fgetc(filePointer); 
            inst.stdC64PulseWaveEn = fgetc(filePointer); 
            inst.stdC64NoiseWaveEn = fgetc(filePointer); 
            inst.stdC64Attack = fgetc(filePointer); 
            inst.stdC64Decay = fgetc(filePointer); 
            inst.stdC64Sustain = fgetc(filePointer); 
            inst.stdC64Release = fgetc(filePointer); 
            inst.stdC64PulseWidth = fgetc(filePointer); 
            inst.stdC64RingModEn = fgetc(filePointer); 
            inst.stdC64SyncModEn = fgetc(filePointer); 
            inst.stdC64ToFilter = fgetc(filePointer); 
            inst.stdC64VolMacroToFilterCutoffEn = fgetc(filePointer); 
            inst.stdC64UseFilterValuesFromInst = fgetc(filePointer); 
            
            // Filter globals 
            inst.stdC64FilterResonance = fgetc(filePointer); 
            inst.stdC64FilterCutoff = fgetc(filePointer); 
            inst.stdC64FilterHighPass = fgetc(filePointer); 
            inst.stdC64FilterLowPass = fgetc(filePointer); 
            inst.stdC64FilterCH2Off = fgetc(filePointer); 
        }
        else if (strcmp(systemType.name, "GAMEBOY") == 0) // Using GameBoy 
        {
            inst.stdGBEnvVol = fgetc(filePointer); 
            inst.stdGBEnvDir = fgetc(filePointer); 
            inst.stdGBEnvLen = fgetc(filePointer); 
            inst.stdGBSoundLen = fgetc(filePointer); 
        }
    }

    return inst; 
}

PCMSample loadPCMSample(FILE *filePointer)
{
    PCMSample sample; 

    sample.size = fgetc(filePointer);
    sample.size |= fgetc(filePointer) << 8;
    sample.size |= fgetc(filePointer) << 16;
    sample.size |= fgetc(filePointer) << 24;

    uint8_t name_size = fgetc(filePointer); 
    sample.name = (char *)malloc(name_size * sizeof(char)); 
    fgets(sample.name, name_size + 1, filePointer);

    sample.rate = fgetc(filePointer); 
    sample.pitch = fgetc(filePointer); 
    sample.amp = fgetc(filePointer); 
    sample.bits = fgetc(filePointer); 

    sample.data = (uint16_t *)malloc(sample.size * sizeof(uint16_t *)); 
    for (uint32_t i = 0; i < sample.size; i++) 
    {
        sample.data[i] = fgetc(filePointer); 
        sample.data[i] |= fgetc(filePointer) << 8; 
    }

    return sample; 
}

