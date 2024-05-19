#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();
  //above are ok. 

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Extracts a file name part from *SRCP into PART,
and updates *SRCP so that the next call will return the next
file name part.
Returns 1 if successful, 0 at end of string, -1 for a too-long
file name part. */
static int
get_next_part (char part[NAME_MAX], const char **srcp)
{
  const char *src = *srcp;
  char *dst = part;
  /* Skip leading slashes.
  If it's all slashes, we're done. */
  while (*src == '/')
    src++;
  if (*src == '\0')
    return 0;
  /* Copy up to NAME_MAX character from SRC to DST.
  Add null terminator. */
  while (*src != '/' && *src != '\0')
  {
    if (dst < part + NAME_MAX)
      *dst++ = *src;
    else
      return -1;
    src++;
  }
  *dst = '\0';
  /* Advance source pointer. */
  *srcp = src;
  return 1;
}


/* Resolves relative or absolute file NAME.
   Returns true if successful, false on failure.
   Stores the directory corresponding to the name into *DIRP,
   and the file name part into BASE_NAME. */
static bool resolve_name_to_entry(const char* name, struct dir **dirp, char base_name[NAME_MAX + 1]) {
  struct dir *dir = NULL;
  struct inode *inode = NULL;
  const char *cp = name;
  char part[NAME_MAX + 1];
  bool success = false;

  // Find starting directory
  if (name[0] == '/' || thread_current()->cwd == NULL) {
    dir = dir_open_root();
  } else {
    dir = dir_reopen(thread_current()->cwd);
  }

  // Check if starting directory is valid
  if (dir == NULL) {
    return false;
  }

  // Get first name part
  if (get_next_part(part, &cp) <= 0) {
    dir_close(dir);
    return false;
  }

  // Traverse down directories
  while (true) {
    int ok = get_next_part(base_name, &cp);
    if (ok <= 0) {
      success = ok == 0;
      break;
    }

    if (!dir_lookup(dir, part, &inode)) {
      break;
    }

    dir_close(dir);
    dir = dir_open(inode);
    if (dir == NULL) {
      break;
    }

    strlcpy(part, base_name, NAME_MAX + 1);
  }

  // Handle results or cleanup on failure
  if (success) {
    *dirp = dir;
    strlcpy(base_name, part, NAME_MAX + 1);
  } else {
    dir_close(dir);
    *dirp = NULL;
    base_name[0] = '\0';
  }

  return success;
}


/* Resolves relative or absolute file NAME to an inode.
   Returns an inode if successful, or a null pointer on failure.
   The caller is responsible for closing the returned inode. */

/* Checks if the provided name refers to the root directory. */
static bool is_root_directory(const char *name) {
  return name[0] == '/' && name[strspn(name, "/")] == '\0';
}

static struct inode *resolve_name_to_inode(const char *name) {
  // Check for root directory case
  if (is_root_directory(name)) {
    return inode_open(ROOT_DIR_SECTOR);
  }

  // Resolve name to directory entry
  struct dir *dir;
  char entry[NAME_MAX + 1];
  if (!resolve_name_to_entry(name, &dir, entry)) {
    return NULL; // Resolution failed
  }

  // Look up inode in the resolved directory
  struct inode *inode;
  dir_lookup(dir, entry, &inode);
  dir_close(dir);
  return inode;
}



bool filesys_create(const char *name, off_t initial_size, enum inode_type type) {
  char entry[NAME_MAX + 1];
  block_sector_t inode_sector = 0;
  struct dir *dir = NULL;
  struct inode *inode;

  bool success = resolve_name_to_entry(name, &dir, entry);
  if (success) {
    success = free_map_allocate(&inode_sector);
  }

  if (success) {
    inode = (type == FILE_INODE) ? file_create(inode_sector, initial_size)
                                 : dir_create(inode_sector, inode_get_inumber(dir_get_inode(dir)));

    if (inode != NULL) {
      success = dir_add(dir, entry, inode_sector);
      if (!success) {
        inode_remove(inode);
      }
      inode_close(inode);
    } else {
      success = false;
      free_map_release(inode_sector);  // Release allocated sector on failure
    }
  }

  if (dir) {
    dir_close(dir);  // Close directory regardless of success
  }

  return success;
}


/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct inode *
filesys_open (const char *name)
{
  if(name[0] == '\0'){
    return NULL;
  }

  return resolve_name_to_inode (name);
}


/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir;
  char entry[NAME_MAX + 1];
  if (!resolve_name_to_entry (name, &dir, entry)) 
    {
      return false;
    }
    bool ok = dir_remove (dir, entry);
    dir_close (dir);
    return ok;
}

/* Change current directory to NAME.
   Return true if successful, false on failure. */
bool
filesys_chdir (const char *name) 
{
  struct dir *dir = dir_open (resolve_name_to_inode (name));
  if (dir != NULL){
      struct dir* temp = thread_current()->cwd;
      thread_current ()->cwd = dir;
      dir_close (temp);
      return true;
    }
  else
    return false;
}

/* Formats the file system. */
static void
do_format (void)
{
  struct inode *inode;
  printf ("Formatting file system...");

  /* Set up free map. */
  free_map_create ();

  /* Set up root directory. */
  inode = dir_create (ROOT_DIR_SECTOR, ROOT_DIR_SECTOR);

  if (inode == NULL)
    PANIC ("root directory creation failed");
  inode_close (inode);

  free_map_close ();

  printf ("done.\n");
}