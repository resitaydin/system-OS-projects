
#include <common/types.h>
#include <gdt.h>
#include <memorymanagement.h>
#include <hardwarecommunication/interrupts.h>
#include <syscalls.h>
#include <hardwarecommunication/pci.h>
#include <drivers/driver.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/vga.h>
#include <drivers/ata.h>
#include <gui/desktop.h>
#include <gui/window.h>
#include <multitasking.h>

#include <drivers/amd_am79c973.h>

// #define GRAPHICSMODE


using namespace myos;
using namespace myos::common;
using namespace myos::drivers;
using namespace myos::hardwarecommunication;
using namespace myos::gui;

TaskManager taskManager;


void printf(char *str) // slidable printf
{
    static uint16_t *VideoMemory = (uint16_t *)0xb8000;

    static uint8_t x = 0, y = 0;

    for (int i = 0; str[i] != '\0'; ++i)
    {
        switch (str[i])
        {
        case '\n':
            x = 0;
            y++;
            break;
        default:
            VideoMemory[80 * y + x] = (VideoMemory[80 * y + x] & 0xFF00) | str[i];
            x++;
            break;
        }

        if (x >= 80)
        {
            x = 0;
            y++;
        }

        if (y >= 25)
        {
            // Slide all input one line up
            for (int j = 0; j < 24; j++)
            {
                for (int k = 0; k < 80; k++)
                {
                    VideoMemory[80 * j + k] = VideoMemory[80 * (j + 1) + k];
                }
            }

            // Clear the last line
            for (int k = 0; k < 80; k++)
            {
                VideoMemory[80 * 24 + k] = (VideoMemory[80 * 24 + k] & 0xFF00) | ' ';
            }

            x = 0;
            y = 24;
        }
    }
}

// Function to print integers
void printfInt(int num)
{
    // Buffer to hold the string representation of the integer
    char str[12]; 

    // Convert integer to string (base 10)
    int i = 0;
    bool isNegative = false;

    // Handle 0 explicitly
    if (num == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
    }
    else
    {
        // Handle negative numbers for base 10
        if (num < 0)
        {
            isNegative = true;
            num = -num;
        }

        // Process individual digits
        while (num != 0)
        {
            int rem = num % 10;
            str[i++] = (rem > 9) ? (rem - 10) + 'A' : rem + '0';
            num = num / 10;
        }

        // If the number is negative, append '-'
        if (isNegative)
            str[i++] = '-';

        str[i] = '\0'; // Append string terminator

        // Reverse the string
        for (int start = 0, end = i - 1; start < end; start++, end--)
        {
            char temp = str[start];
            str[start] = str[end];
            str[end] = temp;
        }
    }

    // Print the string
    printf(str);
}

void printfHex(uint8_t key)
{
    char* foo = "00";
    char* hex = "0123456789ABCDEF";
    foo[0] = hex[(key >> 4) & 0xF];
    foo[1] = hex[key & 0xF];
    printf(foo);
}
void printfHex16(uint16_t key)
{
    printfHex((key >> 8) & 0xFF);
    printfHex( key & 0xFF);
}
void printfHex32(uint32_t key)
{
    printfHex((key >> 24) & 0xFF);
    printfHex((key >> 16) & 0xFF);
    printfHex((key >> 8) & 0xFF);
    printfHex( key & 0xFF);
}



class PrintfKeyboardEventHandler : public KeyboardEventHandler
{
public:
    void OnKeyDown(char c)
    {
        char* foo = " ";
        foo[0] = c;
        printf(foo);
    }
};

class MouseToConsole : public MouseEventHandler
{
    int8_t x, y;
public:
    
    MouseToConsole()
    {
        uint16_t* VideoMemory = (uint16_t*)0xb8000;
        x = 40;
        y = 12;
        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);        
    }
    
    virtual void OnMouseMove(int xoffset, int yoffset)
    {
        static uint16_t* VideoMemory = (uint16_t*)0xb8000;
        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);

        x += xoffset;
        if(x >= 80) x = 79;
        if(x < 0) x = 0;
        y += yoffset;
        if(y >= 25) y = 24;
        if(y < 0) y = 0;

        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);
    }
    
};

void sleep(int seconds){
    for(int i=0; i<seconds*100000000; i++){

    }
}

void testFork() {

    int childPid = 0;
    fork(&childPid);

    if(childPid == -1){
        printf("Fork failed...\n");
        return;
    }
    if(childPid == 0) {
        printf("Child PID: ");
        printfHex(getpid());
        printf("\n");
    }
    else{
        // printf("Parent PID ");
        // printfHex(getpid());
        // printf("\n");
    }
}

void taskFork()
{
    while(1){
        testFork();
        printf("...Delay...\n");
        sleep(10);
    }
}

void testForkAndWaitpid() {
    int childPid = 0;
    fork(&childPid);

    printf("WaitPid Test function...");

    if(childPid == -1){
        printf("Fork failed...\n");
        return;
    }
    if(childPid == 0) {
        // This is the child process
        printf("Child process is running...\n");
        sleep(5);
        printf("Child process is finished...\n");
        exit();
        
    } else {
        // This is the parent process
        printf("Parent process is waiting for child to finish...\n");
        waitpid(childPid);
        printf("Parent process continues...\n");
    }
    
    while(1);
}

void testExec(){
    printf("Exec test is running...\n");
    printf("PID: ");
    printfHex(getpid());
    printf("\n");
    printf("Exec test is finished...\n");
    exit();
}

void testForkExecWaitpid(){
    int childPid = 0;
    fork(&childPid);

    if(childPid == -1){
        printf("Fork failed...\n");
        return;
    }
    if(childPid == 0) {
        // This is the child process
        printf("Child process is running...\n");
        execve(testExec);
    } else {
        // This is the parent process
        printf("Parent process is waiting for child to finish...\n");
        waitpid(childPid);
        printf("Parent process continues...\n");
    }
}

void collatz() {
    for(int i = 1; i < 15; i++) {
        int n = i;
        printfInt(n);
        printf("- Sequence: ");

        while (n != 1) {
            if (n % 2 == 0) {
                n = n / 2;
            } else {
                n = 3 * n + 1;
            }
            printfInt(n);
            printf(", ");
        }

        printf("\n");
    }

    printf("******* Collatz program is finished *******\n");
    exit();
}

void long_running_program(int n){
    uint32_t result = 0;
    for(int i=0; i<n; i++){
        for(int j=0 ; j<n; j++){
            result += i*j;
        }
    }
    printf("Result: ");
    printfInt(result);
    printf("\n");
    printf("******* Long running program is finished *******\n\n");
    exit();
}

void init() {

    for(int i = 0; i<3; ++i){
        int childPid1 = 0;

        fork(&childPid1);

        if (childPid1 == -1) {
            printf("Fork failed...\n");
            return;
        } else if (childPid1 == 0) {
            execve(collatz);
            sleep(10);
            printf("execve failed...\n"); // Only printed if execve fails
            exit(); // Ensure exit if execve fails
        } 

        printf("Parent process is waiting for children to finish...\n");
        waitpid(childPid1);
        
        int childPid2 = 0;
        fork(&childPid2);

        if (childPid2 == -1) {
            printf("Fork failed...\n");
            return;
        } else if (childPid2 == 0) {
            // This is the second child process (long running program)
            printf("******* Long running program is running *******\n");
            long_running_program(1000);
            // Exit is handled within long_running_program
        } 
        else{
            waitpid(childPid2);
        }
        printf("Init process continues...\n");
    }

    
    while (1) {
        sleep(50);
        //printf("Kernel is running...\n");
        // Kernel LOOP
    }
}


typedef void (*constructor)();
extern "C" constructor start_ctors;
extern "C" constructor end_ctors;
extern "C" void callConstructors()
{
    for(constructor* i = &start_ctors; i != &end_ctors; i++)
        (*i)();
}



extern "C" void kernelMain(const void* multiboot_structure, uint32_t /*multiboot_magic*/)
{
    printf("Welcome to aydOS - Aydin Operating System! \n");

    GlobalDescriptorTable gdt;
    
    uint32_t* memupper = (uint32_t*)(((size_t)multiboot_structure) + 8);
    size_t heap = 10*1024*1024;
    MemoryManager memoryManager(heap, (*memupper)*1024 - heap - 10*1024);
    
    // printf("heap: 0x");
    // printfHex((heap >> 24) & 0xFF);
    // printfHex((heap >> 16) & 0xFF);
    // printfHex((heap >> 8 ) & 0xFF);
    // printfHex((heap      ) & 0xFF);
    
    void* allocated = memoryManager.malloc(1024);
    // printf("\nallocated: 0x");
    // printfHex(((size_t)allocated >> 24) & 0xFF);
    // printfHex(((size_t)allocated >> 16) & 0xFF);
    // printfHex(((size_t)allocated >> 8 ) & 0xFF);
    // printfHex(((size_t)allocated      ) & 0xFF);
    // printf("\n");
    
    Task init_task(&gdt, init);

    taskManager.AddTask(&init_task);
    
    InterruptManager interrupts(0x20, &gdt, &taskManager);
    SyscallHandler syscalls(&interrupts, 0x80);
    

    interrupts.Activate();

    printf("Interrupts activated...\n");

    while(1)
    {
        #ifdef GRAPHICSMODE
            desktop.Draw(&vga);
        #endif
    }
}

 // printf("Initializing Hardware, Stage 1\n");
    
    // #ifdef GRAPHICSMODE
    //     Desktop desktop(320,200, 0x00,0x00,0xA8);
    // #endif
    
    // DriverManager drvManager;
    
    //     #ifdef GRAPHICSMODE
    //         KeyboardDriver keyboard(&interrupts, &desktop);
    //     #else
    //         PrintfKeyboardEventHandler kbhandler;
    //         KeyboardDriver keyboard(&interrupts, &kbhandler);
    //     #endif
    //     drvManager.AddDriver(&keyboard);
        
    
    //     #ifdef GRAPHICSMODE
    //         MouseDriver mouse(&interrupts, &desktop);
    //     #else
    //         MouseToConsole mousehandler;
    //         MouseDriver mouse(&interrupts, &mousehandler);
    //     #endif
    //     drvManager.AddDriver(&mouse);
        
    //     PeripheralComponentInterconnectController PCIController;
    //     PCIController.SelectDrivers(&drvManager, &interrupts);

    //     #ifdef GRAPHICSMODE
    //         VideoGraphicsArray vga;
    //     #endif
        
    // printf("Initializing Hardware, Stage 2\n");
    //     drvManager.ActivateAll();
        
    // printf("Initializing Hardware, Stage 3\n");

    // #ifdef GRAPHICSMODE
    //     vga.SetMode(320,200,8);
    //     Window win1(&desktop, 10,10,20,20, 0xA8,0x00,0x00);
    //     desktop.AddChild(&win1);
    //     Window win2(&desktop, 40,15,30,30, 0x00,0xA8,0x00);
    //     desktop.AddChild(&win2);
    // #endif


    /*
    printf("\nS-ATA primary master: ");
    AdvancedTechnologyAttachment ata0m(true, 0x1F0);
    ata0m.Identify();
    
    printf("\nS-ATA primary slave: ");
    AdvancedTechnologyAttachment ata0s(false, 0x1F0);
    ata0s.Identify();
    ata0s.Write28(0, (uint8_t*)"http://www.AlgorithMan.de", 25);
    ata0s.Flush();
    ata0s.Read28(0, 25);
    
    printf("\nS-ATA secondary master: ");
    AdvancedTechnologyAttachment ata1m(true, 0x170);
    ata1m.Identify();
    
    printf("\nS-ATA secondary slave: ");
    AdvancedTechnologyAttachment ata1s(false, 0x170);
    ata1s.Identify();
    // third: 0x1E8
    // fourth: 0x168
    */
    
    
    // amd_am79c973* eth0 = (amd_am79c973*)(drvManager.drivers[2]);
    // eth0->Send((uint8_t*)"Hello Network", 13);
