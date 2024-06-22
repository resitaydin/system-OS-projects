
#include <multitasking.h>
#include <memorymanagement.h>

using namespace myos;
using namespace myos::common;

TaskManager* TaskManager::currentTaskManager = 0;
static uint32_t processCounter = 0;


Task::Task(GlobalDescriptorTable *gdt, void entrypoint())
: pid(processCounter++), ppid(-1)
{

    this->gdt = gdt;
    this->cpustate = nullptr;

    if (entrypoint != nullptr) {

        this->cpustate = (CPUState*)(&stack[4096] - sizeof(CPUState));
        this->cpustate->eip = (uint32_t)entrypoint;
    }

    // set the state of the task to ready
    taskState = READY;

    waitPid = -1;
    
    cpustate -> eax = 0;
    cpustate -> ebx = 0;
    cpustate -> ecx = 0;
    cpustate -> edx = 0;

    cpustate -> esi = 0;
    cpustate -> edi = 0;
    cpustate -> ebp = 0;
   
    cpustate -> eip = (uint32_t)entrypoint;
    cpustate -> cs = gdt->CodeSegmentSelector();

    cpustate -> eflags = 0x202;

     /*
    cpustate -> gs = 0;
    cpustate -> fs = 0;
    cpustate -> es = 0;
    cpustate -> ds = 0;
    cpustate -> error = 0;    
    */
}

Task::~Task()
{
}

        
TaskManager::TaskManager()
{
    currentTaskManager = this;
    numTasks = 0;
    currentTask = -1;
}

TaskManager::~TaskManager()
{
}

uint32_t TaskManager::getCurrentTaskPid() // getPID
{
    return tasks[currentTask]->pid;
}

void printf(char*);
void printfInt(int);
void printfHex(uint8_t);

bool TaskManager::exitTask(){
    // set task state to finished
    tasks[currentTask]->taskState = FINISHED;
    printProcessTable();
    return true;
}

uint32_t TaskManager::forkTask(CPUState* cpustate){ // fork
    if(numTasks >= 256)
        return 0;

    // Allocate and initialize the new task

    void (*entrypoint)() = (void (*)()) cpustate->eip;
    GlobalDescriptorTable* gdt = tasks[currentTask]->gdt;

    if(!gdt || !entrypoint)
        return -1;

    tasks[numTasks] = new Task(gdt, entrypoint);

    if (!tasks[numTasks]) {
        printf("Failed to allocate new task\n");
        return -1; // Failed to create a task
    }

    // set task state to ready
    tasks[numTasks]->taskState = READY;
    tasks[numTasks]->ppid = tasks[currentTask]->pid;
    //tasks[numTasks]->pid = processCounter++;

    // copy the stack
    for(int i = 0; i < sizeof(tasks[currentTask]->stack); i++)
        tasks[numTasks]->stack[i] = tasks[currentTask]->stack[i];

    // Calculate the offset
    uint32_t offset = (uint32_t) cpustate - (uint32_t) tasks[currentTask]->stack;
    tasks[numTasks]->cpustate = (CPUState*) ( (uint32_t) tasks[numTasks]->stack + offset);


    // Adjust the stack pointer (esp) for the child task
    tasks[numTasks]->cpustate->esp = (uint32_t)tasks[numTasks]->stack + (cpustate->esp - (uint32_t)tasks[currentTask]->stack);

    tasks[numTasks]->cpustate->ecx = 0; // return value of fork

    numTasks++;
    return tasks[numTasks - 1]->pid;
}

common::uint32_t TaskManager::execveTask(void entrypoint()){
    // Replace the calling process with a new program image
    // initializes new stack heap, data segments, etc.
    // and starts executing the new program

    // set task state to ready
    tasks[currentTask]->taskState = READY;

    // allocate new stack
    tasks[currentTask]->cpustate = (CPUState*) (tasks[currentTask]->stack + 4096 - sizeof(CPUState)); // 4 KiB stack

    // Initialize the CPUState fields
    tasks[currentTask]->cpustate->eax = 0;
    tasks[currentTask]->cpustate->ebx = 0;
    tasks[currentTask]->cpustate->ecx = tasks[currentTask]->pid;

    tasks[currentTask]->cpustate->esi = 0;
    tasks[currentTask]->cpustate->edi = 0;
    tasks[currentTask]->cpustate->ebp = 0;
    tasks[currentTask]->cpustate->error = 0;

    tasks[currentTask]->cpustate->eip = (uint32_t) entrypoint;
    tasks[currentTask]->cpustate->cs = tasks[currentTask]->gdt->CodeSegmentSelector();

    tasks[currentTask]->cpustate->esp = (uint32_t)tasks[currentTask]->stack + 4096; // Initialize stack pointer
    
    tasks[currentTask]->cpustate->eflags = 0x202; // Interrupts enabled

    // return cpustate
    return (uint32_t) tasks[currentTask]->cpustate;
}

bool TaskManager::waitpidTask(common::uint32_t esp) {
    CPUState* cpu = (CPUState*)esp;
    uint32_t pid = cpu->ebx;

    // prevent waiting itself or init
    if (pid == tasks[currentTask]->pid || pid == 0)
        return false;

    uint32_t childIndex = getTaskIndex(pid);

    if (childIndex == -1) {
        printf("Child process not found\n");
        return false;
    }

    // check if the child process has already finished
    if(tasks[childIndex]->taskState == FINISHED)
        return false;

    // set the task state to waiting
    tasks[currentTask]->taskState = WAITING;
    tasks[currentTask]->waitPid = pid;
    tasks[currentTask]->cpustate = cpu;
    
    return true;
}


bool TaskManager::AddTask(Task* task)
{
    if(numTasks >= 256)
        return false;

    tasks[numTasks]->taskState = READY;
    tasks[numTasks]->pid = task->pid;
    tasks[numTasks]->ppid = task->ppid;

    tasks[numTasks]->cpustate = (CPUState *) (tasks[numTasks]->stack + 4096 - sizeof(CPUState));

    tasks[numTasks]->cpustate->eax = task->cpustate->eax;
    tasks[numTasks]->cpustate->ebx = task->cpustate->ebx;
    tasks[numTasks]->cpustate->ecx = task->cpustate->ecx;
    tasks[numTasks]->cpustate->edx = task->cpustate->edx;

    tasks[numTasks]->cpustate->esi = task->cpustate->esi;
    tasks[numTasks]->cpustate->edi = task->cpustate->edi;
    tasks[numTasks]->cpustate->ebp = task->cpustate->ebp;

    tasks[numTasks]->cpustate->eip = task->cpustate->eip;
    tasks[numTasks]->cpustate->cs = task->cpustate->cs;

    tasks[numTasks]->cpustate->eflags = task->cpustate->eflags;

    numTasks++;
    return true;
}

CPUState* TaskManager::Schedule(CPUState* cpustate) {
    if (numTasks <= 0)
        return cpustate;

    // Save the current CPU state if there's a current task
    if (currentTask >= 0)
        tasks[currentTask]->cpustate = cpustate;

    //printProcessTable();

    // Round-robin scheduling
    int nextTask = (currentTask + 1) % numTasks;
    for (int i = 0; i < numTasks; i++) {
        // Handle tasks that are waiting for a child process
        if (tasks[nextTask]->taskState == WAITING) {
            uint32_t childIndex = getTaskIndex(tasks[nextTask]->waitPid);

            if (childIndex != -1 && tasks[childIndex]->taskState == FINISHED) {
                // If the child process has finished, unblock the current task
                tasks[nextTask]->taskState = READY;
                tasks[nextTask]->waitPid = 0;
            }
        }

        // Check if the next task is READY to run
        if (tasks[nextTask]->taskState == READY) {
            currentTask = nextTask;
            return tasks[nextTask]->cpustate;
        }

        // Move to the next task
        nextTask = (nextTask + 1) % numTasks;
    }

    // If no task is ready to run, return the current CPU state
    return cpustate;
}


void TaskManager::printProcessTable() {
    printf("##########################\n");
    printf("PID     PPID     State\n");
    for(int i = 0; i < numTasks; i++) {
        printfInt(tasks[i]->pid);
        printf("       ");
        printfInt(tasks[i]->ppid);
        printf("        ");

        switch(tasks[i]->taskState) {
            case READY:
                printf(" READY\n");
                break;
            case WAITING:
                printf("WAITING\n");
                break;
            case FINISHED:
                printf("FINISHED\n");
                break;
            default:
                printf("UNKNOWN\n");
        }
    }
    printf("##########################\n\n");

}

uint32_t TaskManager::getTaskIndex(uint32_t pid) {
    for (int i = 0; i < numTasks; i++) {
        if (tasks[i]->pid == pid) {
            return i;
        }
    }
    return -1;
}
