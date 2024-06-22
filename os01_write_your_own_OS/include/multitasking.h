 
#ifndef __MYOS__MULTITASKING_H
#define __MYOS__MULTITASKING_H

#include <common/types.h>
#include <gdt.h>

namespace myos
{
    
    struct CPUState
    {
        common::uint32_t eax;
        common::uint32_t ebx;
        common::uint32_t ecx;
        common::uint32_t edx;

        common::uint32_t esi;
        common::uint32_t edi;
        common::uint32_t ebp;

        common::uint32_t error;

        common::uint32_t eip;
        common::uint32_t cs;
        common::uint32_t eflags;
        common::uint32_t esp;
        common::uint32_t ss;

        /*
        common::uint32_t gs;
        common::uint32_t fs;
        common::uint32_t es;
        common::uint32_t ds;
        */   
    } __attribute__((packed));
    
    enum TaskState{
        READY,
        WAITING,
        FINISHED
    };
    
    class Task
    {
    friend class TaskManager;
    private:
        common::uint8_t stack[4096]; // 4 KiB
        CPUState* cpustate;
        TaskState taskState;
        myos::common::uint32_t pid;
        myos::common::uint32_t ppid;
        GlobalDescriptorTable* gdt;
        myos::common::uint32_t waitPid;
    public:
        Task(GlobalDescriptorTable *gdt, void entrypoint());
        ~Task();
    };
    
    
    class TaskManager
    {
    private:
        Task* tasks[256];
        int numTasks;
        int currentTask;
    public:
        TaskManager();
        ~TaskManager();
        static TaskManager* currentTaskManager;
        bool AddTask(Task* task);
        bool exitTask();
        common::uint32_t getCurrentTaskPid();
        common::uint32_t forkTask(CPUState* cpustate);
        common::uint32_t execveTask(void entrypoint());
        bool waitpidTask(common::uint32_t esp);
        CPUState* Schedule(CPUState* cpustate);
        void printProcessTable();
        common::uint32_t getTaskIndex(common::uint32_t pid);
    };
    
}

#endif