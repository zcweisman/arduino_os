#include <stdio.h>
#include <avr/io.h>
#include <string.h>
#include <stdlib.h>
#include "ext.h"
#include "SdReader.h"

#define DIRECT_BLOCKS 12
#define INDIRECT_BLOCKS 256
#define DOUBLE_INDIRECT_BLOCKS 65536
#define DIR_READ_SIZE 100
#define ROOT_INODE 2
#define BLOCK_SIZE 1024
#define SECTOR_SIZE 512
#define BUFFER_SIZE 256
#define INODE_TYPE_DIR 0x4000
#define INODE_TYPE_FILE 0x8000
#define NUM_SONGS 13
#define BLOCKS_PER_GROUP 8192
#define INODES_PER_GROUP 7696
#define INODE_TABLE_OFFSET 5
#define MAX_NAME_LEN 75

//Public Functions
void getSongName(uint16_t index, char buffer[MAX_NAME_LEN]);
//Search
struct ext2_dir_entry searchRoot(uint16_t index, char buffer[MAX_NAME_LEN]);
//Directory 
struct ext2_dir_entry directoryEntry(uint32_t blockIndex,uint16_t index, char buffer[MAX_NAME_LEN]);
//General
void indexToBlock(uint32_t index, uint32_t* block, uint16_t* offset);
struct ext2_inode getInode(int inodeNum);
uint32_t getSongStart(uint16_t songIndex);


//------------------------Public Functions--------------------------

//Returns the name of the song at |index|
//Variable size depending on entryName
void getSongName(uint16_t index, char buffer[MAX_NAME_LEN])
{
    struct ext2_dir_entry dirEntry = searchRoot(index, buffer);
    return;
}

//Given a song index, a buffer, and the number of times this function has been called
//On this song, fills the buffer with the next 256 bytes
//Returns the number of calls needed to get the full song
uint32_t fillBuffer(uint16_t songIndex, uint8_t buffer[BUFFER_SIZE], uint32_t callCount)
{
    uint32_t inodeNum = getSongStart(songIndex);
    uint32_t indirect, doubleIndirect;
    //Amount of data we've read from this file so far
    uint32_t readData = callCount * BUFFER_SIZE;
    //Determine which inode block we're in
    uint16_t inodeBlock = readData / BLOCK_SIZE;
    //And the offset within that block
    uint16_t inodeOffset;
    struct ext2_inode inode = getInode(inodeNum);

    uint32_t block;
    uint16_t offset;
    if(inodeBlock < DIRECT_BLOCKS)
    {
        inodeOffset = readData % BLOCK_SIZE;
        indexToBlock((BLOCK_SIZE * inode.i_block[inodeBlock]) + inodeOffset, &block, &offset);
    }
    else if(inodeBlock < DIRECT_BLOCKS + INDIRECT_BLOCKS)
    {
        //How far into the indirect block we need to look, 4 byte pointers
        inodeOffset = (inodeBlock - DIRECT_BLOCKS) * 4;

        //Indirect is block 12
        indexToBlock((BLOCK_SIZE * inode.i_block[12]) + inodeOffset, &block, &offset);
        //Get the pointer to the portion of the indirect block we care about
        sdReadData(block, offset, &indirect, 4);
        //Reset the offset to how far into the indirect block we should look
        inodeOffset = readData % BLOCK_SIZE;
        //Set up block and offset for the indirect block
        indexToBlock((BLOCK_SIZE * indirect) + inodeOffset, &block, &offset);
    }
    else
    {
        //How far into the double indirect block we need to look
        inodeOffset = ((inodeBlock - (DIRECT_BLOCKS + INDIRECT_BLOCKS)) / INDIRECT_BLOCKS) * 4;

        //Get the pointer to the portion of the double indirect block we care about
        indexToBlock((BLOCK_SIZE * inode.i_block[13]) + inodeOffset, &block, &offset);
        sdReadData(block, offset, &doubleIndirect, 4);

        //How far into the indirect block we need to look
        inodeOffset = ((inodeBlock - (DIRECT_BLOCKS + INDIRECT_BLOCKS)) % INDIRECT_BLOCKS) * 4;

        //Get the pointer to the portion of the indirect block we care about
        indexToBlock((BLOCK_SIZE * doubleIndirect) + inodeOffset, &block, &offset);
        sdReadData(block, offset, &indirect, 4);

        //Reset the offset to how far into the indirect block we should look
        inodeOffset = readData % BLOCK_SIZE;

        //Set up block and offset for the indirect block
        indexToBlock((BLOCK_SIZE * indirect) + inodeOffset, &block, &offset);
    }
    sdReadData(block, offset, buffer, BUFFER_SIZE);

    indirect = inode.i_size - (readData + BUFFER_SIZE);
    return indirect;
}

//------------------------------Search--------------------------------

//Returns the directory entry of the file at |index| in the root directory
//128 + 32 + 16 + 16
//24 bytes
struct ext2_dir_entry searchRoot(uint16_t index, char buffer[MAX_NAME_LEN])
{
    //Only need to go to the 1st block, all songs fit there
    return directoryEntry(getInode(ROOT_INODE).i_block[0], index, buffer);
}

//----------------------------Directory-------------------------------

//Returns the |index|th directory entry from the directory at |blockIndex|
//32 + 16 + 16 + 16 + 16 + 32 + 16 + 16 + 8*(100) + 8 + (64 + 8*len)
struct ext2_dir_entry directoryEntry(uint32_t blockIndex,uint16_t index, char buffer[MAX_NAME_LEN])
{
    uint16_t readData = 0;
    uint16_t currentIndex = 0;
    uint32_t block;
    uint16_t offset;
    uint16_t entrySize;
    uint8_t data[DIR_READ_SIZE];
    struct ext2_dir_entry* directory;
    struct ext2_dir_entry dirReturn;
    index += 3; //Account for . .. and lost+found entries
    //Get directory entry
    indexToBlock(BLOCK_SIZE * blockIndex, &block, &offset);
    sdReadData(block, offset, data, DIR_READ_SIZE);

    directory = (struct ext2_dir_entry*)data;
    while(readData < BLOCK_SIZE)
    {
        if(currentIndex == index)
            break;
        
        readData += directory->rec_len;

        indexToBlock((BLOCK_SIZE * blockIndex) + readData, &block, &offset);
        //Data to read can stradle a sector
        if(offset + DIR_READ_SIZE > SECTOR_SIZE)
        {
            entrySize = directory->rec_len;
             //Get the portion in this sector
            sdReadData(block, offset, data, DIR_READ_SIZE - offset);
            //Get the remaining portion
            sdReadData(block + 1, 0, data + DIR_READ_SIZE - offset, 
                    entrySize - (DIR_READ_SIZE - offset));
        }
        else
            sdReadData(block, offset, data, DIR_READ_SIZE);

        directory = (struct ext2_dir_entry*)data;
        currentIndex++;
    }
    memcpy(&dirReturn, directory, sizeof(struct ext2_dir_entry));
    //indexToBlock((BLOCK_SIZE * blockIndex) + readData + 8, &block, &offset); 
    //sdReadData(block, offset, buffer, dirReturn.name_len + 1);
    memcpy(buffer, directory->name, dirReturn.name_len + 1);
    buffer[dirReturn.name_len] = "\0";
    return dirReturn;
}

//------------------------------General-------------------------------

//Given a raw index to the file, converts it to block and offset needed for read_data
void indexToBlock(uint32_t index, uint32_t* block, uint16_t* offset)
{
    *offset = index % SECTOR_SIZE;
    *block = (index - *offset) / SECTOR_SIZE;

    return;
}

uint32_t getSongStart(uint16_t songIndex)
{
    char fakebuf[2];
    return searchRoot(songIndex, fakebuf).inode;
}

//Mallocs and returns a pointer to the inode at |inodeNum|
struct ext2_inode getInode(int inodeNum)
{
    struct ext2_inode inode;
    int blockGroup = (inodeNum - 1) / INODES_PER_GROUP;
    int localIndex = (inodeNum - 1) % INODES_PER_GROUP;
    uint32_t block;
    uint16_t offset;
    
    //raw index to seek within the file, first get to the right block, 
    //then go to that block groups inode table, and index inside
    uint32_t fileIndex = ((uint32_t)BLOCK_SIZE * ((blockGroup * BLOCKS_PER_GROUP) + INODE_TABLE_OFFSET))
     + (localIndex * sizeof(struct ext2_inode));

    //Convert raw index into block and offset
    indexToBlock(fileIndex, &block, &offset);

    sdReadData(block, offset, &inode, sizeof(struct ext2_inode));
    return inode;
}