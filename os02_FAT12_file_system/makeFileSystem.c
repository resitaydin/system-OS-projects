// makeFileSystem.c
#include "FAT12.h"

#define MAX_FS_SIZE (NUMBER_OF_BLOCKS * BYTES_PER_SECTOR)

void initialize_file_system(FileSystem *fs, int block_size) {
    fs->super_block.block_size = block_size;
    fs->super_block.number_of_blocks = NUMBER_OF_BLOCKS;
    fs->super_block.root_dir_pos = 0;
    fs->super_block.fat_pos = fs->super_block.root_dir_pos + sizeof(fs->root_dir) / BYTES_PER_SECTOR;
    fs->super_block.data_pos = fs->super_block.fat_pos + sizeof(fs->fat) / BYTES_PER_SECTOR;

    memset(fs->fat, 0, sizeof(fs->fat));
    memset(fs->root_dir, 0, sizeof(fs->root_dir));
    memset(fs->data, 0, sizeof(fs->data));

    for (int i = 0; i < NUMBER_OF_BLOCKS; i++) {
        fs->fat[i].entry = 0x000; // All clusters are free
    }

   // Example directory entries
    // DirectoryEntry *entry1 = &fs->root_dir[0];
    // strcpy((char *)entry1->filename, "FILE1");
    // strcpy((char *)entry1->extension, "TXT");
    // entry1->file_attributes[0] = 0x01; // Read attribute
    // entry1->creation_date[0] = 0x21; entry1->creation_date[1] = 0x98;
    // entry1->creation_time[0] = 0x00; entry1->creation_time[1] = 0x00;

    // DirectoryEntry *entry2 = &fs->root_dir[1];
    // strcpy((char *)entry2->filename, "FILE2");
    // strcpy((char *)entry2->extension, "TXT");
    // entry2->file_attributes[0] = 0x02; // Write attribute
    // entry2->creation_date[0] = 0x21; entry2->creation_date[1] = 0x98;
    // entry2->creation_time[0] = 0x00; entry2->creation_time[1] = 0x00;

    // DirectoryEntry *entry3 = &fs->root_dir[2];
    // strcpy((char *)entry3->filename, "SECRET");
    // strcpy((char *)entry3->extension, "TXT");
    // entry3->file_attributes[0] = 0x03; // Read and Write attributes
    // strcpy((char *)entry3->password, "123456");
    // entry3->creation_date[0] = 0x21; entry3->creation_date[1] = 0x98;
    // entry3->creation_time[0] = 0x00; entry3->creation_time[1] = 0x00;

}

void write_file_system_to_file(FileSystem *fs, const char *filename, int block_size_bytes) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to create file");
        exit(EXIT_FAILURE);
    }

    fwrite(fs, sizeof(FileSystem), 1, file);

    // Calculate the required size based on the block size
    long required_size = 0;
    if (block_size_bytes == 512) { // For block size 0.5 KB
        required_size = 2 * 1024 * 1024; // 2MB
    } else if (block_size_bytes == 1024) { // For block size 1 KB
        required_size = 4 * 1024 * 1024; // 4MB
    } else {
        fprintf(stderr, "Invalid block size. Must be either 0.5 or 1.\n");
        exit(EXIT_FAILURE);
    }

    // Pad the file to the required size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    if (file_size < required_size) {
        char *buffer = (char*) calloc(1, required_size - file_size);
        fwrite(buffer, 1, required_size - file_size, file);
        free(buffer);
    }

    fclose(file);
}



int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <block_size_in_KB> <output_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    double block_size = atof(argv[1]); // Convert argument to double
    if (block_size != 0.5 && block_size != 1.0) {
        fprintf(stderr, "Invalid block size. Must be either 0.5 or 1.\n");
        return EXIT_FAILURE;
    }

    int block_size_bytes = (int)(block_size * 1024); // Convert KB to bytes

    const char *filename = argv[2];

    FileSystem fs;
    initialize_file_system(&fs, block_size_bytes);
    write_file_system_to_file(&fs, filename, block_size_bytes); // Pass block_size_bytes
    printf("File system created successfully in %s\n", filename);
    return EXIT_SUCCESS;
}

