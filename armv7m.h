#ifndef ARMV7m_h
#define ARMV7m_h

#include <stdint.h>

#define UART(N) ((N)+(void*)0x4000c000)
#define UART_DATA UART(0)
#define UART_FLAG UART(0x18)

/* ARM System Control Block */
#define SCB(N) ((N)+(void*)0xe000e000)
/* TI System Control registers */
#define SYSCON(N) ((N)+(void*)0x400fe000)
/* TI GPIO */
#define GPIO(N,OFF) ((OFF)+0x1000*(N)+(void*)0x40058000)

#ifndef PRIGROUP
#define PRIGROUP 4
#endif

#define PRIO_SUB_MASK ((1<<(PRIGROUP+1))-1)
#define PRIO_GRP_MASK ((PRIO_SUB_MASK)^0xff)
#define PRIO_GRP(P) (((P)<<(PRIGROUP+1))&PRIO_GRP_MASK)
#define PRIO_SUB(S) ((S)&PRIO_SUB_MASK)
#define PRIO(P,S) (PRIO_GRP(P)|PRIO_SUB(S))

/* The TMC1294 only implements 3 bit priority fields.
 * So PRIGROUP 0-4 imply 4 with no sub-priority
 */
#define PRIGROUP_BITS 3
#define PRIO_MASK (((1<<3)-1)<<(8-PRIGROUP_BITS))


typedef void (*vectfn)(void);

typedef struct {
	char *boot_stack;
	vectfn reset;
	vectfn nmi;
	vectfn hard;
	vectfn mem;
	vectfn bus;
	vectfn usage;
	uint32_t recv1[4];
	vectfn svc;
	vectfn debug;
	uint32_t recv2;
	vectfn pendsv;
	vectfn systick;
	vectfn irq[8];
} vect_table;

extern vect_table run_table;

static inline
void out8(void *addr, uint8_t val)
{
	volatile uint8_t *A = addr;
	*A = val;
	__asm__ volatile ("dsb" ::: "memory");
}

static inline
void out32(void *addr, uint32_t val)
{
	volatile uint32_t *A = addr;
	*A = val;
	__asm__ volatile ("dsb" ::: "memory");
}

static inline
uint32_t in32(void *addr)
{
	volatile uint32_t *A = addr;
	__asm__ volatile ("dsb" ::: "memory");
	return *A;
}

#define rmw(N, ADDR, MASK, VAL) \
    do{uint##N##_t temp = in##N(ADDR);\
    temp&=~(MASK); temp|=(VAL)&(MASK);\
    out##N(ADDR, temp);\
    }while(0)

#define loop_until(N, ADDR, MASK, OP, VAL) \
    do{}while(!((in##N(ADDR)&(MASK))OP VAL))

void abort(void) __attribute__((noreturn));

static inline
void putc(char c)
{
    if(c=='\n')
        putc('\r');
    loop_until(32, UART_FLAG, 0x20, ==, 0); /* wait for TX FIFO not full */
    out8(UART_DATA, c);
}

static inline
void puts(const char *s)
{
	char c;
	while((c=*s++)!='\0')
		putc(c);
}

static inline
void flush(void)
{
    /* wait for TX FIFO empty */
    loop_until(32, UART_FLAG, 0x80, ==, 0x80);
}

extern char hexchars[16];

static inline
void puthex(uint32_t v)
{
	unsigned i;
	for(i=0; i<8; i++, v<<=4) {
		putc(hexchars[v>>28]);
	}
}

#define CPSIE(MASK) __asm__ volatile ("cpsie " #MASK ::: "memory")
#define CPSID(MASK) __asm__ volatile ("cpsid " #MASK ::: "memory")

static inline
void basepri(uint8_t prio)
{
    prio = PRIO_GRP(prio);
    __asm__ volatile ("msr BASEPRI, %0" : : "r"(prio) : "memory");
}

#define SVC(N) __asm__ volatile ("svc " #N ::: "memory")

#define MPU_XN (1<<28)

#define MPU_NANA (0<<24)
#define MPU_RWNA (1<<24)
#define MPU_RWRO (2<<24)
#define MPU_RWRW (3<<24)
#define MPU_RONA (5<<24)
#define MPU_RORO (6<<24)

#define MPU_STRONG (0<<16)
#define MPU_DEVICE (1<<16)
#define MPU_NORMAL (6<<16)

/* ceil(log(v, 2))
 *  log2_ceil(31) -> 6
 *  log2_ceil(32) -> 6
 *  log2_ceil(33) -> 7
 */
static inline
unsigned log2_ceil(uint32_t v)
{
    unsigned r=0, c=0;
    while(v) {
        c += v&1;
        v >>= 1;
        r++;
    }
    if(c>1) r++;
    return r;
}

static inline
void set_mpu(unsigned region, uint32_t base, uint32_t size,
			 uint32_t attrs)
{
	unsigned sbits = log2_ceil(size<32 ? 32 : size)-2;
	uint32_t rbase = base&(~0x1f);
	uint32_t rattr = (attrs&~0xffff) | (sbits<<1) | 1;
	puts("set_mpu ");
	putc('0'+region);
	putc(' ');
	puthex(rbase);
	putc(' ');
	puthex(rattr);
	putc('\n');
	out32((void*)0xe000ed98, region&0xff); /* RNR */
	out32((void*)0xe000eda0, 0); /* Disable */
	out32((void*)0xe000ed9c, rbase);
	out32((void*)0xe000eda0, rattr);
}

static inline
void enable_mpu(unsigned usrena, unsigned privena, unsigned hfnmiena)
{
    uint32_t val = (usrena ? 1 : 0) | (hfnmiena ? 2 : 0) | (privena ? 0 : 4);
	out32((void*)0xe000ed94, val);
    __asm__ volatile ("dsb\nisb" :::);
}

uint32_t* get_src_stack(uint32_t *sp);
void inst_skip(uint32_t *sp);
int get_svc(uint32_t *sp);

#endif /* ARMV7m_h */
