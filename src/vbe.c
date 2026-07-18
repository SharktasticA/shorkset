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



#include "shorkmenu.h"
#include "vbe.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>



int VBE_INITIALISED = 0;
void *LOW_MEM = NULL;
int MEM_FD = -1;
uint16_t MODE_LIST[MODE_LIST_MAX];
int MODE_LIST_COUNT = 0;
int MODE_LIST_LOADED = 0;



/**
 * Loads MODE_LIST with supported VESA mode numbers found from a AX=4F00h call.
 * @return 1 if at least one mode loaded; 0 if none or error
 */
int loadModeList(void)
{
    if (MODE_LIST_LOADED)
        return MODE_LIST_COUNT > 0;
    // We set MODE_LIST_LOADED to 1 even if this func ultimately fails to
    // prevent endless, fruitless retrying later...
    MODE_LIST_LOADED = 1;
    MODE_LIST_COUNT = 0;

    VBE_INFO_BLOCK info;
    if (vbeGetInfo(&info) != 1)
        return 0;

    unsigned seg = (unsigned)(info.videoModePtr >> 16) & 0xFFFF;
    unsigned off = (unsigned)(info.videoModePtr & 0xFFFF);
    uintptr_t addr = ((uintptr_t)seg << 4) + off;

    // Guard against going above the low 1MB we actually mapped
    if (addr + (MODE_LIST_MAX * 2) > 0x100000)
        return 0;

    uint16_t *modes = (uint16_t*)addr;
    for (int i = 0; i < MODE_LIST_MAX; i++)
    {
        if (modes[i] == 0xFFFF) break;
        MODE_LIST[MODE_LIST_COUNT++] = modes[i];
    }

    return MODE_LIST_COUNT > 0;
}

/**
 * Runs a real-mode INT 10h call via the kernel's VM86OLD syscall.
 * @param inout CPU registers to work with (passed by reference)
 * @return 1 if successful; 0 if not
 */
int runInt10(VM86_REGS *inout)
{
    VM86_BLOCK vm;
    memset(&vm, 0, sizeof(vm));

    // Load the requested register values (inout) then point CS:EIP to the
    // real-mode stub to run
    vm.regs = *inout;
    vm.regs.cs  = STUB_SEG;
    vm.regs.eip = 0;
    vm.regs.ss  = STACK_SEG;
    vm.regs.esp = STACK_OFF;
    vm.regs.eflags = VM_FLAG | IOPL3_FLAG;
    vm.regs.ds = vm.regs.es;

    // Tell the kernel to trap INT 0xFF back to us
    vm.int_revectored.__map[0xFF / 32] |= 1UL << (0xFF % 32);

    for (int i = 0; i < 128; i++)
    {
        // Hand control to the CPU in VM86 mode; returns when trap occurs
        long ret = syscall(SYS_VM86OLD, &vm);
        if (ret < 0)
            return 0;

        // Reached our INT 0xFF sentinel, so return the result registers via
        // inout...
        if (VM86_TYPE(ret) == VM86_INTX && VM86_ARG(ret) == 0xFF)
        {
            *inout = vm.regs;
            return 1;
        }

        // On VM86_UNKNOWN...
        if (VM86_TYPE(ret) == 1)
            return 0;
    }

    return 0;
}

/**
 * Makes an AX=4F00h call to get VBE_INFO_BLOCK. We need it for its videoModePtr,
 * which points to a list of mode numbers the BIOS actually supports.
 * @param out Retrieved VBE_INFO_BLOCK (passed by reference)
 * @return 1 if successful; 0 if not
 */
int vbeGetInfo(VBE_INFO_BLOCK *out)
{
    if (!VBE_INITIALISED || !out)
        return 0;

    memset((void*)(uintptr_t)XFER_ADDR, 0, sizeof(VBE_INFO_BLOCK));

    VM86_REGS regs;
    memset(&regs, 0, sizeof(regs));
    regs.eax = 0x4F00;
    regs.es  = XFER_SEG;
    regs.edi = 0;

    if (runInt10(&regs) != 1)
        return 0;

    if ((regs.eax & 0xFFFF) != VBE_SUCESS)
        return 0;

    memcpy(out, (void*)(uintptr_t)XFER_ADDR, sizeof(VBE_INFO_BLOCK));
    return 1;
}

/**
 * Makes an AX=4F01h call to get VBE_INFO_MODE_BLOCK. We need it as a fallback
 * method for when AX=4F00h fails.
 * @param vesaMode VESA mode number to query
 * @param out Retrieved VBE_INFO_MODE_BLOCK (passed by reference)
 * @return 1 if successful; 0 if not
 */
int vbeGetModeInfo(int vesaMode, VBE_INFO_MODE_BLOCK *out)
{
    if (!VBE_INITIALISED || !out)
        return 0;

    memset((void*)(uintptr_t)XFER_ADDR, 0, sizeof(VBE_INFO_MODE_BLOCK));

    VM86_REGS regs;
    memset(&regs, 0, sizeof(regs));
    regs.eax = 0x4F01;
    regs.ecx = (unsigned)vesaMode;
    regs.es  = XFER_SEG;
    regs.edi = 0;

    if (runInt10(&regs) != 1)
        return 0;

    if ((regs.eax & 0xFFFF) != VBE_SUCESS)
        return 0;

    memcpy(out, (void*)(uintptr_t)XFER_ADDR, sizeof(VBE_INFO_MODE_BLOCK));
    return 1;
}

/**
 * Makes preparations to enter VM86 mode. Maps low 1MB of physical memory and
 * creates real-mode call stub for VM86.
 * @return 1 if successful; 0 if not
 */
int vbeInit(void)
{
    // Code not ready - remove this return once it is!
    return 1;

    if (VBE_INITIALISED)
        return 1;
    if (geteuid() != 0)
        return 0;

    MEM_FD = open("/dev/mem", O_RDWR);
    if (MEM_FD < 0)
        return 0;

    // Map the first 1MB of physical memory
    LOW_MEM = mmap((void*)0, 0x100000, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED | MAP_FIXED, MEM_FD, 0);
    if (LOW_MEM == MAP_FAILED)
    {
        close(MEM_FD);
        MEM_FD = -1;
        return 0;
    }

    // Prepare opcodes for runInt10() to run to enter VM86 mode (int 0x10 + int
    // 0xFF)
    unsigned char *stub = (unsigned char*)(uintptr_t)STUB_ADDR;
    stub[0] = 0xCD; stub[1] = 0x10;
    stub[2] = 0xCD; stub[3] = 0xFF;

    VBE_INITIALISED = 1;
    return 1;
}

/**
 * Unmaps low memory. Use immediately after use VM86 is no longer needed.
 */
void vbeShutdown(void)
{
    // Code not ready - remove this return once it is!
    return;

    if (LOW_MEM)
        munmap(LOW_MEM, 0x100000);
    if (MEM_FD >= 0)
        close(MEM_FD);
    LOW_MEM = NULL;
    MEM_FD = -1;
    VBE_INITIALISED = 0;
    MODE_LIST_LOADED = 0;
    MODE_LIST_COUNT = 0;
}

/**
 * Checks if the give Linux VGA mode number represents a resolution reported as
 * available by VBE.
 * @param vgaMode Linux VGA mode number to query
 * @return 1 if available; 0 if not
 */
int vgaModeAvailable(int vgaMode)
{
    // Code not ready - remove this return once it is!
    return 1;

    int vesaMode = vgaToVESAMode(vgaMode);
    if (vesaMode < 0)
        return 0;

    // Check the list of VESA mode numbers returned by AX=4F00h to see if our
    // number is present
    if (loadModeList())
    {
        for (int i = 0; i < MODE_LIST_COUNT; i++)
            // Check with and without "use linear framebuffer" bit (0x4000) set
            // in case AX=4F00h was called with it
            if (MODE_LIST[i] == vesaMode || MODE_LIST[i] == (vesaMode | 0x4000))
                return 1;
    }

    // If AX=4F00h failed or returned nothing, try probing for the specific
    // mode with AX=4F01h instead. This is known to be required for VMware
    // SVGA Adapter II.
    VBE_INFO_MODE_BLOCK info;
    if (vbeGetModeInfo(vesaMode, &info) != 1)
        return 0;
    // Make sure the mode has linear frame buffer (LFB)
    int hasLFB = (info.modeAttribs & VBE_MODE_LFB_AVAILABLE) ? 1 : 0;
    return ((info.modeAttribs & VBE_MODE_SUPPORTED) && hasLFB) ? 1 : 0;
}

/**
 * Converts a Linux VGA mode number to a real VESA mode number.
 * @param vgaMode VGA mode number to convert
 * @return Result VESA mode number if successful; -1 if invalid
 */
int vgaToVESAMode(int vgaMode)
{
    int vesaMode = vgaMode - 0x200;
    if (vesaMode < 0x100 || vesaMode > 0x8FF)
        return -1;
    return vesaMode;
}
