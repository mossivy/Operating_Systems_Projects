#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "API.h"

int file_cat(char *name)
{
    int inodeNum = search_cur_dir(name);
    if (inodeNum < 0) {
        printf("Could not find file");
        return 0;
    }
    Inode inode = read_inode(inodeNum);

    if (inode.type == directory) {  // directory = 1
        printf("Can not read a directory");
        return 0;
    }
    char *file = malloc(inode.size);
    char buf[512];
    int bytes_read = 0;
    int indirect_block_map[128];
    if (inode.indirectBlock > 0) {
        read_disk_block(inode.indirectBlock, (char*)indirect_block_map);
    }
    for (int i = 0; i < inode.blockCount; i++) {
        memset(buf, 0, 512);
        int block;
        if (i < 15) {
            block = inode.directBlock[i];

        } else {
            block = indirect_block_map[i-15];
        }
        read_disk_block(block, buf);
        int byte_diff = inode.size - bytes_read;
        if (byte_diff > 512){
            memcpy(file + bytes_read, buf, 512);
            bytes_read += 512;
        }
        else {
            memcpy(file + bytes_read, buf, byte_diff);
            bytes_read += byte_diff;
            buf[byte_diff] = "\0";
        }
    }
    printf("%s", file);
    return 0;
}

int file_remove(char *name)
{
    //search_cur_dir(char *name)
    int src_inode_num = search_cur_dir(name);
    if (src_inode_num < 0) {
        printf("Cannot find source file\n");
        return -1;
    }
    Inode src_inode = read_inode(src_inode_num);
    if (src_inode.type != file) {
        printf("Not a file");
        return -1;
    }

    for (int i = 0; i < curDir.numEntry; i++) {
        if (strcmp(curDir.dentry[i].name, name) == 0) {
            //might be off by one
            if (i != curDir.numEntry - 1) {
                curDir.dentry[i] = curDir.dentry[curDir.numEntry - 1];
            } // else it is at the end
            curDir.numEntry--;
        }
    }
    for (int i = 1; i < src_inode.linkCount; i++) {
        for (int n = 0; n < curDir.numEntry; n++) {
            int link_inode = search_cur_dir(curDir.dentry[i].name);
            if (src_inode_num == link_inode) {
                if (i != curDir.numEntry - 1) {
                    curDir.dentry[i] = curDir.dentry[curDir.numEntry - 1];
                } // else it is at the end
                curDir.numEntry--;
            }
        }
    }

    write_disk_block(curDirInode,(char*)&curDir);
    int indirect_block_map[128];
    if (src_inode.indirectBlock > 0) {
        read_disk_block(src_inode.indirectBlock, (char*)indirect_block_map);
    }
    for (int i = 0; i < src_inode.blockCount; i++) {
        int block;
        if (i < 15) {
            block = src_inode.directBlock[i];

        } else {
            block = indirect_block_map[i-15];
        }
        free_block(block);
    }
    free_inode(src_inode_num);
   	return 0;
}

int hard_link(char *src, char *dest)
{
    int src_inode_num = search_cur_dir(src);
    int dest_inode_num = search_cur_dir(dest);
    if (src_inode_num < 0) {
        printf("Cannot find source file\n");
        return -1;
    }
    if (dest_inode_num > 0) {
        printf("Already a file");
        return -1;
    }
    Inode src_inode = read_inode(src_inode_num);
    if (src_inode.type != file) {
        printf("Not a file");
        return -1;
    }

    if (curDir.numEntry + 1 > MAX_DIR_ENTRY) {
        printf("Hard link failed: directory is full\n");
        return -1;
    }

    strncpy(curDir.dentry[curDir.numEntry].name, dest, strlen(dest));
    curDir.dentry[curDir.numEntry].name[strlen(dest)] = '\0';
    curDir.dentry[curDir.numEntry].inode = src_inode_num;
    curDir.numEntry++;
    write_disk_block(curDirInode,(char*)&curDir);
    src_inode.linkCount++;
    write_inode(src_inode_num, src_inode);

    return 0;
}

int file_copy(char *src, char *dest)
{
    //search_cur_dir(char *src)
    //read_disk(..)
    //look at file_create
    int src_inode_num = search_cur_dir(src);
    int dest_inode_num = search_cur_dir(dest);
    if (src_inode_num < 0) {
        printf("file copy: source fnf");
        return -1;
    }
    if (dest_inode_num > 0) {
        printf("Already a file");
        return -1;
    }
    if (curDir.numEntry + 1 > MAX_DIR_ENTRY) {
        printf("file copy: directory full");
        return -1;
    }
    Inode src_inode = read_inode(src_inode_num);
    if (src_inode.type != file) {
        printf("file copy: not a file");
        return -1;
    }
    if (src_inode.blockCount > superBlock.freeBlockCount) {
        printf("file copy: data block full\n");
        return -1;
    }

    Inode newInode;
    newInode.type = file;
    newInode.size = src_inode.size;
    newInode.blockCount = src_inode.blockCount;
    newInode.linkCount = 1;

    int inodeNum = get_inode();
    if(inodeNum < 0) {
        printf("File_create error: not enough inode.\n");
        return -1;
    }
    // add a new file into the current directory entry
    strncpy(curDir.dentry[curDir.numEntry].name, dest, strlen(dest));
    curDir.dentry[curDir.numEntry].name[strlen(dest)] = '\0';
    curDir.dentry[curDir.numEntry].inode = inodeNum;
    curDir.numEntry++;
    write_disk_block(curDirInode,(char*)&curDir);

    int block;
    char buf[512];
    int i;
    for(i = 0; i < 15; i++)
    {
        if (i >= src_inode.blockCount) break;
        block = get_block();
        if(block == -1) {
            printf("File_create error: get_block failed\n");
            return -1;
        }
        //set direct block
        newInode.directBlock[i] = block;
        read_disk_block(src_inode.directBlock[i], buf);
        write_disk_block(block, buf);
    }

    if(src_inode.size > 7680) {
        // get an indirect block
        block = get_block();
        if(block == -1) {
            printf("File_create error: get_block failed\n");
            return -1;
        }
        newInode.indirectBlock = block;
        int src_indirectBlockMap[128];
        int indirectBlockMap[128];
        read_disk_block(src_inode.indirectBlock, (char*)src_indirectBlockMap);
        for(i = 15; i < src_inode.blockCount; i++)
        {
            block = get_block();
            if(block == -1) {
                printf("File_create error: get_block failed\n");
                return -1;
            }
            //set direct block
            indirectBlockMap[i-15] = block;
            read_disk_block(src_indirectBlockMap[i-15], buf);
            write_disk_block(block, buf);
        }
        write_disk_block(newInode.indirectBlock, (char*)indirectBlockMap);
    }
    write_inode(inodeNum, newInode);
    return 0;
}


/* =============================================================*/

int file_create(char *name, int size)
{
		int i;
		int block, inodeNum, numBlock;

		if(size <= 0 || size > 73216){
				printf("File create failed: file size error\n");
				return -1;
		}

		inodeNum = search_cur_dir(name);
		if(inodeNum >= 0) {
				printf("File create failed:  %s exist.\n", name);
				return -1;
		}

		if(curDir.numEntry + 1 > MAX_DIR_ENTRY) {
				printf("File create failed: directory is full!\n");
				return -1;
		}

		if(superBlock.freeInodeCount < 1) {
				printf("File create failed: inode is full!\n");
				return -1;
		}

        //?
		numBlock = size / BLOCK_SIZE;
		if(size % BLOCK_SIZE > 0) numBlock++;

		if(size > 7680) {
				if(numBlock+1 > superBlock.freeBlockCount)
				{
						printf("File create failed: data block is full!\n");
						return -1;
				}
		} else {
				if(numBlock > superBlock.freeBlockCount) {
						printf("File create failed: data block is full!\n");
						return -1;
				}
		}

		char *tmp = (char*) malloc(sizeof(int) * size+1);

		rand_string(tmp, size);
		printf("File contents:\n%s\n", tmp);

		// get inode and fill it
		inodeNum = get_inode();
		if(inodeNum < 0) {
				printf("File_create error: not enough inode.\n");
				return -1;
		}

		Inode newInode;

		newInode.type = file;
		newInode.size = size;
		newInode.blockCount = numBlock;
		newInode.linkCount = 1;

		// add a new file into the current directory entry
		strncpy(curDir.dentry[curDir.numEntry].name, name, strlen(name));
		curDir.dentry[curDir.numEntry].name[strlen(name)] = '\0';
		curDir.dentry[curDir.numEntry].inode = inodeNum;
		curDir.numEntry++;
        write_disk_block(curDirInode,(char*)&curDir);

		// get data blocks
		for(i = 0; i < 15; i++)
		{
				if (i >= numBlock) break;
				block = get_block();
				if(block == -1) {
						printf("File_create error: get_block failed\n");
						return -1;
				}
				//set direct block
				newInode.directBlock[i] = block;
				write_disk_block(block, tmp+(i*BLOCK_SIZE));
		}

		if(size > 7680) {
				// get an indirect block
				block = get_block();
				if(block == -1) {
						printf("File_create error: get_block failed\n");
						return -1;
				}

				newInode.indirectBlock = block;
				int indirectBlockMap[128];

				for(i = 15; i < numBlock; i++)
				{
                    block = get_block();
                    if(block == -1) {
                        printf("File_create error: get_block failed\n");
                        return -1;
                    }
                    //set direct block
                    indirectBlockMap[i-15] = block;
                    write_disk_block(block, tmp+(i*BLOCK_SIZE));
				}
				write_disk_block(newInode.indirectBlock, (char*)indirectBlockMap);
		}

		write_inode(inodeNum, newInode);
		printf("File created. name: %s, inode: %d, size: %d\n", name, inodeNum, size);

		free(tmp);
		return 0;
}

int file_stat(char *name)
{
		char timebuf[28];
		Inode targetInode;
		int inodeNum;

		inodeNum = search_cur_dir(name);
		if(inodeNum < 0) {
				printf("file cat error: file is not exist.\n");
				return -1;
		}

		targetInode = read_inode(inodeNum);
		printf("Inode = %d\n", inodeNum);
		if(targetInode.type == file) printf("type = file\n");
		else printf("type = directory\n");
		printf("size = %d\n", targetInode.size);
		printf("linkCount = %d\n", targetInode.linkCount);
		printf("num of block = %d\n", targetInode.blockCount);
}
