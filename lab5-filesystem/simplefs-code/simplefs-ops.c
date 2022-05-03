#include "simplefs-ops.h"
extern struct filehandle_t file_handle_array[MAX_OPEN_FILES]; // Array for storing opened files

int simplefs_create(char *filename){
	/*
	    Create file with name `filename` from disk
	*/
	struct inode_t tempInode;
	for(int i = 0; i < NUM_INODES; i++){
		simplefs_readInode(i, &tempInode);
		if(tempInode.status == INODE_IN_USE && strncmp(tempInode.name, filename, MAX_NAME_STRLEN) == 0)
			return -1;
	}

	int inode_no;
	if((inode_no = simplefs_allocInode()) != -1){
		simplefs_readInode(inode_no, &tempInode);
		tempInode.status = INODE_IN_USE;
		strncpy(tempInode.name, filename, MAX_NAME_STRLEN);
		tempInode.file_size = 0;
		for(int i = 0; i < MAX_FILE_SIZE; i++)
  	    	tempInode.direct_blocks[i] = -1;
		simplefs_writeInode(inode_no, &tempInode);
		return inode_no;
	}
	else return -1;
}


void simplefs_delete(char *filename){
	/*
	    delete file with name `filename` from disk
	*/
	struct inode_t tempInode;
	int inode_no = -1;
	for(int i = 0; i < NUM_INODES; i++){
		simplefs_readInode(i, &tempInode);
		if(tempInode.status == INODE_IN_USE && strncmp(tempInode.name, filename, MAX_NAME_STRLEN) == 0){
			inode_no = i;
			break;
		}
	}
	if(inode_no == -1)
		return;
	
	for(int i = 0; i < MAX_FILE_SIZE; i++){
		if(tempInode.direct_blocks[i] != -1)
			simplefs_freeDataBlock(tempInode.direct_blocks[i]);
	}
	tempInode.status = INODE_FREE;
	simplefs_freeInode(inode_no);
}

int simplefs_open(char *filename){
	/*
	    open file with name `filename`
	*/
	struct inode_t tempInode;
	int inode_no = -1;
	for(int i = 0; i < NUM_INODES; i++){
		simplefs_readInode(i, &tempInode);
		if(tempInode.status == INODE_IN_USE && strncmp(tempInode.name, filename, MAX_NAME_STRLEN) == 0){
			inode_no = i;
			break;
		}
	}
	if(inode_no == -1)
		return -1;
    
	for(int i = 0; i < MAX_OPEN_FILES; i++){
		if(file_handle_array[i].inode_number == -1){
			file_handle_array[i].inode_number = inode_no;
			file_handle_array[i].offset = 0;
			return i;
		}
	}
	return -1;
}

void simplefs_close(int file_handle){
	/*
	    close file pointed by `file_handle`
	*/
	if(file_handle >= MAX_OPEN_FILES)
		return;
	file_handle_array[file_handle].inode_number = -1;
	file_handle_array[file_handle].offset = 0;
}

int simplefs_read(int file_handle, char *buf, int nbytes){
	/*
	    read `nbytes` of data into `buf` from file pointed by `file_handle` starting at current offset
	*/
	if(file_handle >= MAX_OPEN_FILES)
		return -1;
	if(file_handle_array[file_handle].inode_number == -1)
		return -1;
	if(nbytes <= 0)
		return -1;

	struct inode_t tempInode;
	simplefs_readInode(file_handle_array[file_handle].inode_number, &tempInode);

	int curr = file_handle_array[file_handle].offset;
	if(curr + nbytes > tempInode.file_size)
    		return -1;

	int currblk = curr/BLOCKSIZE, lastblk = 1+(curr+nbytes-1)/BLOCKSIZE;
	char* buffer = (char*)malloc((lastblk-currblk)*BLOCKSIZE);
	for(int i = 0; currblk+i < lastblk; i++)
		simplefs_readDataBlock(tempInode.direct_blocks[currblk+i], buffer+i*BLOCKSIZE);
	memcpy(buf, buffer + curr%BLOCKSIZE, nbytes);
	free(buffer);
	return 0;
}


int simplefs_write(int file_handle, char *buf, int nbytes){
	/*
	    write `nbytes` of data from `buf` to file pointed by `file_handle` starting at current offset
	*/
	if(file_handle >= MAX_OPEN_FILES)
		return -1;
	if(file_handle_array[file_handle].inode_number == -1)
		return -1;
	if(nbytes <= 0)
		return -1;

	struct inode_t tempInode;
	simplefs_readInode(file_handle_array[file_handle].inode_number, &tempInode);

	int curr = file_handle_array[file_handle].offset;
	if(curr + nbytes > MAX_FILE_SIZE*BLOCKSIZE)
		return -1;

	int currblk = curr/BLOCKSIZE, lastblk = 1+(curr+nbytes-1)/BLOCKSIZE;
	int endblk = tempInode.file_size ? 1+(tempInode.file_size-1)/BLOCKSIZE : 0;

	int i, newblocks[NUM_DATA_BLOCKS];
	for(i = 0; endblk+i < lastblk; i++){
		newblocks[i] = simplefs_allocDataBlock();
		if(newblocks[i] == -1){
			for(int j = 0; j < i; j++)
				simplefs_freeDataBlock(newblocks[i]);
			return -1;
		}
	}
	for(int j = 0; j < i; j++){
		tempInode.direct_blocks[endblk+j] = newblocks[j];
	}
	tempInode.file_size = curr+nbytes > tempInode.file_size ? curr+nbytes : tempInode.file_size;
	simplefs_writeInode(file_handle_array[file_handle].inode_number, &tempInode);

	char* buffer = (char*)malloc((lastblk-currblk)*BLOCKSIZE);
	for(int j = 0; currblk+j < endblk; j++)
		simplefs_readDataBlock(tempInode.direct_blocks[currblk+j], buffer+j*BLOCKSIZE);
	memcpy(buffer + curr%BLOCKSIZE, buf, nbytes);
	for(int j = 0; currblk+j < lastblk; j++)
		simplefs_writeDataBlock(tempInode.direct_blocks[currblk+j], buffer+j*BLOCKSIZE);
	free(buffer);
	return 0;
}


int simplefs_seek(int file_handle, int nseek){
	/*
	   increase `file_handle` offset by `nseek`
	*/
	if(file_handle >= MAX_OPEN_FILES)
		return -1;
	if(file_handle_array[file_handle].inode_number == -1)
		return -1;

	struct inode_t tempInode;
	simplefs_readInode(file_handle_array[file_handle].inode_number, &tempInode);

	if(file_handle_array[file_handle].offset + nseek > tempInode.file_size)
    		return -1;
	if(file_handle_array[file_handle].offset + nseek < 0)
    		return -1;
	
	file_handle_array[file_handle].offset += nseek;
	return 0;
}

