/* See how various faults are reported and recovered: UNDEF
 */
#include "armv7m.h"
#include "testme.h"

void inst_skip(uint32_t *sp);

static
volatile unsigned fault_type;

static volatile unsigned fsr;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

const char *faultnames[] = {
    "None",
    "Mem",
    "Bus",
    "Usage",
};

typedef struct FSRMapEntry {
    unsigned int bit;
    const char *name;
} FSRMapEntry;

static const FSRMapEntry fsrmap[] = {
    { 0x1, "IACCVIOL" },
    { 0x2, "DACCVIOL" },
    { 0x8, "MUNSTKERR" },
    { 0x10, "MSTKERR" },
    { 0x20, "MLSPERR" },
    { 0x80, "MMARVALID" },
    { 0x100, "IBUSERR" },
    { 0x200, "PRECISERR" },
    { 0x400, "IMPRECISERR" },
    { 0x800, "UNSTKERR" },
    { 0x1000, "STKERR" },
    { 0x2000, "LSPERR" },
    { 0x8000, "BFARVALID" },
    { 0x10000, "UNDEFINSTR" },
    { 0x20000, "INVSTATE" },
    { 0x40000, "INVPC" },
    { 0x80000, "NOCP" },
    { 0x1000000, "UNALIGNED" },
    { 0x2000000, "DIVBYZERO" },
};

/* Only reports the first set bit! */
const char *fsrbit(unsigned int fsr)
{
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(fsrmap); i++) {
        if (fsr & fsrmap[i].bit) {
            return fsrmap[i].name;
        }
    }
    return "reserved";
}

const char *faultname(unsigned int faultnum)
{
    return faultnum < ARRAY_SIZE(faultnames) ? faultnames[faultnum] : "unknown";
}

static
void check_fault(unsigned expect, unsigned expectfsr)
{
    unsigned actual = fault_type;

    //testDiag("Fault: %s FSR: %x (%s)", faultname(actual), fsr, fsrbit(fsr));

    testEqI(expect, actual, "fault type");
    testEqI(expectfsr, fsr, "FSR");

    fault_type = 0;
    fsr = 0;
}

static
void set_fault(unsigned actual, unsigned fsrval)
{
    testDiag("%sFault: %x (%s) FSR: %x (%s)",
             fault_type ? "Secondary " : "",
             actual, faultname(actual), fsrval, fsrbit(fsrval));

    if (!fault_type) {
        fault_type = actual;
        fsr = fsrval;
    }
}

static
void hard(void)
{
    testDiag("Unexpected HardFault");
    abort();
}

void bus(uint32_t *sp)
{
    uint32_t sts, addr;

    sts = in32(SCB(0xd28));
    set_fault(2, sts);

    addr= in32(SCB(0xd38));

    out32(SCB(0xd28), 0xff00); /* W1C */
    out32(SCB(0xd38), 0);

    puts("BusFault: ");
    puthex(sts);
    putc(' ');
    if(sts&0x8000) {
        puthex(addr);
        putc(' ');
    }
    if(sts&0x1000)
        puts("STKERR ");
    if(sts&0x0800)
        puts("UNSTKERR ");
    if(sts&0x0400)
        puts("IMPRECISERR ");
    if(sts&0x0200)
        puts("PRECISERR ");
    if(sts&0x0100)
        puts("IBUSERR ");

    putc('\n');

    if(sts&0x0300) {
        /* precise faults would return to the faulting
         * instruction, which would then fault again
         * since we change nothing, so skip it.
         */
        puts("From: ");
        puthex(sp[6]);
        if(sp[6]<0xfffffff0)
            inst_skip(sp);
    } else {
        puts("From before: ");
        puthex(sp[6]);
    }
    putc('\n');

    if(sp[6]>=0xf0000000) {
        /* evil hack since we know this was a bx instruction */
        sp[6] = sp[5]&~1; /* jump to LR */
    }
}

static __attribute__((naked))
void bus_entry(void)
{
    asm("mov r0, sp");
    asm("b bus");
}

void mem(uint32_t *sp)
{
    uint32_t sts, addr;

    sts = in32(SCB(0xd28));

    set_fault(1, sts);

    addr= in32(SCB(0xd34));

    out32(SCB(0xd28), 0xff); /* W1C */


    puts("MemFault: ");
    puthex(sts);
    putc(' ');
    if(sts&0x80) {
        puthex(addr);
        putc(' ');
    }
    if(sts&0x08)
        puts("MSTKERR ");
    if(sts&0x04)
        puts("MUNSTKERR ");
    if(sts&0x02)
        puts("DACCVIOL ");
    if(sts&0x01)
        puts("IACCVIOL ");

    putc('\n');

    puts("From: ");
    puthex(sp[6]);
    putc('\n');

    if(sp[6]>=0xf0000000) {
        /* evil hack since we know this was a bx instruction */
        sp[6] = sp[5]&~1; /* jump to LR */
    } else {
        inst_skip(sp);
    }
}

static __attribute__((naked))
void mem_entry(void)
{
    asm("mov r0, sp");
    asm("b mem");
}


void usage(uint32_t *sp)
{
    uint32_t sts;
    sts = in32(SCB(0xd28));

    set_fault(3, sts);

    out32(SCB(0xd28), 0xffff0000); /* W1C */

    if(sts&0x20000) {
        abort(); /* attempt to execute ARM mode */
    }

    inst_skip(sp);
}

static __attribute__((naked))
void usage_entry(void)
{
    asm("mov r0, sp");
    asm("b usage");
}

void main(void)
{
    run_table.bus = bus_entry;
    run_table.mem = mem_entry;
    run_table.usage = usage_entry;
    run_table.hard = hard;

    testInit(4);

    out32(SCB(0xd24), 0x70000); /* Enable Bus, Mem, and Usage Faults */

    testDiag("Tests of USAGEFAULTs\n");

    testDiag("1 USAGEFAULT for a random undefined insn should be UNDEFINSTR");
    __asm__("UDF 0x24");
    check_fault(3, 0x10000);

    testDiag("2 USAGEFAULT for a cp insn should be NOCP");
    __asm__("cdp 1, 0, c1, c2, c3, 4");
    check_fault(3, 0x80000);

    testDiag("Done.");
}
