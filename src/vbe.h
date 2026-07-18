/*
    ######################################################
    ##             SHORK UTILITY - SHORKSET             ##
    ######################################################
    ## VESA BIOS Extensions (VBE) mode probing via VM86 ##
    ######################################################
    ## Licence: GNU GENERAL PUBLIC LICENSE Version 3    ##
    ######################################################
    ## Kali (links.sharktastica.co.uk)                  ##
    ######################################################
*/



#ifndef VBE
#define VBE

#include <stdint.h>



#pragma pack(push,1)
// 256-bit bitmap marking which interrupts the kernel should trap instead of 
// run
typedef struct {
    unsigned long __map[8];
} REVECTORED_STRUCT;
#pragma pack(pop)

#pragma pack(push,1)
// VBE info block (from INT 0x10, AX=0x4F00)
typedef struct {
    // Signature (should be "VESA")
    char vbeSig[4];
    // VBE version (e.g. 0x0200 for VBE 2.0)
    uint16_t vbeVer;
    // OEM string (vendor name, VBE version, copyright, etc.)
    uint32_t oemStrPtr;
    // Capabilities flag bitfield
    uint32_t capabilities;
    // To video mode numbers (0xFFFF‑terminated)
    uint32_t videoModePtr;
    // Total video memory in 64KB blocks
    uint16_t totalMem;
    // Ignore
    uint8_t reserved[492];
} VBE_INFO_BLOCK;
#pragma pack(pop)

#pragma pack(push,1)
// VBE mode info block (from INT 0x10, AX=0x4F01)
typedef struct {
    uint16_t modeAttribs;
    uint8_t winAAttribs;
    uint8_t winBAttribs;
    uint16_t winGranularity;
    uint16_t winSize;
    uint16_t winASegment;
    uint16_t winBSegment;
    uint32_t winFuncPtr;
    uint16_t pitch;
    uint16_t width;
    uint16_t height;
    uint8_t xCharSize;
    uint8_t yCharSize;
    uint8_t noPlanes;
    uint8_t bitsPerPixel;
    uint8_t banks;
    uint8_t memoryModel;
    uint8_t bankSize;
    uint8_t imagePages;
    uint8_t reserved0;
    uint8_t redMask;
    uint8_t redPos;
    uint8_t greenMask;
    uint8_t greenPos;
    uint8_t blueMask;
    uint8_t BluePos;
    uint8_t reservedMask;
    uint8_t reservedPos;
    uint8_t directColourAttribs;
    uint32_t framebuffer;
    uint8_t reserved1[212];
} VBE_INFO_MODE_BLOCK;
#pragma pack(pop)

// Real-mode CPU register snapshot passed to/from the VM86OLD syscall
typedef struct {
    long ebx, ecx, edx, esi, edi, ebp, eax, __null_ds, __null_es, __null_fs,
    __null_gs, orig_eax, eip;
    unsigned short cs, __csh;
    long eflags, esp;
    unsigned short ss, __ssh, es, __esh, ds, __dsh, fs, __fsh, gs, __gsh;
} VM86_REGS;

// VM86OLD syscall block
typedef struct {
    VM86_REGS regs;
    unsigned long flags;
    unsigned long screen_bitmap;
    unsigned long cpu_type;
    REVECTORED_STRUCT int_revectored;
    REVECTORED_STRUCT int21_revectored;
} VM86_BLOCK;



// VM86_REGS.eflags IOPL bit set to ring 3
#define IOPL3_FLAG              (3 << 12)
// Maximum size for MODE_LIST
#define MODE_LIST_MAX           256
// Real-mode stack offset used while servicing the VM86OLD call
#define STACK_OFF               0x00FEu
// Real-mode stack segment used while servicing the VM86OLD call
#define STACK_SEG               0x0780u
// Segment of the tiny real-mode stub (int 0x10; int 0xFF)
#define STUB_SEG                0x0700u
// Linear address of the tiny real-mode stub (int 0x10; int 0xFF)
#define STUB_ADDR               (STUB_SEG << 4)
// VM86 syscall number
#define SYS_VM86OLD             113
// VBE_INFO_MODE_BLOCK.modeAttribs bit indicating linear framebuffer support
#define VBE_MODE_LFB_AVAILABLE  0x0080
// VBE_INFO_MODE_BLOCK.modeAttribs bit indicating a VBE mode is supported
#define VBE_MODE_SUPPORTED      0x0001
// VBE success return value
#define VBE_SUCESS              0x004F
// Extracts trap argument from a VM86OLD syscall return
#define VM86_ARG(retval)        ((retval) >> 8)
// VM86OLD return type meaning the call was trapped on sentinel int 0xFF
#define VM86_INTX               2
// Extracts trap type from a VM86OLD syscall return value
#define VM86_TYPE(retval)       ((retval) & 0xFF)
// VM86_REGS.eflags VM bit set to put CPU into virtual 8086 mode
#define VM_FLAG                 (1 << 17)
// Segment of the BIOS output's real-mode transfer buffer
#define XFER_SEG                0x0800u
// Linear address of the BIOS output's real-mode transfer buffer
#define XFER_ADDR               (XFER_SEG << 4)

// Low 1MB physical memory region
extern void *LOW_MEM;
// /dev/mem file descriptor
extern int MEM_FD;
// Cached supported VESA mode numbers
extern uint16_t MODE_LIST[MODE_LIST_MAX];
// Number of items in MODE_LIST
extern int MODE_LIST_COUNT;
// Flags if loadModeList() has attempted to load MODE_LIST
extern int MODE_LIST_LOADED;
// Flags if vbeInit() has successfully run
extern int VBE_INITIALISED;



int loadModeList(void);
int runInt10(VM86_REGS*);
int vbeGetInfo(VBE_INFO_BLOCK*);
int vbeGetModeInfo(int, VBE_INFO_MODE_BLOCK*);
int vbeInit(void);
void vbeShutdown(void);
int vgaModeAvailable(int);
int vgaToVESAMode(int);

#endif
