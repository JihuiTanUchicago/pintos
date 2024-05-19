# Data Structures

## Frames
- struct frame *frames;
  - A list of frames that can manage the physical memory space. Initialized by fram_init. 
  - In each frame, we have three things: base (physical address), lock (used to lock the frame), and page (pointer to the page that's owning the frame). 

- struct lock scan_lock;
  - scan_lock is used when we are looking for a frame for a page. We don't want to have concurrency issue and have two pages lock the same frame the same time. 

- size_t hand;
  - hand is used to implement the clock algorithm. In our case, we think it's better to use clock algorithm and not to implement another struct. 

## Thread
- struct hash* pages;
  - hash table the hashes the pages. Much easier to check the information about a page struct. 
- void* user_stack_pointer;
  - used to handle stakc growth. 

## Page
- struct frame* frame: record a pointer to the frame. 
- struct hash_elem hash_elem: hash_elem, used to connect to struct hash* pages in thread.h
- void *addr: Virtual address.
- struct thread* thread: owner thread of the page
- bool read_only: tell me if I can write to the page. 
- bool swap_or_file: when page allocate, this is !read_only, meaning if it is read_only, I now only have it go to file. 
- struct file* exec_file: executable file, obtained from load. 
- off_t offset: where I'm in file. 
- off_t bytes: how much I take from file. 
- block_sector_t b_s: index to swap. 

## Swap
- swap_in: some sanity check followed by getting the page's starting block sector number b_s. Then simply do block_read and load data from disk to memory by for loop over block sectors, totaling size equivalent to a PGSIZE.
- swap_out: similar logic, but this time we need to get non-occupied sectors in disk(just like finding free frames) and then do swap out by loading data from memory to block sectors, update related attributes(b_s) in the page to record that if page_fault and need swap in, the data corresponding to page starts at b_s in disk.

# Algorithms

## Clock algorithm
 - By using hand, we go around all the frames twice and set access bit to 0 as we around. The details were instructed in lecture. We will eventually find a frame whoes access bit is already 0 and at this point, we can release the scan lock because we already found it. What we do is to take the victim page and call page_out on it. 

## Stack
 - Since now everything must be page_allocate instead of palloc, that's what we do. In process.c, we first allocate pages then find them some frames. After that, we install page to make sure they are on page_dir by install_page. These are essentially the same as P2 but we need to implement basics such as pages and frames to make them work. 

## Timer
 - Already inplemented in P1, exactly the same. 

## Frame
 - First we do a regular check see if there are frames that ain't locked. Simply lock that and return is fine. 
 - After we go around and don't find any, we start out clock algorithm. The algorithm is explained in data structure a little bit, which goes around all the frames twice. 
 - frame_lock: locks the frame that the page holds, must check if held by current lock
 - frame_unlock: same logic as frame_lock
 - frame_alloc_and_lock: we attempted to find and evict a frame 2 times; the first time we merely find a free frame not locked by others. The second time we used clock algorithm to determine who to evict.


## Page
 - Page_for_addr: As instructed by Alex Wang, we also decided to implement our stack growth detection here. It returns the page given an address. We can check it from pages. And if null, we see if it's trying to grow the stack. 
 - Page_in would call do_page_in, which is where swap_in truly occurs if data is on disk.
 - Page_out: responsible for managing the process of swapping a page from physical memory to disk storage.
 The page_out function checks if a page is eligible for swapping by determining whether it is not pinned and if it has been modified (dirty). If eligible, it writes the page to a swap partition on the disk, involving locating a free swap slot, performing the disk write, and updating the page table to indicate the page's new location in swap space.

## page_dir
 - we modified the pagedir_destroy to accomodate swap features, which could make some files not presentt on memory. Therefore, we are only using pde_get_pt instead of pde_get_page.
  
## Exception
 - Handle page faults here. If page not present and is user, we can try page in here. 
 - To pass the stack grow test, I also handled the case where it is sysall but not present here as well. 

# Synchronization
 - swap features are synchronized by a swap lock
 - we added file_lock around exec() to force each thread to execute in its entirety, this prevents issues in conflict frame allocation
 - each frame holds a lock, this is for frame management so that no 2 different threads using the same frame, which could cause issue if trying
 to do different things

