#ifndef TYPES_H
#define TYPES_H


// These typedefs are written for x86_64.
// Please note the O.S. here uses like most unixes LP64

//    Datatype | Bits in LP64
//    ---------|-----
//    char     |            8
//    short    |           16
//    _int     |           32
//    int      |           32
//    long     |           64
//    pointer  |           64

typedef char             int8;
typedef short           int16;
typedef int             int32;

typedef unsigned char   uint8;
typedef unsigned short uint16;
typedef unsigned int   uint32;

typedef unsigned long  uint64;

typedef unsigned long  size_t;

/* C23 makes bool/true/false language keywords. */
#if !defined(__cplusplus) && (!defined(__STDC_VERSION__) || (__STDC_VERSION__ < 202311L))
typedef uint8 bool;
#endif

#define NULL ((void *)0)
#if !defined(__cplusplus) && (!defined(__STDC_VERSION__) || (__STDC_VERSION__ < 202311L))
#define true (1==1)
#define false (!true)
#endif

#endif
