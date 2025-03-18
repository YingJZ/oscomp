#include <kernel/riscv.h>
#include <kernel/sched/process.h>
#include <spike_interface/spike_utils.h>
#include <spike_interface/spike_file.h>
#include <util/string.h>
#include <util/printf.h>

// 中断原因码
#define IRQ_M_SOFT    3  // 机器模式软件中断
#define IRQ_M_TIMER   7  // 机器模式定时器中断
#define IRQ_M_EXT     11 // 机器模式外部中断

// // exceptions
// #define CAUSE_MISALIGNED_FETCH 0x0     // Instruction address misaligned
// #define CAUSE_FETCH_ACCESS 0x1         // Instruction access fault
// #define CAUSE_ILLEGAL_INSTRUCTION 0x2  // Illegal Instruction
// #define CAUSE_BREAKPOINT 0x3           // Breakpoint
// #define CAUSE_MISALIGNED_LOAD 0x4      // Load address misaligned
// #define CAUSE_LOAD_ACCESS 0x5          // Load access fault
// #define CAUSE_MISALIGNED_STORE 0x6     // Store/AMO address misaligned
// #define CAUSE_STORE_ACCESS 0x7         // Store/AMO access fault
// #define CAUSE_USER_ECALL 0x8           // Environment call from U-mode
// #define CAUSE_SUPERVISOR_ECALL 0x9     // Environment call from S-mode
// #define CAUSE_MACHINE_ECALL 0xb        // Environment call from M-mode
// #define CAUSE_FETCH_PAGE_FAULT 0xc     // Instruction page fault
// #define CAUSE_LOAD_PAGE_FAULT 0xd      // Load page fault
// #define CAUSE_STORE_PAGE_FAULT 0xf     // Store/AMO page fault

// added @lab1_3
static void handle_timer() {
  int cpuid = read_csr(mhartid);
  // setup the timer fired at next time (TIMER_INTERVAL from now)
  *(uint64 *)CLINT_MTIMECMP(cpuid) = *(uint64 *)CLINT_MTIMECMP(cpuid) + TIMER_INTERVAL;

  // setup a soft interrupt in sip (S-mode Interrupt Pending) to be handled in S-mode
  write_csr(sip, SIP_SSIP);
}

//
// handle_mtrap calls a handling function according to the type of a machine mode interrupt (trap).
//
void handle_mtrap()
{
    uint64 mcause = read_csr(mcause);
    uint64 mtval = read_csr(mtval);
    uint64 mepc = read_csr(mepc);
    
    // 获取中断/异常的类型
    int cause_code = mcause & 0xff;
    int is_interrupt = (mcause >> 63) & 1;
    
    // 如果是中断
    if (is_interrupt) {
        switch (cause_code) {
            case IRQ_M_TIMER:
                // 处理机器模式定时器中断
                handle_timer();
                break;
            case IRQ_M_SOFT:
                // 处理机器模式软件中断
                sprint("Machine software interrupt\n");
                // 可以添加适当的处理代码
                break;
            case IRQ_M_EXT:
                // 处理机器模式外部中断
                sprint("Machine external interrupt\n");
                // 可以添加适当的处理代码
                break;
            default:
                // 未知中断
                sprint("Unknown machine interrupt: mcause %p\n", mcause);
                panic("Unhandled machine interrupt");
        }
    } else {
        // 如果是异常
        switch (cause_code) {
            case CAUSE_ILLEGAL_INSTRUCTION:
                sprint("Illegal instruction at 0x%lx, instruction: 0x%lx\n", mepc, mtval);
                panic("Illegal instruction exception in M-mode");
                break;
            case CAUSE_MISALIGNED_LOAD:
                sprint("Misaligned load at 0x%lx, address: 0x%lx\n", mepc, mtval);
                panic("Misaligned load exception in M-mode");
                break;
            case CAUSE_MISALIGNED_STORE:
                sprint("Misaligned store at 0x%lx, address: 0x%lx\n", mepc, mtval);
                panic("Misaligned store exception in M-mode");
                break;
            case CAUSE_MACHINE_ECALL:
                sprint("Machine mode ecall at 0x%lx\n", mepc);
                // 可以添加处理机器模式环境调用的代码
                // 通常需要增加mepc以跳过ecall指令
                write_csr(mepc, mepc + 4);
                break;
            // 可以根据需要添加更多异常类型的处理
            default:
                // 未知异常
                sprint("Unknown machine exception: mcause %p\n", mcause);
                sprint("sepc=%lx, mepc=%lx, mtval=%p\n", read_csr(sepc), mepc, mtval);
                panic("Unexpected exception in M-mode");
        }
    }
}