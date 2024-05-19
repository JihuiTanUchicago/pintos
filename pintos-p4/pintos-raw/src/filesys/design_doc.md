### Data Structures

`struct thread` now has an additional attribute called `cwd`. This is to support the filesystem with subdirectories, so that each thread knows under what directory it is running.

`struct file_descriptor` now has an additional attribute called `dir`, since now we have to deal with both files and directories when we open or close them.

### Algorithms

#### In Syscall:

- Previous syscalls are modified when dealing with files. Added code to handle the case for directories and conditions to distinguish between files and directories as well.
- `chdir`: Wrapper function for `filesys_chdir`, nothing worth noting.
- `readdir`: Wrapper function for `readdir`, nothing worth noting except we have to use `put_user` to push the name into user space.
- `mkdir`: As simple as `filesys_create`, nothing worth noting.
- `isinumber`: Just look up the corresponding file descriptor and obtain the inode attribute.
- `isdir`: Used helper function `get_dir_by_fd`. If returning NULL, it means it is a file and vice versa.

#### In Inode:

- `inode_create`: Allocate a new inode in memory, initialize with required attributes, and write it to disk.
- `inode_open`: Obtain inode via `inode_disk` through calling `block_read` from the given sector number.
- `inode_get_type`: Similarly, obtain `inode_disk` and get its attribute type to determine whether it is `FILE_INODE` or `DIR_INODE`.
- `inode_close`: Decrement inode's open count, then if it is zero, deallocate it since it's no longer needed.
- `deallocate_recursive` & `deallocate_inode`: Involves obtaining sectors that the inode occupies (in direct, indirect, and doubly indirect blocks), and deallocate them recursively using `free_map_release`.
- `calculate_indices`: Basic mathematical calculations needed.
- `get_data_block`: Based on `calculate_indices`, retrieve the data block from disk or allocate a new block if it doesn't exist.
- `extend_file`: Based on length, recursively allocate new blocks until it covers the length.
- `inode_length`: Return length from struct attribute in corresponding `inode_disk`.
- `inode_deny_write`/`inode_allow_write`: Basic manipulations with deny-write count. Increment for deny and decrement for allow.

#### In File:

- `file_create()`: Create via `inode_create()`, and `inode_write_at` for additional blocks (`extend_file`) according to length.

#### In Directory:

- `dir_create()`: Create via `inode_create()` then record "." and ".." entries via `inode_write_at`.
- `dir_remove()`: Lookup the directory to be removed, check if the directory is empty or in use, then safely remove via `inode_remove` and `inode_close`.
- `dir_readdir`: Read the entries in a directory sequentially and check if it's in use. Continue until reaching the end of the directory (return false) or find an in-use valid entry after "." and ".." (return true).

#### In Filesystem:

- `filesys_create`: Allocate a free sector in the filesystem's freemap upon successful name resolution. Create via `file_create` or `dir_create` depending on `inode_type`. Then a new entry would be added to the parent directory.
- `filesys_open`: Abstraction of `resolve_name_to_inode`, which involves name resolution and looking up the file in the resolved dir entry.
- `filesys_remove`: Call `resolve_name_to_entry` to get the file name and the directory it is under, then call `dir_remove`.
- `filesys_chdir`: Change `thread_current()->cwd` to the resolved directory, obtained from `dir_open (resolve_name_to_inode (name))`.
