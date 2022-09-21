#include <iostream>
#include <cstdint>
#include "disk.h"
#include <vector>
#include <cstring>
#include <vector>
using std::vector;
using std::string;
#include <typeinfo>

#ifndef __FS_H__
#define __FS_H__

#define ROOT_DIR 2
#define ROOT_BLOCK 0
#define FAT_BLOCK 1
#define FAT_FREE 0
#define FAT_EOF -1


#define ROOT_SIZE 64 // The root block can contain 64 data entries.

#define TYPE_FILE 0
#define TYPE_DIR 1
#define READ 0x04
#define WRITE 0x02
#define EXECUTE 0x01

struct dir_entry {
    char file_name[56]; // name of the file / sub-directory
    uint32_t size; // size of the file in bytes // ALSO STORES PARENT INDEX IF FILE TYPE IS FOLDER
    uint16_t first_blk; // index in the FAT for the first block of the file
    uint8_t type; // directory (1) or file (0)
    uint8_t access_rights; // read (0x04), write (0x02), execute (0x01)
};

class FS {
private:
    Disk disk;
    // size of a FAT entry is 2 bytes
    int16_t fat[BLOCK_SIZE/2];
    // std::vector<dir_entry> dir_entries;

    std::vector<dir_entry> all_children; //Children to current dir
    std::vector<string> previous_dirs; //All previously visited directories
    int current_dir_index; //Current directory
    int reset_index;
    int emptycheck_reset_index;

    //*"*"*"*"*"*"*"*"* HELP FUNCTIONS STARTs HERE *"*"*"*"*"*"*"*"*
    void clean_fat(void);

    int file_size_check(string file_to_evaluate);

    int file_fit_check(int num_blocks);

    int traverse_filepath(string filepath, int from_cd = 1, int from_emptycheck = 0);

    void reset_filepath(int from_emptycheck = 0);

    void create_new_folder(string filepath);

    void find_children(void);

    dir_entry create_dir_entry(int size_of_file, int start_block, int type, string filepath, int access_rights = READ + WRITE);

    int check_number_of_blocks(int size_of_file);
    
    int find_space_and_write_to_disk(int num_blocks, uint8_t* block, string string_to_eval);

    int write_dir_entry_to_disk(dir_entry entry, int from_mkdir = 0);

    string get_file_name(string filepath);

    void setup_root_dir(void);

    void load_dir_content(void);

    int find_file_in_current_dir(string file_name);

    int find_space_and_write_folder_to_disk(int num_blocks, string folder_name);
    
    void update_parent_directory();

    std::vector<string> path_to_vector(string filepath);

    void remove_file_from_fat(int dir_entry_index);

    int folder_empty_check(int dir_entry_index, string filepath);

    int check_access_rights(int dir_entry_index, int accessrights);
    
    //*"*"*"*"*"*"*"*"* HELP FUNCTIONS ENDS HERE *"*"*"*"*"*"*"*"*


public:

    FS();
    ~FS();
    // formats the disk, i.e., creates an empty file system
    int format();
    // create <filepath> creates a new file on the disk, the data content is
    // written on the following rows (ended with an empty row)
    int create(std::string filepath);
    // cat <filepath> reads the content of a file and prints it on the screen
    int cat(std::string filepath);
    // ls lists the content in the currect directory (files and sub-directories)
    int ls();

    // cp <sourcepath> <destpath> makes an exact copy of the file
    // <sourcepath> to a new file <destpath>
    int cp(std::string sourcepath, std::string destpath);
    // mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
    // or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
    int mv(std::string sourcepath, std::string destpath);
    // rm <filepath> removes / deletes the file <filepath>
    int rm(std::string filepath);
    // append <filepath1> <filepath2> appends the contents of file <filepath1> to
    // the end of file <filepath2>. The file <filepath1> is unchanged.
    int append(std::string filepath1, std::string filepath2);

    // mkdir <dirpath> creates a new sub-directory with the name <dirpath>
    // in the current directory
    int mkdir(std::string dirpath);
    // cd <dirpath> changes the current (working) directory to the directory named <dirpath>
    int cd(std::string dirpath);
    // pwd prints the full path, i.e., from the root directory, to the current
    // directory, including the currect directory name
    int pwd();

    // chmod <accessrights> <filepath> changes the access rights for the
    // file <filepath> to <accessrights>.
    int chmod(std::string accessrights, std::string filepath);
};

#endif // __FS_H__
