#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "API.h"

int dir_make(char* name)
{

    int inode_num = search_cur_dir(name);
    int cur_dir_num = search_cur_dir(".");
    if (inode_num > -1) {
        printf("mkdir: Already a file or directory\n");
        return -1;
    }
    if (curDir.numEntry + 1 > MAX_DIR_ENTRY || superBlock.freeInodeCount < 1) {
        printf("mkdir: directory or inode is full\n");
        return -1;
    }
    if (superBlock.freeBlockCount < 1) {
        printf("mkdir: data block is full\n");
    }

    inode_num = get_inode();
    int block = get_block();
    Inode newInode;
    newInode.type = directory;
    newInode.size = 1;
    newInode.blockCount = 1;
    newInode.linkCount = 1;
    strncpy(curDir.dentry[curDir.numEntry].name, name, strlen(name));
    curDir.dentry[curDir.numEntry].name[strlen(name)] = '\0';
    curDir.dentry[curDir.numEntry].inode = inode_num;
    curDir.numEntry++;
    write_disk_block(curDirInode,(char*)&curDir);

    DirectoryEntry parent;
    DirectoryEntry child;
    child.name[0] = '.';
    child.name[1] = '\0';
    child.inode = inode_num;
    parent.name[0] = '.';
    parent.name[1] = '.';
    parent.name[2] = '\0';
    parent.inode = cur_dir_num;
//    parent.inode = 1;
    Dentry newDentry;
    newDentry.numEntry = 2;
    newDentry.dentry[0] = child;
    newDentry.dentry[1] = parent;
    write_disk_block(block, (char*)&newDentry);
    newInode.directBlock[0] = block;
    write_inode(inode_num, newInode);
    return 0;
}

int dir_remove(char *name)
{
    int inode_num = search_cur_dir(name);
    if (inode_num < 0) {
        printf("rmdir : could not find directory");
        return -1;
    }
    if (inode_num == 0) {
        printf("rmdir : cannot delete the root directory");
        return -1;
    }
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        printf("rmdir : can only delete subdirectories");
        return -1;
    }
    Inode inode = read_inode(inode_num);
    if (inode.type != directory) {
        printf("rmdir : not a directory");
        return -1;
    }
    Dentry dentry;
    read_disk_block((int)inode.directBlock[0],(char*)&dentry);
    if (dentry.numEntry > 2) {
        printf("rmdir : can only delete an empty directory");
        return -1;
    }
    for (int i = 0; i < curDir.numEntry; i++) {
        if (strcmp(curDir.dentry[i].name, name) == 0) {
            if (i != curDir.numEntry - 1)
                curDir.dentry[i] = curDir.dentry[curDir.numEntry - 1];
            curDir.numEntry--;
        }
    }
    write_disk_block(curDirInode, (char*)&curDir);
    free_block(inode.directBlock[0]);
    free_inode(inode_num);
    return 0;
}

int dir_change(char* name)
{
    int dest_inode_num = search_cur_dir(name);
    if (dest_inode_num < 0) {
        printf("cd : could not find directory");
        return -1;
    }
    Inode dest_inode = read_inode(dest_inode_num);
    if (dest_inode.type != directory) {
        printf("cd : not a directory");
        return -1;
    }
    Dentry *destDentry = malloc(sizeof(Dentry));
    read_disk_block((int)dest_inode.directBlock[0], (char*)destDentry);
    memcpy(&curDir, destDentry, sizeof(Dentry));
    free(destDentry);
    curDirInode = dest_inode.directBlock[0];
   	return 0;
}


/* ===================================================== */

int ls()
{
		int i;
		int inodeNum;
		Inode targetInode;
		for(i = 0; i < curDir.numEntry; i++)
		{
				inodeNum = curDir.dentry[i].inode;
				targetInode = read_inode(inodeNum);
				if(targetInode.type == file) printf("type: file, ");
				else printf("type: dir, ");
				printf("name \"%s\", inode %d, size %d byte\n", curDir.dentry[i].name, curDir.dentry[i].inode, targetInode.size);
		}

		return 0;
}
