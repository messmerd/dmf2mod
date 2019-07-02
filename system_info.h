
#ifndef __SYSTEM_INFO_H__
#define __SYSTEM_INFO_H__

typedef struct System
{
    unsigned char id;
    char *name;
    unsigned char channels;
} System;

System getSystem(unsigned char systemByte);

#endif 