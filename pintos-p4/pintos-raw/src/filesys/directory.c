#include "filesys/directory.h"


/* Creates a directory in the given SECTOR with its parent in PARENT_SECTOR.
   Returns inode of created directory if successful, null pointer on failure.
   On failure, SECTOR is released in the free map. */

static bool initialize_directory_entries(struct inode *inode, block_sector_t sector, block_sector_t parent_sector);
static struct inode *create_and_initialize_inode(block_sector_t sector, block_sector_t parent_sector);

struct inode *dir_create(block_sector_t sector, block_sector_t parent_sector) {
  struct inode *inode = create_and_initialize_inode(sector, parent_sector);
  if(inode == NULL){free_map_release(sector);}
  return inode;
}

/* Creates an inode and initializes directory entries "." and "..".
   Returns the inode if successful, null pointer on failure. */
static struct inode *create_and_initialize_inode(block_sector_t sector, block_sector_t parent_sector) {
  struct inode *inode = inode_create(sector, DIR_INODE);
  if (inode != NULL && !initialize_directory_entries(inode, sector, parent_sector)) {
    inode_remove(inode);
    inode_close(inode);
    inode = NULL;
  }
  return inode;
}

/* Initializes directory entries "." and ".." for a given inode.
   Returns true on success, false on failure. */
static bool initialize_directory_entries(struct inode *inode, block_sector_t sector, block_sector_t parent_sector) {
  struct dir_entry entry_arr[2] = {0};
  // Initialize "." and ".." entry
  for(int i = 0; i < 2; i++){
    strlcpy(entry_arr[i].name, ".", sizeof entry_arr[i].name);
    entry_arr[i].inode_sector = sector;
    entry_arr[i].in_use = true;
  }
  // Write entry_arr to inode
  return inode_write_at(inode, entry_arr, sizeof entry_arr, 0) == sizeof entry_arr;
}


/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL && inode_get_type(inode) == DIR_INODE)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      //printf("directory.c, inode != NULL && dir != NULL && inode_get_type(inode) == DIR_INODE.\n");
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{

  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (e.in_use && !strcmp (name, e.name)) 
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }

  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  inode_lock(dir->inode);
  bool ok = lookup(dir, name, &e, NULL);
  inode_unlock(dir->inode);

  *inode = ok ? inode_open(e.inode_sector) : NULL;
  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX || strchr (name, '/') )
    return false;

  /* Check that NAME is not in use. */
  inode_lock(dir->inode);
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  inode_unlock(dir->inode);
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
/* Check if a directory inode is removable (not in use and empty). */
static bool is_removable_directory(struct inode *inode) {
  if (inode_open_cnt(inode) > 1) {
    return false;
  }

  struct dir_entry scan_entry;
  off_t scan_offset = 0;
  int in_use_count = 0;

  while (inode_read_at(inode, &scan_entry, sizeof scan_entry, scan_offset) == sizeof scan_entry) {
    scan_offset += sizeof scan_entry;
    if (scan_entry.in_use) {
      in_use_count++;
    }
  }

  return in_use_count <= 2; // Directory is empty if it has only "." and ".." entries
}

bool dir_remove(struct dir *dir, const char *name) {
  struct dir_entry e;
  struct inode *inode = NULL;
  off_t ofs;
  bool success = false;

  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  // Reject removal of "." and ".."
  if (!strcmp(name, ".") || !strcmp(name, "..")) {
    return false;
  }

  inode_lock(dir->inode);

  if (!lookup(dir, name, &e, &ofs)) {
    // Entry not found
    inode_unlock(dir->inode);
    return false;
  }

  inode = inode_open(e.inode_sector);
  if (inode == NULL) {
    inode_unlock(dir->inode);
    return false;
  }

  if (inode_get_type(inode) == DIR_INODE && !is_removable_directory(inode)) {
    inode_unlock(dir->inode);
    inode_close(inode);
    return false;
  }

  // Erase directory entry
  e.in_use = false;
  if (inode_write_at(dir->inode, &e, sizeof e, ofs) == sizeof e) {
    inode_remove(inode);
    success = true;
  }

  inode_unlock(dir->inode);
  inode_close(inode);
  return success;
}


/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */

/* Checks if a directory entry name is valid (not "." or ".."). */
static bool is_valid_entry(const char *entry_name) {
  return strcmp(entry_name, ".") != 0 && strcmp(entry_name, "..") != 0;
}

bool dir_readdir(struct dir *dir, char name[NAME_MAX + 1]) {
  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  inode_lock(dir->inode);

  struct dir_entry entry;
  bool is_entry_found = false;
  while (!is_entry_found && inode_read_at(dir->inode, &entry, sizeof entry, dir->pos) == sizeof entry) {
    dir->pos += sizeof entry;
    if (entry.in_use && is_valid_entry(entry.name)) {
      strlcpy(name, entry.name, NAME_MAX + 1);
      is_entry_found = true;
    }
  }

  inode_unlock(dir->inode);
  return is_entry_found;
}

