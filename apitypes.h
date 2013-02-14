#ifndef __APITYPES_H__
#define __APITYPES_H__

#ifdef __GNUC__
#define PACKED __attribute__((packed))
#define ALIGNED __attribute__((aligned(0x4)))
#else
#define PACKED
#define ALIGNED
#endif

typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef signed short   int16;
typedef unsigned long  uint32;
typedef signed char    int8;

typedef struct bd_addr_t
{
    uint8 addr[6];

}bd_addr;

typedef bd_addr hwaddr;
typedef struct
{
    uint8 len;
    uint8 data[];
}uint8array;

typedef struct
{
    uint8 len;
    int8 data[];
}string;

#endif
