#include <iostream>
#include "fs.h"
#include <cstring>
#include <vector>
#include <typeinfo> // for debugging
#include <iomanip>
#include <istream>
#include <streambuf>
using namespace std;

//*"*"*"*"*""*"*"*"*"" GLOBAL VARIABLES START HERE *"*"*"*"*""*"*"*"*""
struct dir_entry dir_entries[ROOT_SIZE];
vector<int> dir_path;
int curr_dir_content[ROOT_SIZE];

struct membuf : std::streambuf
{
    membuf(char* begin, char* end)
    {
        this->setg(begin, begin, end);
    }
};

//*"*"*"*"*""*"*"*"*"" GLOBAL VARIABLES ENDS HERE *"*"*"*"*""*"*"*"*""

//*"*"*"*"*""*"*"*"*"" HELP FUNCTIONS STARTS HERE *"*"*"*"*""*"*"*"*""

int FS::file_size_check(string file_to_evaluate)
{
    int size_of_file = file_to_evaluate.length();
    
    int num_blocks = size_of_file / BLOCK_SIZE + 1;
    int num_blocks_temp = size_of_file / BLOCK_SIZE;

    if (size_of_file == BLOCK_SIZE*num_blocks_temp)
    {
        num_blocks = num_blocks_temp;
    }
    return num_blocks;
}

int FS::file_fit_check(int num_blocks)
{
    int found_blocks = 0;

    for (int i = 0; i < 64; i++)
    {
        if (fat[i] == 0)
        {
            found_blocks++;
        }

        if (found_blocks == num_blocks)
        {
            return 1;
        }
    }
    return 0;
}

int FS::check_access_rights(int dir_entry_index, int accessrights)
{
    if (dir_entry_index == 100)
    {
        dir_entry_index = 0;
    }

    int dest_file_accessrights = dir_entries[dir_entry_index].access_rights;

    //READ check
    if (accessrights == READ)
    {
        if (dest_file_accessrights == READ || dest_file_accessrights == READ + WRITE || dest_file_accessrights == READ + WRITE + EXECUTE || dest_file_accessrights == READ + EXECUTE)
        {
        return 0;
        }
        return -1;
    }
    //WRITE check
    if (accessrights == WRITE)
    {
        if (dest_file_accessrights == WRITE || dest_file_accessrights == READ + WRITE || dest_file_accessrights == READ + WRITE + EXECUTE || dest_file_accessrights == WRITE + EXECUTE)
        {
        return 0;
        }
        return -1;
    }
    return -1;
}

void FS::load_dir_content(void)
{
    disk.read(dir_entries[current_dir_index].first_blk, (uint8_t*) curr_dir_content); 
    disk.read(ROOT_BLOCK, (uint8_t*)dir_entries); // FOR SOME REASON ABOVE READ DESTROYS dir_entries, SO READ AGAIN.
    return;
}

int FS::traverse_filepath(string filepath, int from_cd, int from_emptycheck)
{
    //This function will traverse the filepath and change the current_path

    //Save current index
    if (from_emptycheck == 0)
    {
        reset_index = current_dir_index;
    }

    if (from_emptycheck == 1)
    {
        emptycheck_reset_index = current_dir_index;
    }

    //2. Traverse filepath
    std::vector<string> path_vector = path_to_vector(filepath);
    for (int i = 0; i < path_vector.size() - from_cd; i++)
    {
        if ((filepath.front() == '/') && (i == 0))
            {
                current_dir_index = 0;
                load_dir_content();
                continue;
            }
        
        if (path_vector[i] == "..")
        {
            if (dir_entries[current_dir_index].size == 100)
            {
                cout << "CANT GO ABOVE ROOT! \n";
                reset_filepath();
                return -1;
            }
            current_dir_index = dir_entries[curr_dir_content[0]].size;
            current_dir_index = dir_entries[current_dir_index].size;

            if (from_cd == 0)
            {
                previous_dirs.pop_back();
            }

            load_dir_content();
            continue;
        }

        current_dir_index = find_file_in_current_dir(path_vector[i]);
        
        if (current_dir_index == -1)
        {
            cout << "FOLDER NOT FOUND! \n";
            reset_filepath();
            return -1;
        }

        int parent_index = dir_entries[curr_dir_content[0]].size;
        if (check_access_rights(parent_index, READ) == -1)
        {
            cout << "You do not have the access rights to write to this folder! \n";
            reset_filepath();
            return -1;
        }    

        if (from_cd == 0)
        {
            previous_dirs.push_back(dir_entries[current_dir_index].file_name);
        }

        load_dir_content();
    }

    return 0;
}

void FS::reset_filepath(int from_emptycheck)
{
    //This function is used to reset the filepath after using "traverse_filepath" to create a file at a certain location

    //1. Change dir_path to the saved filepath that traverse_filepath saved in som global variable.
    if (from_emptycheck == 0)
    {
        current_dir_index = reset_index;
    }

    if (from_emptycheck == 1)
    {
        current_dir_index = emptycheck_reset_index;
    }

    load_dir_content();
    //2. Dones
    return;
}

dir_entry FS::create_dir_entry(int size_of_file, int start_block, int type, string filepath, int access_rights)
{
    //This function creates a dir_entry and returns it.
    struct dir_entry dir_entry_template;

    string filename = filepath;
    if (filepath != "")
    {
        filename = get_file_name(filepath);
    }

    strcpy(dir_entry_template.file_name, filename.c_str());
    dir_entry_template.size = size_of_file;
    dir_entry_template.first_blk = start_block;
    dir_entry_template.type = type;
    dir_entry_template.access_rights = READ + WRITE;

    return dir_entry_template;
}

string FS::get_file_name(string filepath)
{
      std::vector<string> path_str = path_to_vector(filepath);
      string filename = path_str.back();

      return filename;
}

std::vector<string> FS::path_to_vector(string filepath)
{
      std::vector<string> path_str;
      string dir;
      char delimiter = '/';
      int filepath_size = filepath.length();
      char filepath_char[filepath_size];
      strcpy(filepath_char, filepath.c_str());

      membuf sbuf(filepath_char, filepath_char + sizeof(filepath_char));
      istream in(&sbuf);

      while (getline(in, dir, delimiter))
      {
        path_str.push_back(dir);
      }

      return path_str;
}

int FS::check_number_of_blocks(int size_of_file)
{
    // check amount of block
    int num_blocks = size_of_file / BLOCK_SIZE + 1;
    int num_blocks_temp = size_of_file / BLOCK_SIZE;

    if (size_of_file==BLOCK_SIZE*num_blocks_temp)
    {
      num_blocks = num_blocks_temp;
    }

    return num_blocks;
}

int FS::find_space_and_write_to_disk(int num_blocks, uint8_t* block, string string_to_eval)
{   
    int start_block;
    int free_spaces[num_blocks];
    int free_space_counter = 0;
    int size_of_file_temp = string_to_eval.length();
    
    for (int i = 0; i < BLOCK_SIZE/2; i++)
    {
      // check where the file can fit in the fat
      if (fat[i] == FAT_FREE)
      {
        free_spaces[free_space_counter] = i;
        free_space_counter++;
      }

      if (free_space_counter >= num_blocks)
      {
        // go back and fill
        start_block = 0;

        //set fat values
        int elements_in_fat_counter = 1;
        for(int j = start_block; j < 64; j++)
        {
          if (fat[j] == FAT_FREE)
          {
            if (start_block==0)
            {
              start_block = j;
            }

            if (elements_in_fat_counter == free_space_counter)
            {
              fat[j] = FAT_EOF;
              break;
            }

            fat[j] = free_spaces[elements_in_fat_counter];
            elements_in_fat_counter++;
          }
        }
        disk.write(FAT_BLOCK, (uint8_t*)fat);

        // Write to the disk
        for(int j = start_block; j < start_block + num_blocks; j++)
        {
          if (num_blocks == 1)
          {
            disk.write(j, block);
            break;
          }

          for (int r = 0; r < num_blocks-1; r++)
          {
            string block_to_write = string_to_eval.substr(r*BLOCK_SIZE, r*BLOCK_SIZE + BLOCK_SIZE);
            uint8_t* block = (uint8_t*)block_to_write.c_str();
            disk.write(j+r, block);
            size_of_file_temp = size_of_file_temp - BLOCK_SIZE;
          }

          string block_to_write = string_to_eval.substr((num_blocks-1)*BLOCK_SIZE, (num_blocks-1)*BLOCK_SIZE + size_of_file_temp);
          uint8_t* block = (uint8_t*)block_to_write.c_str();
          disk.write(j+num_blocks-1, block);
          break;
        }
        break;
      }
    }
    return start_block;
}

int FS::find_space_and_write_folder_to_disk(int num_blocks, string folder_name)
{
    int start_block;
    int free_spaces[num_blocks];
    int free_space_counter = 0;
    for (int i = 0; i < BLOCK_SIZE/2; i++)
    {
        // check where the file can fit in the fat
        if (fat[i] == FAT_FREE)
        {
          free_spaces[free_space_counter] = i;
          free_space_counter++;
        }

        if (free_space_counter >= num_blocks)
        {
            // go back and fill
            start_block = i - free_space_counter + 1;
            //set fat values
            int elements_in_fat_counter = 1;
            for(int j = start_block; j < start_block + num_blocks; j++)
            {
                if(elements_in_fat_counter == free_space_counter)
                {
                    fat[j] = FAT_EOF;
                    elements_in_fat_counter++;
                }

                else
                {
                    fat[j] = free_spaces[elements_in_fat_counter];
                    elements_in_fat_counter++;
                }
            }
           
            disk.write(FAT_BLOCK, (uint8_t*)fat);

            dir_entry new_folder_entry = create_dir_entry(current_dir_index, start_block, TYPE_DIR, folder_name);
            int dir_entry_index = write_dir_entry_to_disk(new_folder_entry);


            dir_entry new_folder_pointer = create_dir_entry(dir_entry_index, dir_entries[current_dir_index].first_blk, TYPE_DIR, "..");
            int point_index = write_dir_entry_to_disk(new_folder_pointer, 1);
             
            int new_folder_dir_content[ROOT_SIZE];
            new_folder_dir_content[0] = point_index;

            for(int i = 1; i < ROOT_SIZE; i++)
            {
                new_folder_dir_content[i] = -1;
            }

            disk.write(ROOT_BLOCK, (uint8_t*)dir_entries);
            disk.write(start_block,  (uint8_t*)new_folder_dir_content);
            break;
        }
    }
    return 0;
}

int FS::write_dir_entry_to_disk(dir_entry entry, int from_mkdir)
{
    int dir_entry_index;
    for(int i = 0; i < ROOT_SIZE; i++)
    {
      if(dir_entries[i].first_blk == 0)
      {
        dir_entries[i] = entry;
        dir_entry_index = i;
        break;
      }
    }

    if (from_mkdir == 0)
    {
        for( int i = 1; i < ROOT_SIZE; i++)
        {
            if (curr_dir_content[i] == -1)
            {
                curr_dir_content[i] = dir_entry_index;
                break;
            }
        }
    }

    disk.write(ROOT_BLOCK, (uint8_t*)dir_entries);

    return dir_entry_index;
}

int FS::find_file_in_current_dir(string file_name)
{
    int file_found = 0;
    int entry_index;
    int blocks_to_read;
    
    for (int i = 0; i < ROOT_SIZE; i++)
    {
        if(dir_entries[curr_dir_content[i]].file_name == file_name)
        {
            file_found = 1;
            entry_index = curr_dir_content[i];
            return entry_index;
        }
    }
    return -1;
}

void FS::update_parent_directory()
{
    disk.write(dir_entries[current_dir_index].first_blk, (uint8_t*)curr_dir_content);
    return;
}

void FS::remove_file_from_fat(int dir_entry_index)
{
    int blocks_to_read = dir_entries[dir_entry_index].size/BLOCK_SIZE;
    if(dir_entries[dir_entry_index].size%BLOCK_SIZE > 0)
    {
        blocks_to_read++;
    }

    int curr_block = dir_entries[dir_entry_index].first_blk;
    int index = curr_block;
    for (int i = 0; i < blocks_to_read; i++)
    {
        index = curr_block;
        if(curr_block != -1)
        {
            curr_block = fat[curr_block];
        }
        fat[index] = FAT_FREE;
    }
    disk.write(FAT_BLOCK, (uint8_t*)fat);
    return;
}

int FS::folder_empty_check(int dir_entry_index, string filepath)
{
    int folder_to_check_content[ROOT_SIZE];
    traverse_filepath(filepath, 0, 1);

    for (int i = 1; i < ROOT_SIZE; i++)
    {
        if (curr_dir_content[i] != -1)
        {
            return -1;
        }
    }
    reset_filepath(1);
    return 0;
}
//*"*"*"*"*""*"*"*"*"" HELP FUNCTIONS ENDS HERE *"*"*"*"*""*"*"*"*""
FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";

    disk.read(ROOT_BLOCK, (uint8_t*)dir_entries);

    disk.read(FAT_BLOCK, (uint8_t*)fat);

    disk.read(ROOT_DIR, (uint8_t*)curr_dir_content);

    current_dir_index = 0;

    cout << "-- Finished booting -- \n\n";
}

FS::~FS(){}

// formats the disk, i.e., creates an empty file system
int
FS::format()
{
    std::cout << "FS::format()\n\n";

    string empty_str = "";
    dir_path.clear();

    uint8_t* empty_block = (uint8_t*)empty_str.c_str();

    for (int i = 2; i < disk.get_no_blocks(); i++)
    {
        fat[i] = 0;
        disk.write(i, empty_block);
    }

    dir_entry root_entry = create_dir_entry(100, ROOT_DIR, TYPE_DIR, "..");
    dir_entries[0] = root_entry;

    curr_dir_content[0] = 0;

    for(int i = 1; i < ROOT_SIZE; i++) 
    {
        struct dir_entry temp_entry;
        strcpy(temp_entry.file_name, "");
        temp_entry.size = 0;
        temp_entry.first_blk = 0;
        temp_entry.type = 0;
        temp_entry.access_rights = 0;
        dir_entries[i] = temp_entry;
    }

    for(int i = 1; i < ROOT_SIZE; i++)
    {
        curr_dir_content[i] = -1;
    }

    fat[ROOT_DIR] = FAT_EOF;
    disk.write(ROOT_DIR, (uint8_t*)curr_dir_content);

    fat[ROOT_BLOCK] = FAT_EOF;
    disk.write(ROOT_BLOCK, (uint8_t*)dir_entries);

    fat[FAT_BLOCK] = FAT_EOF;
    disk.write(FAT_BLOCK, (uint8_t*)fat);

    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{
    if (traverse_filepath(filepath) == -1)
    {
        return -1;
    }

    //Permissions
    int parent_index = dir_entries[curr_dir_content[0]].size;
    if (check_access_rights(parent_index, WRITE) == -1)
    {
        cout << "You do not have the access rights to write to this folder! \n";
        reset_filepath();
        return -1;
    }

    //Exist check
    string file_name = get_file_name(filepath);
    if (find_file_in_current_dir(file_name) != -1)
    {
        cout << "File already exists in current folder\n";
        reset_filepath();
        return -1;
    }

    int size_of_file;
    string string_to_eval;
    string string_to_eval_temp;
    int start_block;

    cout << "Enter file content: ";

    while (getline(cin, string_to_eval_temp) && string_to_eval_temp.length() != 0)
    {
      string_to_eval += string_to_eval_temp + "\n";
    }

    uint8_t* block = (uint8_t*)string_to_eval.c_str();
    size_of_file = string_to_eval.length();

    int num_blocks = file_size_check(string_to_eval);

    //Check that the file fits
    if (file_fit_check(num_blocks) == 0)
    {
        cout << "File too large!\n";
        return 0;
    }
    
    start_block = find_space_and_write_to_disk(num_blocks, block, string_to_eval);

    dir_entry entry = create_dir_entry(size_of_file, start_block, TYPE_FILE, filepath);

    write_dir_entry_to_disk(entry);

    update_parent_directory();
    
    reset_filepath();

    std::cout << "FS::create(" << filepath << ")\n\n";
    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n\n";
    int blocks_to_read;
    int entry_index;
    int file_found = 0;
     
    if (traverse_filepath(filepath) == -1)
    {
        return -1;
    }

    string file_name = get_file_name(filepath);

    entry_index = find_file_in_current_dir(file_name);

    if (find_file_in_current_dir(file_name) == -1)
    {
        cout << "File doesn't exist in current folder\n";
        reset_filepath();
        return -1;
    }

    if (dir_entries[entry_index].type == TYPE_DIR)
    {
        cout << "Can't read a folder\n";
        reset_filepath();
        return -1;
    }
    
    if (check_access_rights(entry_index, READ) == -1)
    {
        cout << "You do not have the access rights to read this file! \n";
        reset_filepath();
        return -1;
    }
    
    blocks_to_read = dir_entries[entry_index].size/BLOCK_SIZE;
    
    if(dir_entries[entry_index].size%BLOCK_SIZE > 0)
    {
        blocks_to_read++;
    }

    char* block_content;
    block_content = (char*)calloc(BLOCK_SIZE, sizeof(char));
    char* read_data;
    read_data = (char*) calloc(blocks_to_read, BLOCK_SIZE);
    memset(read_data, 0, BLOCK_SIZE*blocks_to_read);
    int next_block = dir_entries[entry_index].first_blk;
    
    for(int i = 0; i < blocks_to_read; i++)
    {
        disk.read(next_block, (uint8_t*)block_content);
        strcat(read_data, block_content);

        if(next_block != -1)
        {
            next_block = fat[next_block];
        }
    }
    
    cout << read_data << endl;
    free(block_content);
    free(read_data);
    
    reset_filepath();

    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
    std::cout << "FS::ls()\n\n";
    cout << "name\t" << "type\t" << "accesrights\t" << "size\n";

    for (int i = 0; i < sizeof(curr_dir_content)/sizeof(curr_dir_content[0]); i++)
    {
        if (curr_dir_content[i] != -1)
        {   
            cout << dir_entries[curr_dir_content[i]].file_name << "\t";

            if (dir_entries[curr_dir_content[i]].type == 0)
            {
                cout << "file\t";
            }
            else
            {
                cout << "dir\t";
            }

            string accessrights_str;
            int accessrights = dir_entries[curr_dir_content[i]].access_rights;

            if (accessrights == 0)
            {
                accessrights_str = "---";
            }

            else if (accessrights == READ)
            {
                accessrights_str = "r--";
            }

            else if (accessrights == READ + WRITE)
            {
                accessrights_str = "rw-";
            }

            else if (accessrights == READ + WRITE + EXECUTE)
            {
                accessrights_str = "rwx";
            }

            else if (accessrights == READ + EXECUTE)
            {
                accessrights_str = "r-x";
            }

            else if (accessrights == WRITE)
            {
                accessrights_str = "-w-";
            }

            else if (accessrights == WRITE + EXECUTE)
            {
                accessrights_str = "-wx";
            }

            else if (accessrights == EXECUTE)
            {
                accessrights_str = "--x";
            }

            cout << accessrights_str << "\t\t";

            if (dir_entries[curr_dir_content[i]].type != TYPE_DIR)
            {
                cout << dir_entries[curr_dir_content[i]].size << "\t";
            }
            
            else
            {
                cout << "-\t";
            }

            cout << "\n";
        }
    }
    
    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int
FS::cp(std::string sourcepath, std::string destpath)
{
    if (traverse_filepath(sourcepath) == -1)
    {
        return -1;
    }

    string source_file_name = get_file_name(sourcepath);
    
    int dir_entry_index = find_file_in_current_dir(source_file_name);

    if (dir_entry_index == -1)
    {
        cout << "The source file does not exist\n";
        reset_filepath();
        return -1;
    }

    int blocks_to_read = dir_entries[dir_entry_index].size / BLOCK_SIZE + 1;
    int blocks_to_read_temp = dir_entries[dir_entry_index].size / BLOCK_SIZE;

    if (file_fit_check(blocks_to_read) == 0)
    {
        cout << "File too large to fit on disk!\n";
        reset_filepath();
        return -1;
    }

    int access_rights_copy = dir_entries[dir_entry_index].access_rights;

    char* block_content;
    block_content = (char*)calloc(BLOCK_SIZE, sizeof(char));
    char* read_data;
    read_data = (char*) calloc(blocks_to_read,BLOCK_SIZE);
    int next_block = dir_entries[dir_entry_index].first_blk;
    for(int i = 0; i < blocks_to_read; i++)
    {
        disk.read(next_block, (uint8_t*)block_content);
        strcat(read_data, block_content);

        if(next_block != -1)
        {
            next_block = fat[next_block];
        }
    }

    string string_to_eval = read_data;
    uint8_t* block = (uint8_t*)string_to_eval.c_str();
    int file_size = string_to_eval.length();

    reset_filepath();

    if (traverse_filepath(destpath) == -1)
    {
        return -1;
    }

    string file_name = get_file_name(destpath);
    int destination_index = find_file_in_current_dir(file_name);
    if (destination_index != -1)
    {   
        if (dir_entries[destination_index].type == TYPE_DIR)
        {
            reset_filepath();
            traverse_filepath(destpath, 0);
            file_name = source_file_name;

            if (find_file_in_current_dir(file_name) != -1)
            {
                cout << "File already exists in destination folder\n";
                reset_filepath();
                return -1;
            }
        }

        else
        {
            cout << "File already exists in destination folder\n";
            reset_filepath();
            return -1;
        }
    }
    int start_block = find_space_and_write_to_disk(blocks_to_read, block, string_to_eval);

    dir_entry dir_entry_template = create_dir_entry(file_size, start_block, TYPE_FILE, file_name,  access_rights_copy);
    write_dir_entry_to_disk(dir_entry_template);
    
    update_parent_directory();

    reset_filepath();

    std::cout << "FS::cp(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    if (traverse_filepath(destpath) == -1)
    {
        return -1;
    }
    string dest_file_name = get_file_name(destpath);

    int destination_index = find_file_in_current_dir(dest_file_name);

    int move_to_dir = 1;
    if (destination_index != -1)
    {
        if (dir_entries[destination_index].type == TYPE_DIR)
        {
            reset_filepath();
            traverse_filepath(destpath, 0);
            move_to_dir = 0;
            if (find_file_in_current_dir(dest_file_name) != -1)
            {
                cout << "File already exists in destination folder\n";
                reset_filepath();
                return -1;
            }
        }
        else
        {
            cout << "File already exists in current folder\n";
            reset_filepath();
            return -1;
        }
    }

    reset_filepath();

    if (traverse_filepath(sourcepath) == -1)
    {
        return -1;
    }

    string source_file_name = get_file_name(sourcepath);
    int source_entry_index = find_file_in_current_dir(source_file_name);

    if (source_entry_index == -1)
    {
        cout << "Source file not found! \n";
        return -1;
    }

    if (dir_entries[source_entry_index].type == TYPE_DIR)
    {
        cout << "Cant move or rename a directory\n";
        return -1;
    }

    for (int i = 1; i < ROOT_SIZE; i++)
    {
        if(curr_dir_content[i] == source_entry_index)
        {
            curr_dir_content[i] = -1;
            update_parent_directory();
            break;
        }
    }

    reset_filepath();
    traverse_filepath(destpath, move_to_dir);

    if (move_to_dir == 0)
    {
        dest_file_name = source_file_name;
    }

    strcpy(dir_entries[source_entry_index].file_name, dest_file_name.c_str());
    disk.write(ROOT_BLOCK, (uint8_t*)dir_entries);

    for (int i = 1; i < ROOT_SIZE; i++)
    {
        if(curr_dir_content[i] == -1)
        {
            curr_dir_content[i] = source_entry_index;
            update_parent_directory();
            break;
        }
    }

    reset_filepath();
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    if (traverse_filepath(filepath) == -1)
    {
        return -1;
    }

    string file_name = get_file_name(filepath);
    int file_index = find_file_in_current_dir(file_name);

    if (file_index == -1)
    {
        cout << "Cant find file in directory\n";
        reset_filepath();
        return -1;
    }

    if (dir_entries[file_index].type == 1)
    {
        if (dir_entries[file_index].size == 100)
        {
            cout << "Cant remove root\n";
            reset_filepath();
            return -1;
        }

        if (folder_empty_check(file_index, file_name) == -1)
        {
            cout << "The folder needs to be empty! \n";
            reset_filepath();
            return -1;
        }
    }
    remove_file_from_fat(file_index);

    dir_entry clean_dir_entry = create_dir_entry(0, 0, 0, "");
    dir_entries[file_index] = clean_dir_entry;
    for(int i = 1; i < ROOT_SIZE; i++)
    {
        if (curr_dir_content[i] == file_index)
        {
            curr_dir_content[i] = -1;
            break;
        }
    }
    
    update_parent_directory();

    reset_filepath();

    std::cout << "FS::rm(" << filepath << ")\n";
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    if (traverse_filepath(filepath1) == -1)
    {
        // reset_filepath();
        return -1;
    }

    string source_file_name = get_file_name(filepath1);
    int source_entry_index = find_file_in_current_dir(source_file_name);

    if (source_entry_index == -1)
    {
        cout << "Cant find file in directory\n";
        reset_filepath();
        return -1;
    }

    if (check_access_rights(source_entry_index, READ) == -1)
    {
        cout << "You do not have the access rights to read this file! \n";
        reset_filepath();
        return -1;
    }

    reset_filepath();
    if (traverse_filepath(filepath1) == -1)
    {
        return -1;
    }

    string dest_file_name = get_file_name(filepath2);
    int dest_entry_index = find_file_in_current_dir(dest_file_name);

    if (dest_entry_index == -1)
    {
        cout << "Cant find file in directory\n";
        return -1;
    }

    if ((check_access_rights(dest_entry_index, READ) == -1) || (check_access_rights(dest_entry_index, WRITE) == -1))
    {
        cout << "You do not have the access rights to read and write to this file! \n";
        reset_filepath();
        return -1;
    }
    
    int source_blocks_to_read = check_number_of_blocks(dir_entries[source_entry_index].size);
    cout << "source_blocks_to_read: " << source_blocks_to_read << "\n";
    char* block_content_1;
    block_content_1 = (char*)calloc(BLOCK_SIZE, sizeof(char));
    char* read_data_1;
    read_data_1 = (char*) calloc(source_blocks_to_read,BLOCK_SIZE*2);
    memset(read_data_1, 0, BLOCK_SIZE*source_blocks_to_read);
    int next_block_1 = dir_entries[source_entry_index].first_blk;
    for(int i = 0; i < source_blocks_to_read; i++)
    {
        disk.read(next_block_1, (uint8_t*)block_content_1);
        strcat(read_data_1, block_content_1);

        if(next_block_1 != -1)
        {
            next_block_1 = fat[next_block_1];
        }
    }

    int dest_blocks_to_read = check_number_of_blocks(dir_entries[dest_entry_index].size);
    cout << "dest_blocks_to_read: " << dest_blocks_to_read << "\n";
    char* block_content_2;
    block_content_2 = (char*)calloc(BLOCK_SIZE, sizeof(char));
    char* read_data_2;
    read_data_2 = (char*) calloc(dest_blocks_to_read+source_blocks_to_read,BLOCK_SIZE);
    memset(read_data_2, 0, BLOCK_SIZE*(dest_blocks_to_read+source_blocks_to_read));
    int next_block_2 = dir_entries[dest_entry_index].first_blk;
    for(int i = 0; i < dest_blocks_to_read; i++)
    {
        disk.read(next_block_2, (uint8_t*)block_content_2);
        strcat(read_data_2, block_content_2);

        if(next_block_2 != -1)
        {
            next_block_2 = fat[next_block_2];
        }
    }

    strcat(read_data_2, read_data_1);
    string string_to_eval = read_data_2;
    int size_of_file = string_to_eval.length();
    int num_blocks = check_number_of_blocks(size_of_file);
    uint8_t* block = (uint8_t*)string_to_eval.c_str();
    if (file_fit_check(num_blocks) == 0)
    {
        cout << "File too large!\n";
        return 0;
    }
    remove_file_from_fat(dest_entry_index);
    int start_block = find_space_and_write_to_disk(num_blocks, block, string_to_eval);

    dir_entries[dest_entry_index].size = size_of_file;
    dir_entries[dest_entry_index].first_blk = start_block;

    disk.write(ROOT_BLOCK, (uint8_t*)dir_entries);

    reset_filepath();

    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(std::string dirpath)
{
    if (traverse_filepath(dirpath) == -1)
    {
        return -1;
    }

    int parent_index = dir_entries[curr_dir_content[0]].size;
    if (check_access_rights(parent_index, WRITE) == -1)
    {
        cout << "You do not have the access rights to write to this folder! \n";
        reset_filepath();
        return -1;
    }    

    string folder_name = get_file_name(dirpath);
    
    if (find_file_in_current_dir(folder_name) != -1)
    {
        cout << "File/folder already exists in directory\n";
        reset_filepath();
        return -1;        
    }
    
    if (file_fit_check(1) == 0)
    {
        cout << "DISK IS FULL!\n";
        return 0;
    }

    find_space_and_write_folder_to_disk(1, folder_name);

    update_parent_directory();

    reset_filepath();

    std::cout << "FS::mkdir(" << dirpath << ")\n";
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
    if (traverse_filepath(dirpath, 0) == -1)
    {
        return -1;
    }

    std::cout << "FS::cd(" << dirpath << ")\n";
    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int
FS::pwd()
{
    cout << "/";
    for (int i = 0; i < previous_dirs.size(); i++)
    {
        cout << previous_dirs[i] << "/";
    }
    cout << "\n";
    std::cout << "FS::pwd()\n";
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
    traverse_filepath(filepath);

    string file_name = get_file_name(filepath);
    int entry_index = find_file_in_current_dir(file_name);

    if (find_file_in_current_dir(file_name) == -1)
    {
        cout << "File does not exist in current folder\n";
        reset_filepath();
        return -1;
    }

    int accessrights_int = -1;
    int int_in_accessrights = std::stoi(accessrights);
    if (int_in_accessrights == 0)
    {
      accessrights_int = 0;
    }

    else if (int_in_accessrights == READ)
    {
      accessrights_int = READ;
    }

    else if (int_in_accessrights == READ + WRITE)
    {
      accessrights_int = READ + WRITE;
    }

    else if (int_in_accessrights == READ + WRITE + EXECUTE)
    {
      accessrights_int = READ + WRITE + EXECUTE;
    }

    else if (int_in_accessrights == READ + EXECUTE)
    {
      accessrights_int = READ + EXECUTE;
    }

    else if (int_in_accessrights == WRITE)
    {
      accessrights_int = WRITE;
    }

    else if (int_in_accessrights == WRITE + EXECUTE)
    {
      accessrights_int = WRITE + EXECUTE;
    }

    else if (int_in_accessrights == EXECUTE)
    {
      accessrights_int = EXECUTE;
    }

    else
    {
      cout << "Wrong format on input\n";
      return -1;
    }

    dir_entries[entry_index].access_rights = accessrights_int;

    disk.write(ROOT_BLOCK, (uint8_t*)dir_entries);
    update_parent_directory();
    reset_filepath();

    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
    return 0;
}
