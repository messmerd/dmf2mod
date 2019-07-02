
#include "system_info.h"

System systems[10] = {
    {.id = 0x00, .name = "ERROR", .channels = 0},
	{.id = 0x02, .name = "GENESIS", .channels = 10},
	{.id = 0x12, .name = "GENESIS_CH3", .channels = 13},
	{.id = 0x03, .name = "SMS", .channels = 4},
	{.id = 0x04, .name = "GAMEBOY", .channels = 4},
	{.id = 0x05, .name = "PCENGINE", .channels = 6},
	{.id = 0x06, .name = "NES", .channels = 5},
	{.id = 0x07, .name = "C64_SID_8580", .channels = 3},
	{.id = 0x17, .name = "C64_SID_6581", .channels = 3},
	{.id = 0x08, .name = "YM2151", .channels = 13}
	};


System getSystem(unsigned char systemByte)
{
    for (int i = 1; i < 10; i++)
    {
        if (systems[i].id == systemByte) 
            return systems[i];
    }
    return systems[0]; // Error: System byte invalid  
}

