/*
 * VFS (Virtual File System) interface utilities. added @lab4_1.
 */

#include <kernel/fs/hostfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/mm/page.h>
//
#include <kernel/fs/inode.h>
#include <kernel/fs/ramfs.h>
#include <kernel/types.h>

#include <util/hash_table.h>
#include <util/string.h>

#include <spike_interface/spike_utils.h>

struct dentry *vfs_root_dentry;              // system root direntry
struct super_block *vfs_sb_list[MAX_MOUNTS]; // system superblock list
struct device *vfs_dev_list[MAX_VFS_DEV];    // system device list in vfs layer
struct hash_table dentry_hash_table;
struct hash_table vinode_hash_table;

struct file_system_type *fs_list[MAX_SUPPORTED_FS];

//
// initialize file system
//
void init_fs(void) {
  sprint("init_fs: Initiating fs\n");

  // initialize the vfs
  vfs_init();
  // register hostfs and mount it as the root
  if (register_hostfs() < 0)
    panic("init_fs: cannot register hostfs.\n");
  struct device *hostdev = init_host_device("HOSTDEV");
  vfs_mount("HOSTDEV", MOUNT_AS_ROOT);




  // register and mount rfs
  if (register_ramfs() < 0)
    panic("init_fs: cannot register ramfs.\n");
  struct device *ramdisk0 = init_ramfs_device("RAMDISK0");
  ramfs_format_dev(ramdisk0);
  vfs_mount("RAMDISK0", MOUNT_DEFAULT);


  sprint("init_fs: Fs initialized\n");
}

//
// initializes the dentry hash list and vinode hash list
//
int vfs_init() {
  sprint("vfs_init: Initiating vfs\n");

  int ret;

  if ((ret = hash_table_init(&dentry_hash_table, dentry_hash_equal,
                             dentry_hash_func, NULL, NULL, NULL)) != 0)
    return ret;

  if ((ret = hash_table_init(&vinode_hash_table, inode_hash_equal,
                             inode_hash_func, NULL, NULL, NULL)) != 0)
    return ret;
  sprint("vfs_init: Vfs init.\n");

  return 0;
}

//
// mount a file system from the device named "dev_name"
// PKE does not support mounting a device at an arbitrary directory as in Linux,
// but can only mount a device in one of the following two ways (according to
// the mnt_type parameter) :
// 1. when mnt_type = MOUNT_AS_ROOT
//    Mount the device AS the root directory.
//    that is, mount the device under system root direntry:"/".
//    In this case, the device specified by parameter dev_name will be used as
//    the root file system.
// 2. when mnt_type = MOUNT_DEFAULT
//    Mount the device UNDER the root directory.
//    that is, mount the device to "/DEVICE_NAME" (/DEVICE_NAME will be
//    automatically created) folder.
//
struct super_block *vfs_mount(const char *dev_name, int mnt_type) {
  // device pointer
  struct device *p_device = NULL;

  // find the device entry in vfs_device_list named as dev_name
  for (int i = 0; i < MAX_VFS_DEV; ++i) {
    p_device = vfs_dev_list[i];
    if (p_device && strcmp(p_device->dev_name, dev_name) == 0)
      break;
  }
  if (p_device == NULL)
    panic("vfs_mount: cannot find the device entry!\n");

  // add the super block into vfs_sb_list
  struct file_system_type *fs_type = p_device->fs_type;
  struct super_block *sb = fs_type->get_superblock(p_device);

  // add the root vinode into vinode_hash_table
  hash_put_inode(sb->s_root->dentry_inode);

  int err = 1;
  for (int i = 0; i < MAX_MOUNTS; ++i) {
    if (vfs_sb_list[i] == NULL) {
      vfs_sb_list[i] = sb;
      err = 0;
      break;
    }
  }
  if (err)
    panic("vfs_mount: too many mounts!\n");

  // mount the root dentry of the file system to right place
  if (mnt_type == MOUNT_AS_ROOT) {
    vfs_root_dentry = sb->s_root;

    // insert the mount point into hash table
    hash_put_dentry(sb->s_root);
  } else if (mnt_type == MOUNT_DEFAULT) {
    if (!vfs_root_dentry)
      panic("vfs_mount: root dentry not found, please mount the root device "
            "first!\n");

    struct dentry *mnt_point = sb->s_root;

    // set the mount point directory's name to device name
    char *dev_name = p_device->dev_name;
    strcpy(mnt_point->name, dev_name);

    // by default, it is mounted under the vfs root directory
    mnt_point->parent = vfs_root_dentry;

    // insert the mount point into hash table
    hash_put_dentry(sb->s_root);
  } else {
    panic("vfs_mount: unknown mount type!\n");
  }
  sprint("vfs_mount: mount device %s successfully.\n", dev_name);
  return sb;
}

//
// open a file located at "path" with permission of "flags".
// if the file does not exist, and O_CREAT bit is set in "flags", the file will
// be created.
// return: the file pointer to the opened file.
//
struct file *vfs_open(const char *path, int flags) {
  struct dentry *parent =
      vfs_root_dentry; // we start the path lookup from root.
  char miss_name[MAX_PATH_LEN];

  // path lookup.
  struct dentry *file_dentry = lookup_final_dentry(path, &parent, miss_name);

  // file does not exist
  if (!file_dentry) {
    sprint("vfs_open: file_dentry not found.\n");
    int creatable = flags & O_CREAT;

    // create the file if O_CREAT bit is set
    if (creatable) {
      char basename[MAX_PATH_LEN];
      get_base_name(path, basename);

      // a missing directory exists in the path
      if (strcmp(miss_name, basename) != 0) {
        sprint("vfs_open: cannot create file in a non-exist directory!\n");
        return NULL;
      }

      // create the file
      file_dentry = alloc_vfs_dentry(basename, NULL, parent);
      struct inode *new_inode = viop_create(parent->dentry_inode, file_dentry);
      if (!new_inode)
        panic("vfs_open: cannot create file!\n");

      file_dentry->dentry_inode = new_inode;
      atomic_inc(&(new_inode->i_count));
      hash_put_dentry(file_dentry);
      hash_put_inode(new_inode);
    } else {
      sprint("vfs_open: cannot find the file!\n");
      return NULL;
    }
  }


	sprint("vfs_open: open dentry file_dentry=%s\n",file_dentry->name);

  if (file_dentry->dentry_inode->i_mode != S_IFREG) {
    sprint("vfs_open: cannot open a directory!\n");
    return NULL;
  }

  // get writable and readable flags
  int writable = 0;
  int readable = 0;
  switch (flags & MASK_FILEMODE) {
  case O_RDONLY:
    writable = 0;
    readable = 1;
    break;
  case O_WRONLY:
    writable = 1;
    readable = 0;
    break;
  case O_RDWR:
    writable = 1;
    readable = 1;
    break;
  default:
    panic("fs_open: invalid open flags!\n");
  }

  struct file *file = alloc_vfs_file(file_dentry, readable, writable, 0);

  // additional open operations for a specific file system
  // hostfs needs to conduct actual file open.
  if (file_dentry->dentry_inode->i_op->viop_hook_open) {
    if (file_dentry->dentry_inode->i_op->viop_hook_open(
            file_dentry->dentry_inode, file_dentry) < 0) {
      sprint("vfs_open: hook_open failed!\n");
    }
  }
	sprint("vfs_open: end\n");

  return file;
}

//
// read content from "file" starting from file->offset, and store it in "buf".
// return: the number of bytes actually read
//
ssize_t vfs_read(struct file *file, char *buf, size_t count) {
  if (!(file->f_mode & FMODE_READ)) {
    sprint("vfs_read: file is not readable!\n");
    return -1;
  }
  if (file->f_dentry->dentry_inode->i_mode != S_IFREG) {
    sprint("vfs_read: cannot read a directory!\n");
    return -1;
  }
  // actual reading.
  return viop_read(file->f_dentry->dentry_inode, buf, count, &(file->f_pos));
}

//
// write content in "buf" to "file", at file->offset.
// return: the number of bytes actually written
//
ssize_t vfs_write(struct file *file, const char *buf, size_t count) {
  if (!(file->f_mode & FMODE_WRITE)) {
    sprint("vfs_write: file is not writable!\n");
    return -1;
  }
  if (file->f_dentry->dentry_inode->i_mode != S_IFREG) {
    sprint("vfs_read: cannot write a directory!\n");
    return -1;
  }
  // actual writing.
  return viop_write(file->f_dentry->dentry_inode, buf, count, &(file->f_pos));
}

//
// reposition read/write file offset
// return: the new offset on success, -1 on failure.
//
ssize_t vfs_lseek(struct file *file, ssize_t offset, int whence) {
  if (file->f_dentry->dentry_inode->i_mode != S_IFREG) {
    sprint("vfs_read: cannot seek a directory!\n");
    return -1;
  }

  if (viop_lseek(file->f_dentry->dentry_inode, offset, whence,
                 &(file->f_pos)) != 0) {
    sprint("vfs_lseek: lseek failed!\n");
    return -1;
  }

  return file->f_pos;
}

//
// read the vinode information
//
int vfs_stat(struct file *file, struct istat *istat) {
  istat->st_inum = file->f_dentry->dentry_inode->i_ino;
  istat->st_size = file->f_dentry->dentry_inode->i_size;
  istat->st_type = file->f_dentry->dentry_inode->i_mode;
  istat->st_nlinks = file->f_dentry->dentry_inode->i_nlink;
  istat->st_blocks = file->f_dentry->dentry_inode->blocks;
  return 0;
}

//
// read the inode information on the disk
//
int vfs_disk_stat(struct file *file, struct istat *istat) {
  sprint("do_stat\n");
  return viop_disk_stat(file->f_dentry->dentry_inode, istat);
}

//
// make hard link to the file specified by "oldpath" with the name "newpath"
// return: -1 on failure, 0 on success.
//
int vfs_link(const char *oldpath, const char *newpath) {
  struct dentry *parent = vfs_root_dentry;
  char miss_name[MAX_PATH_LEN];

  // lookup oldpath
  struct dentry *old_file_dentry =
      lookup_final_dentry(oldpath, &parent, miss_name);
  if (!old_file_dentry) {
    sprint("vfs_link: cannot find the file!\n");
    return -1;
  }

  if (old_file_dentry->dentry_inode->i_mode != S_IFREG) {
    sprint("vfs_link: cannot link a directory!\n");
    return -1;
  }

  parent = vfs_root_dentry;
  // lookup the newpath
  // note that parent is changed to be the last directory entry to be accessed
  struct dentry *new_file_dentry =
      lookup_final_dentry(newpath, &parent, miss_name);
  if (new_file_dentry) {
    sprint("vfs_link: the new file already exists!\n");
    return -1;
  }

  char basename[MAX_PATH_LEN];
  get_base_name(newpath, basename);
  if (strcmp(miss_name, basename) != 0) {
    sprint("vfs_link: cannot create file in a non-exist directory!\n");
    return -1;
  }

  // do the real hard-link
  new_file_dentry =
      alloc_vfs_dentry(basename, old_file_dentry->dentry_inode, parent);
  int err = viop_link(parent->dentry_inode, new_file_dentry,
                      old_file_dentry->dentry_inode);
  if (err)
    return -1;

  // make a new dentry for the new link
  hash_put_dentry(new_file_dentry);

  return 0;
}

//
// unlink (delete) a file specified by "path".
// return: -1 on failure, 0 on success.
//
int vfs_unlink(const char *path) {
  struct dentry *parent = vfs_root_dentry;
  char miss_name[MAX_PATH_LEN];

  // lookup the file, find its parent direntry
  struct dentry *file_dentry = lookup_final_dentry(path, &parent, miss_name);
  if (!file_dentry) {
    sprint("vfs_unlink: cannot find the file!\n");
    return -1;
  }

  if (file_dentry->dentry_inode->i_mode != S_IFREG) {
    sprint("vfs_unlink: cannot unlink a directory!\n");
    return -1;
  }

  if (file_dentry->d_ref > 0) {
    sprint("vfs_unlink: the file is still opened!\n");
    return -1;
  }

  // do the real unlink
  struct inode *unlinked_vinode = file_dentry->dentry_inode;
  int err = viop_unlink(parent->dentry_inode, file_dentry, unlinked_vinode);
  if (err)
    return -1;

  // remove the dentry from the hash table
  hash_erase_dentry(file_dentry);
  free_vfs_dentry(file_dentry);
  atomic_dec(&unlinked_vinode->i_count);

  // if this inode has been removed from disk
  if (unlinked_vinode->i_nlink == 0) {
    // no one will remember a dead inode
    assert(atomic_read(&unlinked_vinode->i_count) == 0);

    // we don't write back the inode, because it has disappeared from the disk
    hash_erase_inode(unlinked_vinode);
    kfree(unlinked_vinode); // free the vinode
  }

  return 0;
}

//
// close a file at vfs layer.
//
int vfs_close(struct file *file) {
  if (file->f_dentry->dentry_inode->i_mode != S_IFREG) {
    sprint("vfs_close: cannot close a directory!\n");
    return -1;
  }

  struct dentry *dentry = file->f_dentry;
  struct inode *inode = dentry->dentry_inode;

  // additional close operations for a specific file system
  // hostfs needs to conduct actual file close.
  if (inode->i_op->viop_hook_close) {
    if (inode->i_op->viop_hook_close(inode, dentry) != 0) {
      sprint("vfs_close: hook_close failed!\n");
    }
  }

  dentry->d_ref--;
  // if the dentry is not pointed by any opened file, free the dentry
  if (dentry->d_ref == 0) {
    // free the dentry
    hash_erase_dentry(dentry);
    free_vfs_dentry(dentry);
    atomic_dec(&inode->i_count);
    // no other opened hard link
    if (atomic_read(&inode->i_count) == 0) {
      // write back the inode and free it
      if (viop_write_back_vinode(inode) != 0)
        panic("vfs_close: free inode failed!\n");
      hash_erase_inode(inode);
      kfree(inode);
    }
  }

  return 0;
}

//
// open a dir at vfs layer. the directory must exist on disk.
//
struct file *vfs_opendir(const char *path) {
  sprint("vfs_opendir: path = %s\n", path);

  struct dentry *parent = vfs_root_dentry;
  char miss_name[MAX_PATH_LEN];

  // lookup the dir
  struct dentry *file_dentry = lookup_final_dentry(path, &parent, miss_name);

  if (!file_dentry || file_dentry->dentry_inode->i_mode != S_IFDIR) {
    sprint("vfs_opendir: cannot find the direntry!\n");
    return NULL;
  }

  // allocate a vfs file with readable/non-writable flag.
  struct file *file = alloc_vfs_file(file_dentry, 1, 0, 0);

  // additional open direntry operations for a specific file system
  // rfs needs duild dir cache.
  if (file_dentry->dentry_inode->i_op->viop_hook_opendir) {
    if (file_dentry->dentry_inode->i_op->viop_hook_opendir(
            file_dentry->dentry_inode, file_dentry) != 0) {
      sprint("vfs_opendir: hook opendir failed!\n");
    }
  }

  return file;
}

//
// read a direntry entry from a direntry specified by "file"
// the read direntry entry is stored in "dir"
//
int vfs_readdir(struct file *file, struct dir *dir) {
  if (file->f_dentry->dentry_inode->i_mode != S_IFDIR) {
    sprint("vfs_readdir: cannot read a file!\n");
    return -1;
  }
  return viop_readdir(file->f_dentry->dentry_inode, dir, &(file->f_pos));
}

//
// make a new directory specified by "path" at vfs layer.
// note that only the last level directory of the path will be created,
// and its parent directory must exist.
//
int vfs_mkdir(const char *path) {
  struct dentry *parent = vfs_root_dentry;
  char miss_name[MAX_PATH_LEN];

  // lookup the dir, find its parent direntry
  struct dentry *file_dentry = lookup_final_dentry(path, &parent, miss_name);
  if (file_dentry) {
    sprint("vfs_mkdir: the directory already exists!\n");
    return -1;
  }

  char basename[MAX_PATH_LEN];
  get_base_name(path, basename);
  if (strcmp(miss_name, basename) != 0) {
    sprint("vfs_mkdir: cannot create directory in a non-exist directory!\n");
    return -1;
  }

  // do real mkdir
  struct dentry *new_dentry = alloc_vfs_dentry(basename, NULL, parent);
  struct inode *new_dir_inode = viop_mkdir(parent->dentry_inode, new_dentry);
  if (!new_dir_inode) {
    kfree(new_dentry);
    sprint("vfs_mkdir: cannot create directory!\n");
    return -1;
  }

  new_dentry->dentry_inode = new_dir_inode;
  atomic_inc(&new_dir_inode->i_count);
  hash_put_dentry(new_dentry);
  hash_put_inode(new_dir_inode);
  return 0;
}

//
// close a directory at vfs layer
//
int vfs_closedir(struct file *file) {
  if (file->f_dentry->dentry_inode->i_mode != S_IFDIR) {
    sprint("vfs_closedir: cannot close a file!\n");
    return -1;
  }

  // even if a directory is no longer referenced, it will not be freed because
  // it will serve as a cache for later lookup operations on it or its
  // descendants
  file->f_dentry->d_ref--;

  // additional close direntry operations for a specific file system
  // rfs needs reclaim dir cache.
  if (file->f_dentry->dentry_inode->i_op->viop_hook_closedir) {
    if (file->f_dentry->dentry_inode->i_op->viop_hook_closedir(
            file->f_dentry->dentry_inode, file->f_dentry) != 0) {
      sprint("vfs_closedir: hook closedir failed!\n");
    }
  }
  return 0;
}

//
// lookup the "path" and return its dentry (or NULL if not found).
// the lookup starts from parent, and stop till the full "path" is parsed.
// return: the final dentry if we find it, NULL for otherwise.
//
struct dentry *lookup_final_dentry(const char *path, struct dentry **parent,
                                   char *miss_name) {

  sprint("lookup_final_dentry: path = %s.\n", path);
  char path_copy[MAX_PATH_LEN];
  strcpy(path_copy, path);

  // split the path, and retrieves a token at a time.
  // note: strtok() uses a static (local) variable to store the input path
  // string at the first time it is called. thus it can out a token each time.
  // for example, when input path is: /RAMDISK0/test_dir/ramfile2
  // strtok() outputs three tokens: 1)RAMDISK0, 2)test_dir and 3)ramfile2
  // at its three continuous invocations.
  char *token = strtok(path_copy, "/");
  struct dentry *this = *parent;

  // Handle special case ".." for parent directory
  if (token && strcmp(token, "..") == 0) {
    this = (*parent)->parent;
    if (this == NULL) {
      // We're at the root already
      this = *parent;
    }
    *parent = this->parent;
    token = strtok(NULL, "/");
  }

  // Handle special case "." for current_percpu directory
  if (token && strcmp(token, ".") == 0) {
    // sprint("lookup_final_dentry: token is .\n");
    token = strtok(NULL, "/");
  }

  while (token != NULL) {
    *parent = this;

    // Handle ".." within the path
    if (strcmp(token, "..") == 0) {
      this = (*parent)->parent;
      if (this == NULL) {
        // We're at the root already
        this = *parent;
      }
      *parent = this->parent;
      token = strtok(NULL, "/");
      continue;
    }

    // Handle "." within the path (just skip it)
    if (strcmp(token, ".") == 0) {
      token = strtok(NULL, "/");
      continue;
    }

    sprint("lookup_final_dentry: hash_get_dentry.\n", path);
    this = hash_get_dentry((*parent), token); // try hash first
    if (this == NULL) {
      // if not found in hash, try to find it in the directory
      sprint("lookup_final_dentry: alloc_vfs_dentry.\n", path);
      this = alloc_vfs_dentry(token, NULL, *parent);
      // lookup subfolder/file in its parent directory. note:
      // hostfs and rfs will take different procedures for lookup.
      struct inode *found_vinode = viop_lookup((*parent)->dentry_inode, this);
      sprint("lookup_final_dentry: found_vinode=0x%lx.\n", found_vinode);

      if (found_vinode == NULL) {
        // not found in both hash table and directory file on disk.
        kfree(this);
        strcpy(miss_name, token);
        return NULL;
      }

      struct inode *same_inode =
          hash_get_inode(found_vinode->sb, found_vinode->i_ino);
      sprint("lookup_final_dentry: same_inode=0x%lx.\n", same_inode);

      if (same_inode != NULL) {
        // the vinode is already in the hash table (i.e. we are opening another
        // hard link)
				sprint("lookup_final_dentry: this->dentry_inode = same_inode.\n");

        this->dentry_inode = same_inode;
        atomic_inc(&same_inode->i_count);

        kfree(found_vinode);
      } else {
				sprint("lookup_final_dentry: this->dentry_inode = found_vinode.\n");

        // the vinode is not in the hash table
        this->dentry_inode = found_vinode;
        atomic_inc(&found_vinode->i_count);
        hash_put_inode(found_vinode);
      }

      hash_put_dentry(this);
      sprint("lookup_final_dentry: hash_put_dentry=0x%lx.\n", this);
    }
    sprint("lookup_final_dentry: token = %s\n", token);

    // get next token
    token = strtok(NULL, "/");
  }
	sprint("lookup_final_dentry: end.\n");

  return this;
}

//
// get the base name of a path
// handles both absolute and relative paths
//
void get_base_name(const char *path, char *base_name) {
  // Handle NULL or empty path
  if (!path || path[0] == '\0') {
    strcpy(base_name, "");
    return;
  }

  // Handle root path
  if (strcmp(path, "/") == 0) {
    strcpy(base_name, "/");
    return;
  }

  // Handle paths that end with slash (directory paths)
  char path_copy[MAX_PATH_LEN];
  strcpy(path_copy, path);
  path_copy[MAX_PATH_LEN - 1] = '\0';

  // Remove trailing slashes
  int len = strlen(path_copy);
  while (len > 0 && path_copy[len - 1] == '/') {
    path_copy[len - 1] = '\0';
    len--;
  }

  // If path is empty after removing trailing slashes
  if (path_copy[0] == '\0') {
    strcpy(base_name, "/");
    return;
  }

  // Split the path by '/' and find the last token
  char *token = strtok(path_copy, "/");
  char *last_token = token;
  sprint("get_base_name: token = %s\n", token);
  while (token != NULL) {
    // Check for special directory names (for relative path handling)
    if (strcmp(token, ".") == 0 || strcmp(token, "..") == 0) {
      last_token = NULL;
    } else {
      last_token = token;
      sprint("get_base_name: last_token = %s\n", last_token);
    }
    token = strtok(NULL, "/");
    // sprint("get_base_name: token = %s\n", token);
  }

  // Copy the last token to base_name or default to "/"
  if (last_token) {
    strcpy(base_name, last_token);
  } else {
    strcpy(base_name, "/");
  }
}

//
// alloc a (virtual) dir entry
//
struct dentry *alloc_vfs_dentry(const char *name, struct inode *inode,
                                struct dentry *parent) {
  struct dentry *dentry = kmalloc(sizeof(struct dentry));
  if (!dentry) {
    return NULL;
  }
  strcpy(dentry->name, name);
  dentry->dentry_inode = inode;
  if (inode) {
    atomic_inc(&inode->i_count);
  }

  dentry->parent = parent;
  dentry->d_ref = 0;
  return dentry;
}

//
// free a (virtual) dir entry, if it is not referenced by any file
//
int free_vfs_dentry(struct dentry *dentry) {
  if (dentry->d_ref > 0) {
    sprint("free_vfs_dentry: dentry is still in use!\n");
    return -1;
  }
  kfree((void *)dentry);
  return 0;
}

// dentry generic hash table method implementation
int dentry_hash_equal(void *key1, void *key2) {
  struct dentry_key *dentry_key1 = key1;
  struct dentry_key *dentry_key2 = key2;
  if (strcmp(dentry_key1->name, dentry_key2->name) == 0 &&
      dentry_key1->parent == dentry_key2->parent) {
    return 1;
  }
  return 0;
}

size_t dentry_hash_func(void *key) {
  struct dentry_key *dentry_key = key;
  char *name = dentry_key->name;

  size_t hash = 5381;
  int c;

  while ((c = *name++))
    hash = ((hash << 5) + hash) + c; // hash * 33 + c

  hash = ((hash << 5) + hash) + (size_t)dentry_key->parent;
  return hash % HASH_TABLE_SIZE;
}

// dentry hash table interface
struct dentry *hash_get_dentry(struct dentry *parent, char *name) {
  struct dentry_key key = {.parent = parent, .name = name};
  return (struct dentry *)dentry_hash_table.virtual_hash_get(&dentry_hash_table,
                                                             &key);
}

int hash_put_dentry(struct dentry *dentry) {
  struct dentry_key *key = kmalloc(sizeof(struct dentry_key));
  if (!key) {
    return -1;
  }
  key->name = dentry->name;
  key->parent = dentry->parent;

  int ret = dentry_hash_table.virtual_hash_put(&dentry_hash_table, key, dentry);
  if (ret != 0)
    kfree(key);
  return ret;
}

int hash_erase_dentry(struct dentry *dentry) {
  struct dentry_key key = {.parent = dentry->parent, .name = dentry->name};
  return dentry_hash_table.virtual_hash_erase(&dentry_hash_table, &key);
}

// vinode generic hash table method implementation
int inode_hash_equal(void *key1, void *key2) {
  struct inode_key *vinode_key1 = key1;
  struct inode_key *vinode_key2 = key2;
  if (vinode_key1->inum == vinode_key2->inum &&
      vinode_key1->sb == vinode_key2->sb) {
    return 1;
  }
  return 0;
}

size_t inode_hash_func(void *key) {
  struct inode_key *vinode_key = key;
  return vinode_key->inum % HASH_TABLE_SIZE;
}

// vinode hash table interface
struct inode *hash_get_inode(struct super_block *sb, int inum) {
  if (inum < 0)
    return NULL;
  struct inode_key key = {.sb = sb, .inum = inum};
  return (struct inode *)vinode_hash_table.virtual_hash_get(&vinode_hash_table,
                                                            &key);
}

int hash_put_inode(struct inode *vinode) {
  if (vinode->i_ino < 0)
    return -1;
  struct inode_key *key = kmalloc(sizeof(struct inode_key));
  if (!key) {
    return -1;
  }
  key->sb = vinode->sb;
  key->inum = vinode->i_ino;

  int ret = vinode_hash_table.virtual_hash_put(&vinode_hash_table, key, vinode);
  if (ret != 0)
    kfree(key);
  return ret;
}

int hash_erase_inode(struct inode *vinode) {
  if (vinode->i_ino < 0)
    return -1;
  struct inode_key key = {.sb = vinode->sb, .inum = vinode->i_ino};
  return vinode_hash_table.virtual_hash_erase(&vinode_hash_table, &key);
}

//
// shared (default) actions on allocating a vfs inode.
//
struct inode *default_alloc_vinode(struct super_block *sb) {
  struct inode *vinode = kmalloc(sizeof(struct inode));
  if (!vinode) {
    return NULL;
  }
  vinode->blocks = 0;
  vinode->i_ino = 0;
  vinode->i_nlink = 0;
  atomic_set(&vinode->i_count, 0);
  vinode->sb = sb;
  vinode->i_size = 0;
  return vinode;
}
