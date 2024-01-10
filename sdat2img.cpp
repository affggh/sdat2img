// LICENISE         : GPLv3
// Author           : affggh
// Original script  : https://github.com/xpirt/sdat2img

#define VERSION 1.2
#define BLOCK_SIZE 4096

// 32bit file max only 2Gb
// so we use 64 bits offset
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <iostream>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstring>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>

using namespace std;

struct block_range
{
    streamoff a;
    streamoff b;
};

struct block_command
{
    string              command;
    vector<block_range> blocks;
};

streamoff max(vector<block_range> &n)
{
    streamoff max_num = 0;
    for (const auto &i : n) {
       if (i.a > max_num || i.b > max_num) {
            max_num = (i.a > i.b) ? i.a : i.b;
       }
    }
    return max_num * BLOCK_SIZE;
}

class SDAT2IMG 
{
private:
    ifstream flist, fdat;
    ofstream fout;

public:
    errno_t etype;

    SDAT2IMG(const char *transfer_list_file, const char *new_dat_file, const char *output_file) 
    {
        if (!(access(transfer_list_file, F_OK) == 0)){
            cerr << "Error: file " << transfer_list_file << "does not exist!" << endl;
            etype = EIO;
            return;
        }

        if (!(access(new_dat_file, F_OK) == 0)) {
            cerr << "Error: file " << new_dat_file << "does not exist!" << endl;
            etype = EIO;
            return;
        }

        struct stat st;
        stat(transfer_list_file, &st);
        if (S_ISDIR(st.st_mode)) {
            cerr << "Error: " << transfer_list_file << "is a dir!" << endl;
            etype = EIO;
            return;
        }

        stat(new_dat_file, &st);
        if (S_ISDIR(st.st_mode)) {
            cerr << "Error: " << new_dat_file << "is a dir!" << endl;
            etype = EIO;
            return;
        }

        flist.open(transfer_list_file);
        if (!flist) {
            cerr << "Error: " << "could not open transfer list file." << endl;
            etype = EIO;
            return;
        }

        fdat.open(new_dat_file, ios::binary);
        if (!fdat) {
            cerr << "Error: " << "could not open new.dat file." << endl;
            etype = EIO;
            return;
        }

        fout.open(output_file, ios::binary | ios::trunc);
        if (!fout) {
            cerr << "Error: " << "could not open output file." << endl;
            etype = EIO;
            return;
        }
    }

    vector<block_range> split_blocks(string blocks) {
        string line;
        istringstream iss(blocks);
        auto num_len = 0ull;
        vector<streamoff> b;
        vector<block_range> r;

        while(getline(iss, line, ',')) {
            if (num_len == 0) {
                num_len = atoi(line.c_str());
                continue;
            }
            b.push_back(atoi(line.c_str()));
        }

        if (num_len != b.size()) {
            cerr << "Error on parsing following data to rangeset:" << endl << blocks << endl;
            etype = EIO;
            return r;
        }

        for (auto i=0ull; i<b.size(); i+=2) {
            r.push_back(block_range{b[i], b[i+1]});
        }

        return r;
    }

    int parse_transfer_list_file(int &version, int &new_blocks, vector<block_command> &c) {
        int ret = 0;

        istringstream iss;
        string line, cmd, b;

        getline(flist, line);
        version = atoi(line.c_str());

        getline(flist, line);
        new_blocks = atoi(line.c_str());

        if (version >= 2) {
            getline(flist, line);
            getline(flist, line);
        }

        while(getline(flist, line)) {
            iss.clear();
            iss.str(line);
            iss >> cmd;
            if (cmd == "erase" || cmd == "new" || cmd == "zero") {
                iss >> b;
                c.push_back(block_command{cmd, split_blocks(b)});
                if (etype) {
                    ret = etype;
                    break;
                }
            } else {
                cerr << "Command " << cmd << " is not valid." << endl;
                ret = 1;
                break;
            }
        }

        return ret;
    }

    ~SDAT2IMG()
    {
        if (flist)
            flist.close();
        if (fdat)
            fdat.close();
        if (fout)
            fout.close();
    }

    int run(void)
    {
        int                     version, new_blocks;
        vector<block_command>   commands;
        vector<block_range>     all_block_sets;
        streamoff                 max_file_size = 0;
        char                    buf[BLOCK_SIZE];
        streamoff               begin, end, block_count;

        parse_transfer_list_file(version, new_blocks, commands);

        switch (version) {
            case 1:
                cout << "Android Lollipop 5.0 detected!" << endl;
                break;
            case 2:
                cout << "Android Lollipop 5.1 detected!" << endl;
                break;
            case 3:
                cout << "Android Marshmallow 6.x detected!" << endl;
                break;
            case 4:
                cout << "Android Nougat 7.x / Oreo 8.x detected!" << endl;
                break;
            default:
                cout << "Unknown Android version!" << endl;
                break;
        }

        for (const auto &i : commands) {
            for (const auto &b : i.blocks) {
                all_block_sets.push_back(b);
            }
        }

        max_file_size = max(all_block_sets);

        for (const auto &c : commands) {
            if (c.command == "new") {
                for (const auto &block : c.blocks) {
                    begin = block.a;
                    end = block.b;
                    block_count = end - begin;

                    cout << "Copying " << block_count <<" blocks into position " << begin << "..." << fout.tellp() << endl;;

                    fout.seekp(begin*BLOCK_SIZE, ios::beg);

                    while (block_count > 0) {
                        fdat.read(buf, sizeof(buf));
                        fout.write(buf, sizeof(buf));
                        block_count--;
                    }
                }
            } else {
                cout << "Skipping command " << c.command << "..." << endl;
            }
        }

        if (fout.tellp() < max_file_size) {
            fout.seekp(max_file_size, ios::beg);
            fout.flush();
        }
        return 0;
    }
};

void help(void) 
{
    cout << "Usage: ./sdat2img [system.transfer.list] [system.new.dat] <system.img>" << endl;
}

int main(int argc, char** argv) 
{
    int ret = 0;
    char *transfer_list_file, *new_dat_file, *output_file = nullptr;

    cout << "sdat2img cpp version: " << VERSION << endl;
    if (argc < 3 || argc > 4) {
        help();
        return 1;
    }

    if (argc == 3) {
        output_file = strdup("system.img");
    } else if (argc == 4) {
        output_file = strdup(argv[3]);
    }
    transfer_list_file = strdup(argv[1]);
    new_dat_file = strdup(argv[2]);

    SDAT2IMG sdat2img(transfer_list_file, new_dat_file, output_file);
    if (sdat2img.etype) {
        goto error;
    }

    ret = sdat2img.run();
    if (!ret)
        goto success;

error:
    cerr << "Error: something went worng!" << endl;
    ret = sdat2img.etype;

success:
    if (output_file)
        free(output_file);
    if(transfer_list_file)
        free(transfer_list_file);
    if (new_dat_file)
        free(new_dat_file);

    return ret;
}
