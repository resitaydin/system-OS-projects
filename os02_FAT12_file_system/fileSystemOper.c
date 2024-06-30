#include "FAT12.h"
#include <time.h>

#define MAX_FS_SIZE (NUMBER_OF_BLOCKS * BYTES_PER_SECTOR)

void read_file_system_from_file(FileSystem *fs, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    fread(fs, sizeof(FileSystem), 1, file);
    fclose(file);
}

void write_file_system_to_file(FileSystem *fs, const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to create file");
        exit(EXIT_FAILURE);
    }

    fwrite(fs, sizeof(FileSystem), 1, file);
    fclose(file);
}

void print_permissions(uint8_t file_attributes) {
    printf((file_attributes & 0x01) ? "R" : "-");
    printf((file_attributes & 0x02) ? "W" : "-");
    printf(" ");
}

void print_date(uint8_t *date) {
    int year = ((date[1] >> 1) & 0x7F) + 1980;
    int month = ((date[1] & 0x01) << 3) | ((date[0] >> 5) & 0x07);
    int day = date[0] & 0x1F;
    printf("%04d-%02d-%02d ", year, month, day);
}

void print_time(uint8_t *time) {
    int hour = (time[1] >> 3) & 0x1F;
    int minute = ((time[1] & 0x07) << 3) | ((time[0] >> 5) & 0x07);
    int second = (time[0] & 0x1F) * 2;
    printf("%02d:%02d:%02d ", hour, minute, second);
}

// Function to print file type
void print_file_type(uint8_t file_type) {
    printf((file_type == 0x01) ? "Dir" : "File");
}

DirectoryEntry* find_directory(FileSystem *fs, const char *path) {
    if (strcmp(path, "\\") == 0) {
        return fs->root_dir;
    }

    char *token;
    char *path_copy = strdup(path); // Make a copy of the path to avoid modifying the original
    token = strtok(path_copy, "\\");
    DirectoryEntry *current_dir = fs->root_dir;

    while (token != NULL) {
        bool found = false;
        for (int i = 0; i < ROOT_ENTRIES; i++) {
            if (strcmp((char *)current_dir[i].filename, token) == 0 && current_dir[i].file_attributes[0] == 0x10) {
                // Directory found
                current_dir = (DirectoryEntry *)((char *)fs->data + (current_dir[i].first_block_number[0] * BYTES_PER_SECTOR));
                found = true;
                break;
            }
        }
        if (!found) {
            // Directory not found
            free(path_copy);
            return NULL;
        }
        token = strtok(NULL, "\\");
    }

    free(path_copy);
    return current_dir;
}


void create_directory(FileSystem *fs, const char *path, const char *dirname) {
    DirectoryEntry *parent_dir;

    if (strcmp(path, "\\") == 0) {
        parent_dir = fs->root_dir;
    } else {
        parent_dir = find_directory(fs, path);
        if (parent_dir == NULL) {
            printf("Parent directory not found: %s\n", path);
            return;
        }
    }

    int empty_entry_index = -1;
    for (int i = 0; i < ROOT_ENTRIES; i++) {
        if (parent_dir[i].filename[0] == 0) {
            empty_entry_index = i;
            break;
        }
    }

    if (empty_entry_index != -1) {
        DirectoryEntry *new_entry = &parent_dir[empty_entry_index];
        strncpy((char *)new_entry->filename, dirname, MAX_FILENAME);
        new_entry->file_attributes[0] = 0x10;
        new_entry->file_type[0] = 0x01;

        // Find the first available block in the file system
        for (int i = 0; i < NUMBER_OF_BLOCKS; i++) {
            if (fs->fat[i].entry == 0x000) { // Found an empty block
                new_entry->first_block_number[0] = i;
                fs->fat[i].entry = 0xFFF; // Mark the block as occupied in FAT
                break;
            }
        }

        // Set the creation date and time
        time_t t = time(NULL);
        struct tm *tm_info = localtime(&t);

        // Encode date
        int year = tm_info->tm_year + 1900 - 1980;
        int month = tm_info->tm_mon + 1;
        int day = tm_info->tm_mday;
        new_entry->creation_date[0] = (day & 0x1F) | ((month & 0x07) << 5);
        new_entry->creation_date[1] = ((month & 0x08) >> 3) | ((year & 0x7F) << 1);

        // Encode time
        int hour = tm_info->tm_hour;
        int minute = tm_info->tm_min;
        int second = tm_info->tm_sec / 2;
        new_entry->creation_time[0] = (second & 0x1F) | ((minute & 0x07) << 5);
        new_entry->creation_time[1] = ((minute & 0x38) >> 3) | ((hour & 0x1F) << 3);

        printf("Directory created: %s\\%s\n", path, dirname);
    } else {
        printf("No space available to create directory within %s.\n", path);
    }
}

// Function to remove a directory
void remove_directory(FileSystem *fs, const char *path, const char *dirname) {
    DirectoryEntry *parent_dir = find_directory(fs, path);
    if (parent_dir == NULL) {
        printf("Parent directory not found: %s\n", path);
        return;
    }

    // Find the target directory within the parent directory
    DirectoryEntry *target_dir = NULL;
    for (int i = 0; i < ROOT_ENTRIES; i++) {
        DirectoryEntry *entry = &parent_dir[i];
        if (strncmp((char *)entry->filename, dirname, MAX_FILENAME) == 0 && entry->file_attributes[0] == 0x10) {
            target_dir = entry;
            break;
        }
    }

    // Check if the target directory was found
    if (target_dir != NULL) {
        // Check if the directory is empty
        bool is_empty = true;
        for (int j = 0; j < ROOT_ENTRIES; j++) {
            DirectoryEntry *entry = &fs->root_dir[j];
            if (entry->filename[0] != 0 && entry != target_dir &&
                entry->first_block_number[0] == target_dir - fs->root_dir) {
                is_empty = false;
                break;
            }
        }
        if (is_empty) {
            // Remove the directory
            memset(target_dir, 0, sizeof(DirectoryEntry));
            printf("Directory removed: %s/%s\n", path, dirname);
        } else {
            printf("Directory is not empty: %s/%s\n", path, dirname);
        }
    } else {
        printf("Directory not found: %s/%s\n", path, dirname);
    }
}


// Function to list directory contents
void list_directory(FileSystem *fs, const char *path) {
    DirectoryEntry *current_dir = find_directory(fs, path);
    if (current_dir == NULL) {
        printf("Directory not found: %s\n", path);
        return;
    }

    for (int i = 0; i < ROOT_ENTRIES; i++) {
        DirectoryEntry *entry = &current_dir[i];
        if (entry->filename[0] == 0) continue; // Empty entry

        print_file_type(entry->file_type[0]); // Print file type
        printf(" ");
        print_permissions(entry->file_attributes[0]);
        print_date(entry->creation_date);
        print_time(entry->creation_time);

        // Combine filename and extension correctly
        char filename[MAX_FILENAME + 1] = {0};
        char extension[MAX_EXTENSION + 1] = {0};
        strncpy(filename, (char *)entry->filename, MAX_FILENAME);
        strncpy(extension, (char *)entry->extension, MAX_EXTENSION);

        // Print filename and extension
        if(strlen(extension) > 0)
            printf("%s.%s ", filename, extension);
        else
            printf("%s ", filename);
            
        printf("%s\n", entry->password[0] ? "(protected)" : "");
    }
}

void dump_file_system_info(FileSystem *fs) { //  This just displays the subdir and files of root directory
    printf("File System Information:\n");
    printf("Block Count: %d\n", fs->super_block.number_of_blocks);
    printf("Block Size: %d bytes\n", fs->super_block.block_size);

    // Count occupied and free blocks
    int occupied_blocks = 0;
    for (int i = 0; i < NUMBER_OF_BLOCKS; i++) {
        if (fs->fat[i].entry != 0x000) {
            occupied_blocks++;
        }
    }
    int free_blocks = fs->super_block.number_of_blocks - occupied_blocks;
    printf("Occupied Blocks: %d\n", occupied_blocks);
    printf("Free Blocks: %d\n", free_blocks);

    // Count files and directories
    int files = 0;
    int directories = 0;
    for (int i = 0; i < ROOT_ENTRIES; i++) {
        if (fs->root_dir[i].filename[0] != 0) {
            if (fs->root_dir[i].file_attributes[0] == 0x10) {
                directories++;
            } else {
                files++;
            }
        }
    }
    printf("Number of Files: %d\n", files);
    printf("Number of Directories: %d\n", directories);

    // List occupied blocks with their corresponding file names
    printf("\nOccupied Blocks Information:\n");
    for (int i = 0; i < ROOT_ENTRIES; i++) {
        if (fs->root_dir[i].filename[0] != 0) {
            int block_number = fs->root_dir[i].first_block_number[0];
            printf("Block %d: %s.%s\n", block_number, fs->root_dir[i].filename, fs->root_dir[i].extension);
        }
    }
}

void write_file_to_fs(FileSystem *fs, const char *path, const char *filename) {
    // fileSystemOper fileSystem.data write /ysa/file linuxFile
    /*
    Creates a file named file under “/usr/ysa” in the
    file system, then copies the contents of the Linux file
    into the new file.
    */
}

void read_file_from_fs(FileSystem *fs, const char *path, const char *filename) {
    // fileSystemOper fileSystem.data read /ysa/file linuxFile
    /*
    Copies the contents of the file named file under
    “/usr/ysa” in the file system into a new Linux file
    named linuxFile.
    */
}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <file_system.data> <operation> [parameters]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *filename = argv[1]; // Use argv[2] as filename
    const char *operation = argv[2]; // Adjust indices to account for the change

    // Replace '/' with '\\' in paths
    for (int i = 3; i < argc; ++i) {
        for (char *c = argv[i]; *c != '\0'; ++c) {
            if (*c == '/') {
                *c = '\\';
            }
        }
    }

    FileSystem fs;
    read_file_system_from_file(&fs, filename);

    if (strcmp(operation, "dir") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: %s %s <path>\n", argv[0], operation);
            return EXIT_FAILURE;
        }
        const char *path = argv[3]; // Adjust index for path
        list_directory(&fs, path);
    } else if (strcmp(operation, "mkdir") == 0) {
        if (argc != 5) {
            fprintf(stderr, "Usage: %s %s <path> <dirname>\n", argv[0], operation);
            return EXIT_FAILURE;
        }
        const char *path = argv[3]; // Adjust index for path
        const char *dirname = argv[4]; // Adjust index for dirname
        create_directory(&fs, path, dirname);
        write_file_system_to_file(&fs, filename); // Save changes
    } else if (strcmp(operation, "rmdir") == 0) {
        if (argc != 5) {
            fprintf(stderr, "Usage: %s %s <path> <dirname>\n", argv[0], operation);
            return EXIT_FAILURE;
        }
        const char *path = argv[3]; // Adjust index for path
        const char *dirname = argv[4]; // Adjust index for dirname
        remove_directory(&fs, path, dirname);
        write_file_system_to_file(&fs, filename); // Save changes
    }
    else if (strcmp(operation, "dumpe2fs") == 0) {
        // Dump file system information operation
        dump_file_system_info(&fs);
    }
    else if (strcmp(argv[2], "write") == 0) {
        // write function
    }
    else {
        fprintf(stderr, "Unknown operation: %s\n", operation);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
