// fs.h: File System

#pragma once

#include "sfs/disk.h"

#ifdef __APPLE__
#include <sys/types.h>
#endif

#include <cassert>
#include <cstdint>
#include <functional>
#include <vector>


class FileSystem {
public:
  const static uint32_t MAGIC_NUMBER = 0xf0f03410;
  const static uint32_t INODES_PER_BLOCK = 128;
  const static uint32_t POINTERS_PER_INODE = 5;
  const static uint32_t POINTERS_PER_BLOCK = 1024;

private:
  struct SuperBlock {     // Superblock structure
    uint32_t MagicNumber; // File system magic number
    uint32_t Blocks;      // Number of blocks in file system
    uint32_t InodeBlocks; // Number of blocks reserved for inodes
    uint32_t Inodes;      // Number of inodes in file system
  };

  struct Inode {
    uint32_t Valid;                      // Whether or not inode is valid
    uint32_t Size;                       // Size of file
    uint32_t Direct[POINTERS_PER_INODE]; // Direct pointers
    uint32_t Indirect;                   // Indirect pointer
  };

  union Block {
    SuperBlock Super;                      // Superblock
    Inode Inodes[INODES_PER_BLOCK];        // Inode block
    uint32_t Pointers[POINTERS_PER_BLOCK]; // Pointer block
    char Data[Disk::BLOCK_SIZE];           // Data block
  };

  // TODO: Internal helper functions
  Disk *getDisk() const { return disk; }
  
  static SuperBlock getSuperblock(Disk *disk) {
    Block block;
    disk->read(0, block.Data);
    return block.Super;
  }

  SuperBlock getSuperblock() const {
    return getSuperblock(disk);
  }

  uint32_t getInodeBlkIndex(uint32_t inumber) const {
    return inumber / INODES_PER_BLOCK + 1;
  }

  Inode &getInode(uint32_t inumber, Block &inodeBlock) const {
    uint32_t inodeBlkIndex = getInodeBlkIndex(inumber);
    uint32_t offset = inumber % INODES_PER_BLOCK;
    disk->read(inodeBlkIndex, inodeBlock.Data);
    return inodeBlock.Inodes[offset];
  }

  /// return the disk block index for a given inode block index
  uint32_t getDiskBlkNo_direct(const Inode &inode, uint32_t blockIndex) {
    assert(blockIndex < 5);
    return inode.Direct[blockIndex];
  }

  uint32_t getDiskBlkNo_indirect(const uint32_t (&pointers)[1024], uint32_t blockIndex) {
    assert(blockIndex >= 5);
    return pointers[blockIndex - 5];
  }

  /// helper function combining the above two
  uint32_t getDiskBlkNo(const Inode &inode, uint32_t blockIndex, Block &indirectBlk) {
    uint32_t diskBlkNo;
    if (blockIndex < 5) {
      diskBlkNo = getDiskBlkNo_direct(inode, blockIndex);
    } else {
      disk->read(inode.Indirect, indirectBlk.Data);
      diskBlkNo = getDiskBlkNo_indirect(indirectBlk.Pointers, blockIndex);
    }
    return diskBlkNo;
  }

  /// alocate one free block and make them not free
  ssize_t allocateBlock() {
    for (std::size_t i = 1; i < freeBlocks.size(); ++i) {
      if (freeBlocks[i]) {
        freeBlocks[i] = false;
        return i;
      }
    }
    return -1;
  }

  uint32_t blockCount(const Inode &inode) const {
    return (inode.Size + Disk::BLOCK_SIZE - 1) / Disk::BLOCK_SIZE;
  }

  /// make `index` to be a free block
  void reclaimBlock(uint32_t index) {
    freeBlocks[index] = true;
  }

  ssize_t allocateBlockForInode(Inode &inode);

  void initFreeBlocks_forInodeBlock(const Inode (&inodes)[INODES_PER_BLOCK]);

  // TODO: Internal member variables
  Disk *disk = nullptr;
  // Bitmap for freeblocks, true indicating free
  std::vector<bool> freeBlocks;

public:
  static void debug(Disk *disk);
  static bool format(Disk *disk);

  bool mount(Disk *disk);

  ssize_t create();
  bool remove(size_t inumber);
  ssize_t stat(size_t inumber);

  ssize_t read(size_t inumber, char *data, size_t length, size_t offset);
  ssize_t write(size_t inumber, char *data, size_t length, size_t offset);
};
