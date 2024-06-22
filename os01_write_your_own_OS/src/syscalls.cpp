
#include <syscalls.h>
 
using namespace myos;
using namespace myos::common;
using namespace myos::hardwarecommunication;
 
SyscallHandler::SyscallHandler(InterruptManager* interruptManager, uint8_t InterruptNumber)
:    InterruptHandler(interruptManager, InterruptNumber  + interruptManager->HardwareInterruptOffset())
{
}

SyscallHandler::~SyscallHandler()
{
}


uint32_t myos::getpid(){
    int result = 0;
    asm("int $0x80" : "=c" (result) : "a" (20));
    return result;
}

void myos::fork(int* pid){
    asm("int $0x80" : "=c" (*pid) : "a" (2));
}

void myos::exit(){
    asm("int $0x80" : : "a" (1));
}

uint32_t myos::execve(void entrypoint()){
    uint32_t result;
    asm("int $0x80" : "=c" (result) : "a" (11), "b" ( (uint32_t) entrypoint));
    return result;
}

void myos::waitpid(common::uint32_t pid_t){
    asm("int $0x80" : : "a" (7), "b" (pid_t));
}

uint32_t SyscallHandler::HandleInterrupt(uint32_t esp)
{
    CPUState* cpu = (CPUState*)esp;
    

    switch(cpu->eax)
    {
        case 1: // exit
            if(InterruptHandler::sys_exit())
                return InterruptHandler::HandleInterrupt(esp);
            break;

        case 7: // waitpid
            if( InterruptHandler::sys_waitpid(esp) ){
                return InterruptHandler::HandleInterrupt(esp);
            }
            break;

        case 11: // execve
            esp = InterruptHandler::sys_execve(cpu->ebx);
            break;

        case 20:    // getPID
            cpu->ecx = InterruptHandler::sys_getpid();
            break;

        case 2:    // fork
            cpu->ecx = InterruptHandler::sys_fork(cpu);
            return InterruptHandler::HandleInterrupt(esp);
            break;
            
        default:
            break;
    }

    return esp;
}

// void sysprintf(char* str)
// {
//     asm("int $0x80" : : "a" (4), "b" (str));
// }

// case 4: // write
    //         printf((char*)cpu->ebx);
    //         break;