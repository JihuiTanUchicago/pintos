#include "filesys/inode.h"
#include <bitmap.h>
#include <list.h>
#include <debug.h>
#include <round.h>
#include <stdio.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"

#include "userprog/syscall.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define DIRECT_CNT 123
#define INDIRECT_CNT 1
#define DBL_INDIRECT_CNT 1
#define SECTOR_CNT (DIRECT_CNT + INDIRECT_CNT + DBL_INDIRECT_CNT)

//one block has 8 sectors I believe? PTRS_PER_SECTOR = 512 bytes/4 bytes = 128 offsets. 
//128 = 123 + 1 + 1 + 3
#define PTRS_PER_SECTOR ((off_t) (BLOCK_SECTOR_SIZE / sizeof (block_sector_t)))
#define INODE_SPAN ((DIRECT_CNT                                              \
                     + PTRS_PER_SECTOR * INDIRECT_CNT                        \
                     + PTRS_PER_SECTOR * PTRS_PER_SECTOR * DBL_INDIRECT_CNT) \
                    * BLOCK_SECTOR_SIZE)



static void deallocate_inode (const struct inode *inode);

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t sectors[SECTOR_CNT]; /* Sectors. */
    enum inode_type type;               /* FILE_INODE or DIR_INODE. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    //if not opened, not used yet since not in open_inodes list. 
    struct list_elem elem;              /* Element in inode list. */
    //sector will be used to locate inode disk
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    //no two threads can modify an inode at the same time. 
    struct lock lock;                   /* Protects the inode. */

    /* Denying writes. */
    struct lock deny_write_lock;        /* Protects members below. */
    struct condition no_writers_cond;   /* Signaled when no writers. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    int writer_cnt;                     /* Number of writers. */
  };

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Controls access to open_inodes list. */
static struct lock open_inodes_lock;

/* Initializes the inode module. */
//DONE.
void
inode_init (void) 
{
  list_init (&open_inodes);
  lock_init (&open_inodes_lock);
}


/* Initializes an inode of the given TYPE, 

writes the new inode to sector SECTOR on the file system device, 
and returns the inode thus created.  

Returns a null pointer if unsuccessful, 
in which case SECTOR is released in the free map. */
//Done
struct inode *
inode_create(block_sector_t sector, enum inode_type type) {
  struct inode_disk *disk_inode;

  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc(1, sizeof *disk_inode);
  if (disk_inode == NULL) {
    free_map_release(sector);
    return NULL;
  }
  
  disk_inode->type = type;
  disk_inode->magic = INODE_MAGIC;
  disk_inode->length = 0;
  //the rest are zeros. 

  block_write(fs_device, sector, disk_inode);

  struct inode *mem_inode = inode_open(sector);

  free(disk_inode);

  if (mem_inode == NULL) {
    free_map_release(sector);
  }

  return mem_inode;
}



// Forward declaration of helper functions
static struct inode *find_open_inode(block_sector_t sector);
static struct inode *create_new_inode(block_sector_t sector);

/* Opens an inode from SECTOR and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails or sector is invalid. */
struct inode *inode_open(block_sector_t sector) {
  struct inode *inode;

  lock_acquire(&open_inodes_lock);
  
  // Check if the inode is already open
  inode = find_open_inode(sector);
  if (inode != NULL) {
    inode->open_cnt++;
    lock_release(&open_inodes_lock);
    return inode;
  }

  // Create a new inode as it is not already open
  inode = create_new_inode(sector);
  lock_release(&open_inodes_lock);

  return inode;
}

/* Searches for an open inode with the specified sector.
   Returns the inode if found, otherwise NULL. */
static struct inode *find_open_inode(block_sector_t sector) {
  struct list_elem *e;
  for (e = list_begin(&open_inodes); e != list_end(&open_inodes);
       e = list_next(e)) {
    struct inode *inode = list_entry(e, struct inode, elem);
    if (inode->sector == sector) {
      return inode;
    }
  }
  return NULL;
}

/* Creates a new inode for the specified sector.
   Returns the inode, or NULL if memory allocation fails. */
static struct inode *create_new_inode(block_sector_t sector) {
  struct inode *inode = malloc(sizeof(*inode));
  if (inode == NULL) {
    free_map_release(sector);
    return NULL;
  }

  // Initialize the new inode
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->removed = false;
  lock_init(&inode->lock);
  lock_init(&inode->deny_write_lock);
  cond_init(&inode->no_writers_cond);
  inode->deny_write_cnt = 0;
  inode->writer_cnt = 0;
  list_push_front(&open_inodes, &inode->elem);

  return inode;
}

/* Reopens and returns INODE. */
//DONE.
struct inode *
inode_reopen (struct inode *inode)
{
  // added the locks so that open_cnt is synchronized. 
  if (inode != NULL)
    {
      lock_acquire (&open_inodes_lock);
      inode->open_cnt++;
      lock_release (&open_inodes_lock);
    }
  return inode;
}

/* Returns the type of INODE. */
//DONE.
enum inode_type inode_get_type(const struct inode *inode) {
  ASSERT(inode != NULL);
  enum inode_type type;
  // Allocate space for disk_inode
  struct inode_disk *disk_inode = malloc(sizeof(struct inode_disk));
  // Read the inode_disk data from the inode
  block_read(fs_device, inode->sector, disk_inode);
  // Retrieve the type from the inode_disk
  type = disk_inode->type;
  // Free the allocated memory
  free(disk_inode);
  return type;
}


/* Returns INODE's inode number. */
//DONE.
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void inode_close(struct inode *inode) {
  if(inode == NULL){return;}

  lock_acquire(&open_inodes_lock);
  inode->open_cnt -= 1;
  if(inode->open_cnt > 0){
    lock_release(&open_inodes_lock);
  }else{
    list_remove(&inode->elem);
    if(inode->removed == true){
      deallocate_inode(inode);
    }
    lock_release(&open_inodes_lock);
    free(inode);
  }

}


/* Deallocates SECTOR and anything it points to recursively.
   LEVEL is 2 if SECTOR is doubly indirect,
   or 1 if SECTOR is indirect,
   or 0 if SECTOR is a data sector. */
//DONE
static void deallocate_recursive(block_sector_t sector, int level) {
  if (level == 2){
    //assign space
    uint32_t* doubly_indirect_map = malloc(BLOCK_SECTOR_SIZE);
    if(doubly_indirect_map == NULL) return;
    block_read (fs_device, sector, doubly_indirect_map);

    for (int i=0; i<PTRS_PER_SECTOR; i++){
      if (doubly_indirect_map[i]==0) break;
      // not zero, we do have an indirect map
      uint32_t* indirect_map = malloc(BLOCK_SECTOR_SIZE);
      block_read (fs_device, doubly_indirect_map[i], indirect_map);

      // about to enter into the indirect map.
      for (int j=0; j<PTRS_PER_SECTOR; j++){
        if (indirect_map[j]==0) {break;}
        // not zero, there is a sector. 
        free_map_release (indirect_map[j]);
      }

      free_map_release (doubly_indirect_map[i]);
      //finished using it, free the map. 
      free(indirect_map);
    }
    free_map_release (sector);
    //finished using the doubly_indirect_map. 
    free(doubly_indirect_map);
  }else if (level == 1){
    uint32_t* indirect_map = malloc(BLOCK_SECTOR_SIZE);
    block_read (fs_device, sector, indirect_map);
    // remember to zero out the sectors before using it leter. 
    for (int i=0; i<PTRS_PER_SECTOR; i++){
      if (indirect_map[i]==0) break;
      free_map_release (indirect_map[i]);
     
    }
    free_map_release (sector);
    //free the indirect_map after using it. 
    free(indirect_map);
  }else if (level == 0){
    // regular data sector
    free_map_release (sector);
  }
}


/* Deallocates the blocks allocated for INODE. */
//DONE
static void
deallocate_inode (const struct inode *inode)
{
  /// deallocate recursive ..
  // mainly go check the on disk things. 
  struct inode_disk *buffer = calloc(1, sizeof *buffer);
  block_read (fs_device, inode->sector, buffer);
  for (int i=0; i<DIRECT_CNT; i++){
    //if nothing there assigned, just return. 
    if (buffer->sectors[i]==0) {
      free (buffer);
      return;
    }
    deallocate_recursive(buffer->sectors[i], 0);
    // set back to zero. 
    buffer->sectors[i] = 0;
  }
  deallocate_recursive(buffer->sectors[DIRECT_CNT],1);
  deallocate_recursive(buffer->sectors[DIRECT_CNT+1],2);
  free_map_release (inode->sector);
  free (buffer);
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
//DONE.
void
inode_remove (struct inode *inode) 
{ 
  ASSERT (inode != NULL);
  lock_acquire(&inode->lock);
  inode->removed = true;
  lock_release(&inode->lock);
}

/* Translates SECTOR_IDX into a sequence of block indexes in
   OFFSETS and sets *OFFSET_CNT to the number of offsets. 
   offset_cnt can be 1 to 3 depending on whether sector_idx 
   points to sectors within DIRECT, INDIRECT, or DBL_INDIRECT ranges.
*/
//Done
static void
calculate_indices (off_t sector_idx, size_t offsets[], size_t *offset_cnt)
{

  if(sector_idx < 0){
    *offset_cnt = 0; // Indicate an error condition
  }
  if (sector_idx > INODE_SPAN){
     // Sector index out of range
    *offset_cnt = 0; // Indicate an error condition
  }
  
  if (sector_idx < DIRECT_CNT){
    /* Handle direct blocks. When sector_idx < DIRECT_CNT */
    // offset_cnt = 1, and offsets[0] = sector_idx
    *offset_cnt = 1;
    offsets[0] = sector_idx;
    return;
  }
  sector_idx -= DIRECT_CNT; //123

  if (sector_idx < PTRS_PER_SECTOR){
    //printf("inode.c,calculate_indices, indirect.\n");
    /* Handle indirect blocks. */
    // offset_cnt = 2, offsets[0] = DIRECT_CNT, offsets[1] ...
    *offset_cnt = 2;
    offsets[0] = DIRECT_CNT; //Index of the indirect block
    offsets[1] = sector_idx ; // Index within the indirect block
    return;
  }
  sector_idx -= PTRS_PER_SECTOR;//128
  
  if (sector_idx < (PTRS_PER_SECTOR*PTRS_PER_SECTOR)){
    //printf("inode.c,calculate_indices, double indirect.\n");
     /* Handle doubly indirect blocks. */
    // offset_cnt = 3, offsets[0] = DIRECT_CNT + INDIRECT_CNT, offsets[1], offsets[2] ...
    *offset_cnt = 3;
    offsets[0] = DIRECT_CNT + 1; //Index of the doubly indrect block
    offsets[1] = sector_idx / PTRS_PER_SECTOR; //Index of indirect block within doubly indirect block
    offsets[2] = sector_idx; //Index within the indirect block
    return;
  }
}

/* Retrieves the data block for the given byte OFFSET in INODE,
   setting *DATA_BLOCK to the block and data_sector to the sector to write 
   (for inode_write_at method).

   Returns true if successful, false on failure.

   If ALLOCATE is false (usually for inode read), 
   then missing blocks will be successful with *DATA_BLOCK set to a null pointer.

   If ALLOCATE is true (for inode write), then missing blocks will be allocated. 
   This method may be called in parallel */

static bool handle_direct_block(struct inode* inode, size_t block_index, bool allocate, void** data_block, block_sector_t* data_sector){
  // Allocate space for disk_inode
  
  struct inode_disk *disk_inode = malloc(sizeof(struct inode_disk));
  if(disk_inode == NULL){return false;}
  // Read the inode_disk data from the inode
  block_read(fs_device, inode->sector, disk_inode);

  // Retrieve sector
  block_sector_t sector = disk_inode->sectors[block_index];
  if(sector == 0 && allocate){//check if needed to allocate
    if(!free_map_allocate(&sector)){
      free(disk_inode);
      return false;
    }
    disk_inode->sectors[block_index] = sector; //Update since allocated
    
    void* new_block_data = calloc(1, BLOCK_SECTOR_SIZE);
    if(new_block_data == NULL){
      free(disk_inode);
      free_map_release(sector); 
      return false;
    } //Memory allocation failed
    block_write(fs_device, sector, new_block_data);
    free(new_block_data);
    block_write(fs_device, inode->sector, disk_inode);

  }

  *data_sector = sector; //Update the sector number for the caller
  //If sector is not zero, read data from the block
  if(sector != 0){
    *data_block = malloc(BLOCK_SECTOR_SIZE);
    if(*data_block == NULL){
      free(disk_inode);
      return false;
    }
    block_read(fs_device, sector, *data_block);
  }else{
    //return NULL in *data_block if not allocating
    *data_block = NULL;
  }

  free(disk_inode);
  return true;
}

static bool handle_indirect_block(struct inode* inode, size_t block_index, bool allocate, void** data_block, block_sector_t* data_sector){
  //printf("inode.c,handle_indirect_block.\n");
  size_t offsets[3];
  size_t offset_cnt;
  calculate_indices(block_index, offsets, &offset_cnt);
  size_t indirect_block_index = offsets[0];
  size_t direct_block_offset = offsets[1];

  //***First Level: Get indirect block index to retrieve indirect_block
  // Allocate space for disk_inode
  struct inode_disk *disk_inode = malloc(sizeof(struct inode_disk));
  if(disk_inode == NULL) {return false;}
  // Read the inode_disk data from the inode
  block_read(fs_device, inode->sector, disk_inode);
  // Retrieve indirect sector
  block_sector_t indirect_sector = disk_inode->sectors[indirect_block_index];
  if(indirect_sector == 0 && allocate){ //allocate if needed
    if(!free_map_allocate(&indirect_sector)){
      free(disk_inode);
      return false;
    }
    disk_inode->sectors[indirect_block_index] = indirect_sector; //Update indirect sector info
    void* new_block_data = calloc(1, BLOCK_SECTOR_SIZE);
    if(new_block_data == NULL){
      free(disk_inode);
      free_map_release(indirect_sector); 
      return false;
    } //Memory allocation failed
    block_write(fs_device, indirect_sector, new_block_data);
    free(new_block_data);
    block_write(fs_device, inode->sector, disk_inode);
  }
  free(disk_inode);

  //***Second Level: Retrieve indirect_block, and get direct block data
  //BELOW is similar logic in handle_direct_block, can consider replace
  struct inode_disk *disk_inode2 = malloc(sizeof(struct inode_disk));
  if(disk_inode2 == NULL){return false;}
  uint32_t *indirect_block = malloc(BLOCK_SECTOR_SIZE);
  if(indirect_block == NULL){
    free(disk_inode2);
    return false;
  }
  if(indirect_sector != 0){block_read(fs_device, indirect_sector, disk_inode2);}
  else{memset(indirect_block, 0, BLOCK_SECTOR_SIZE);}
 
  block_sector_t sector = disk_inode2->sectors[direct_block_offset];
  if(sector == 0 && allocate){ //allocate if needed
    if(!free_map_allocate(&sector)){
      free(disk_inode2);
      free(indirect_block);
      return false;
    }
    disk_inode2->sectors[direct_block_offset] = sector; //Update direct sector info 
    void* new_block_data2 = calloc(1, BLOCK_SECTOR_SIZE);
    if(new_block_data2 == NULL){
      free(disk_inode2);
      free_map_release(sector); 
      return false;
    } //Memory allocation failed
    block_write(fs_device, sector, new_block_data2);
    free(new_block_data2);
    block_write(fs_device, indirect_sector, disk_inode2);
  }

  *data_sector = sector; //Update the sector number for the caller
  if(sector != 0){
    *data_block = malloc(BLOCK_SECTOR_SIZE);
    if(*data_block == NULL){
      free(disk_inode2);
      free(indirect_block);
      return false;
    }
    block_read(fs_device, sector, *data_block);
  }else{
    *data_block = NULL;
  }

  free(disk_inode2);
  free(indirect_block);
  return true;
}

static bool handle_doubly_indirect_block(struct inode* inode, size_t block_index,
 bool allocate, void** data_block, block_sector_t* data_sector){
  //printf("inode.c,handle_doubly_indirect_block.\n");
  size_t offsets[3];
  size_t offset_cnt;
  calculate_indices(block_index, offsets, &offset_cnt);
  size_t dbl_indirect_block_index = offsets[0];
  size_t indirect_block_index = offsets[1];
  size_t direct_block_offset = offsets[2];

  //***First Level: Get dbl indirect block index to retrieve dbl indirect_block
  // Allocate space for disk_inode
  struct inode_disk *disk_inode = malloc(sizeof(struct inode_disk));
  if(disk_inode == NULL){return false;}
  // Read the inode_disk data from the inode
  block_read(fs_device, inode->sector, disk_inode);
  // Retrieve dbl indirect sector
  block_sector_t dbl_indirect_sector = disk_inode->sectors[dbl_indirect_block_index];
  if(dbl_indirect_sector == 0 && allocate){ //allocate if needed
    if(!free_map_allocate(&dbl_indirect_sector)){
      free(disk_inode);
      return false;
    }
    disk_inode->sectors[dbl_indirect_block_index] = dbl_indirect_sector; //Update indirect sector info
    void* new_block_data = calloc(1, BLOCK_SECTOR_SIZE);
    if(new_block_data == NULL){
      free(disk_inode);
      free_map_release(dbl_indirect_sector); 
      return false;
    } //Memory allocation failed
    block_write(fs_device, dbl_indirect_sector, new_block_data);
    free(new_block_data);
    block_write(fs_device, inode->sector, disk_inode);
  }
  free(disk_inode);

  //BELOW is similar logic in handle_indirect_block, can consider replace by calling that function
  //***Second Level: Get indirect block index to retrieve indirect_block
  // Allocate space for disk_inode
  struct inode_disk *disk_inode2 = malloc(sizeof(struct inode_disk));
  if(disk_inode2 == NULL){return false;}
  uint32_t *indirect_block = malloc(BLOCK_SECTOR_SIZE);
  if(indirect_block == NULL){return false;}
  if(dbl_indirect_sector != 0){block_read(fs_device, dbl_indirect_sector, disk_inode2);}
  else{memset(indirect_block, 0, BLOCK_SECTOR_SIZE);}

  // Retrieve dbl indirect sector
  block_sector_t indirect_sector = disk_inode2->sectors[indirect_block_index];
  if(indirect_sector == 0 && allocate){
    if(!free_map_allocate(&indirect_sector)){
      free(disk_inode2);
      free(indirect_block);
      return false;
    }
    disk_inode2->sectors[indirect_block_index] = indirect_sector; //Update indirect sector info
    void* new_block_data2 = calloc(1, BLOCK_SECTOR_SIZE);
    if(new_block_data2 == NULL){
      free(disk_inode2);
      free_map_release(indirect_sector); 
      return false;
    } //Memory allocation failed
    block_write(fs_device, indirect_sector, new_block_data2);
    free(new_block_data2);
    block_write(fs_device, dbl_indirect_sector, disk_inode2);
  }
  free(disk_inode2);

  //***Third Level: Get direct block index to retrieve direct_block
  struct inode_disk *disk_inode3 = malloc(sizeof(struct inode_disk));
  uint32_t *direct_block = malloc(BLOCK_SECTOR_SIZE);
  if(direct_block == NULL){return false;}
  if(dbl_indirect_sector != 0 && indirect_sector != 0){
    block_read(fs_device, indirect_sector, disk_inode3);
  }else{
    memset(direct_block, 0, BLOCK_SECTOR_SIZE); //if direct_block has previously not been allocated
  }

  block_sector_t sector = disk_inode3->sectors[direct_block_offset];
  if(sector == 0 && allocate){ //allocate if needed
    if(!free_map_allocate(&sector)){
      free(disk_inode3);
      free(direct_block);
      return false;
    }
    disk_inode3->sectors[direct_block_offset] = sector; //Update direct sector info 
    void* new_block_data3 = calloc(1, BLOCK_SECTOR_SIZE);
    if(new_block_data3 == NULL){
      free(disk_inode3);
      free_map_release(sector); 
      return false;
    } //Memory allocation failed
    block_write(fs_device, sector, new_block_data3);
    free(new_block_data3);
    block_write(fs_device, indirect_sector, disk_inode3);
  }

  *data_sector = sector; //Update the sector number for the caller
  if(sector != 0){
    *data_block = malloc(BLOCK_SECTOR_SIZE);
    if(*data_block == NULL){
      free(disk_inode3);
      free(direct_block);
      return false;
    }
    block_read(fs_device, sector, *data_block);
  }else{
    *data_block = NULL;
  }

  free(disk_inode3);
  free(direct_block);
  return true;
}
static bool
get_data_block (struct inode *inode, off_t offset, bool allocate,
                void **data_block, block_sector_t *data_sector)
{
  ASSERT(inode != NULL);
  ASSERT(offset >= 0);

  size_t block_index = offset/BLOCK_SECTOR_SIZE;
  size_t direct_block_limit = DIRECT_CNT;
  size_t indirect_block_limit = DIRECT_CNT + PTRS_PER_SECTOR * INDIRECT_CNT;
  size_t doubly_indirect_block_limit = indirect_block_limit + PTRS_PER_SECTOR * PTRS_PER_SECTOR * INDIRECT_CNT;

  if(block_index < direct_block_limit){
    return handle_direct_block(inode, block_index, allocate, data_block, data_sector);
  }
  
  else if(block_index < indirect_block_limit){
    return handle_indirect_block(inode, block_index, allocate, data_block, data_sector);
  }
  
  else if(block_index < doubly_indirect_block_limit){
      return handle_doubly_indirect_block(inode, block_index, allocate,data_block,data_sector);
  }
  
  else{
    return false; //Block index out oof range
  }
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. 
   Some modifications might be needed for this function template. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{

   uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  block_sector_t target_sector = 0; // not really useful for inode_read

  while (size > 0)
    {
      /* Sector to read, starting byte offset within sector, sector data. */
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;
      void *block; // may need to be allocated in get_data_block method,
                   // and don't forget to free it in the end

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0 || !get_data_block (inode, offset, false, &block, &target_sector))
        break;

      if (block == NULL)
        memset (buffer + bytes_read, 0, chunk_size);
      else
        {
          memcpy (buffer + bytes_read, block + sector_ofs, chunk_size);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;

      free(block);
    }

  return bytes_read;
}

/* Extends INODE to be at least LENGTH bytes long. */
//Done
static void update_inode_length(struct inode *inode, off_t new_length);
static void extend_file(struct inode *inode, off_t length) {
  // Check if the inode length is already sufficient
  if (inode_length(inode) >= length) {
    return; // No extension needed
  }

  off_t current_length = inode_length(inode);
  while (current_length < length) {
    // Calculate the sector index for the next block to allocate
    off_t sector_idx = bytes_to_sectors(current_length);

    // Allocate the next block if necessary
    block_sector_t sector;
    void *dummy_block; // Placeholder for get_data_block

    if (!get_data_block(inode, sector_idx * BLOCK_SECTOR_SIZE, true, &dummy_block, &sector)) {
      break; // Break on allocation failure
    }

    // Free the dummy block allocated by get_data_block
    free(dummy_block); // Safe to call free on NULL

    // Advance the length to the end of the newly allocated block
    current_length = ((sector_idx + 1) * BLOCK_SECTOR_SIZE > length) ? length : (sector_idx + 1) * BLOCK_SECTOR_SIZE;
  }

  // Update the inode length if it has been extended
  update_inode_length(inode, current_length);
}

static void update_inode_length(struct inode *inode, off_t new_length) {
  if (new_length > inode_length(inode)) {
    struct inode_disk disk_inode;
    block_read(fs_device, inode->sector, &disk_inode);
    disk_inode.length = new_length;
    block_write(fs_device, inode->sector, &disk_inode);
  }
}


/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if an error occurs. 
   Some modifications might be needed for this function template.*/
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{

  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  block_sector_t target_sector = 0;

  /* Don't write if writes are denied. */
  lock_acquire (&inode->deny_write_lock);
  if (inode->deny_write_cnt)
    {
      lock_release (&inode->deny_write_lock);
      return 0;
    }
  inode->writer_cnt++;
  lock_release (&inode->deny_write_lock);

  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector, sector data. */
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;
      void *block; // may need to be allocated in get_data_block method,
                   // and don't forget to free it in the end

      /* Bytes to max inode size, bytes left in sector, lesser of the two. */
      off_t inode_left = INODE_SPAN - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
 
      if (chunk_size <= 0 || !get_data_block (inode, offset, true, &block, &target_sector)){
        break;
      }

      memcpy (block + sector_ofs, buffer + bytes_written, chunk_size);
      block_write(fs_device, target_sector, block);

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
      free(block);
    }

  extend_file (inode, offset);

  lock_acquire (&inode->deny_write_lock);
  if (--inode->writer_cnt == 0)
    cond_signal (&inode->no_writers_cond, &inode->deny_write_lock);
  lock_release (&inode->deny_write_lock);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
//DONE.
void
inode_deny_write (struct inode *inode) 
{ 
  inode->deny_write_cnt++;
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
//DONE.
void
inode_allow_write (struct inode *inode) 
{ 
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
//DONE
off_t
inode_length (const struct inode *inode)
{
  struct inode_disk *buffer = calloc(1, sizeof *buffer);
  block_read (fs_device, inode->sector, buffer);
  int length = buffer->length;

  free (buffer);
  return length;
}

/* Returns the number of openers. */
//DONE.
int
inode_open_cnt (const struct inode *inode)
{ 
  int open_cnt;

  lock_acquire (&open_inodes_lock);
  open_cnt = inode->open_cnt;
  lock_release (&open_inodes_lock);

  return open_cnt;
}

/* Locks INODE. */
//DONE.
void
inode_lock (struct inode *inode)
{ 
  lock_acquire (&inode->lock);
}

/* Releases INODE's lock. */
//DONE. 
void
inode_unlock (struct inode *inode)
{
  lock_release (&inode->lock);
}