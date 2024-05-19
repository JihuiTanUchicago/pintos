=== Data Structures ===

struct thread now has an additional attribute called "cwd". This is to support filesystem with subdirectories, so that each thread knows under what directory it is running. 

struct file_descriptor now has an additional attribute called "dir," since now we have to deal with both file and directory when we open or close them.

=== Algorithms ===

In Syscall:

- Previous syscalls are modified when there involves dealing with files. Added code to handle the case for handling directories and conditions to distinguish whether we are dealing with files or directories as well.

-chdir: wrapper function for filesys_chdir, nothing worth noted
-readdir: wrapper function for readdir, nothing worth noted except with have to use put_user to push the name into user space.
-mkdir: as simple as filesys_create, nothing worth noted
-isinumber: just look up the corresponding file descriptor and obtained the inode attribute
-isdir: used helper function get_dir_by_fd. If returnning NULL, that means it is a file and vice versa.



In Inode:

- inode_create: allocate a new inode in memory, initialize with requried attributes, and write it to disk
- inode_open: obtain inode via inode_disk though calling block_read from given sector number
- inode_get_type: similarly, obtain indoe_disk and get its attribute type to determine whether it is FILE_INODE or DIR_INODE
- inode_close: decrement inode's open count, then if ==0 then deallocate since it's no longer needed; 
- deallocate recursive & deallocate_inode, basically involves obtaining sectors that the inode occupies(in direct, indirect, and doubly indirect blocks), and deallocate them recursively using free_map_release
- calculate_indices: nothing much worth noted except some basic mathematical calculations needed
- get_data_block: based on calculate_indices, retrieve the data block from disk or allocate a new block if it doesn't exist if asked to.
- extend_file: based on length, recursively allocate new block until it covers the length
- inode_length: return length from struct attribute in corresponding inode_disk
- inode_deny_write/inode_allow_write: basic manipulations with deny-write count. ++ for deny and -- for allow.

In file:

- file_create(): do the creation via inode_create(), and inode_write_at for additional blocks(extend_file) according to length

In directory: 

- dir_create(): do the creation via inode_create() then record "." and ".." entries via inode_write_at
- dir_remove(): lookup the directory to be removed, and check if the directory is empty or in use. then safely remove via inode_remove and inode_close
- dir_readdir: read the entries in a directory sequentially and check if it's in use. Conotinue until reach the end of the directory(return false) or find a in-use valid entry after "." and ".."(return true)

In filesystem:

- filesys_create: Allocate a free sector in the filesys's freemap upon successful name resolution. Create via file_Create or dir_create depending on inode_type. Then a new entry would be added to the parent directory.
- filesys_open: abstraction of resolve_name_to_inode, which would involve name resolution and looking up the file in the resolved dir entry.
- filesys_remove:call resolve_name_to_entry to get the file name and the dir it is under. Then call dir_remove
- filesys_chdir: basically changing thread_current()->cwd to resolved dir, obtained from dir_open (resolve_name_to_inode (name))
