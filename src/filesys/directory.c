#include "filesys/directory.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include <limits.h>
#include <list.h>
#include <stdio.h>
#include <string.h>

/* A directory. */
struct dir {
  struct inode *inode; /* Backing store. */
  off_t pos;           /* Current position. */
};

/* A single directory entry. */
struct dir_entry {
  block_sector_t inode_sector; /* Sector number of header. */
  char name[NAME_MAX + 1];     /* Null terminated file name. */
  bool in_use;                 /* In use or free? */
};

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool dir_create(block_sector_t sector, size_t entry_cnt) {
  return inode_create(sector, entry_cnt * sizeof(struct dir_entry));
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *dir_open(struct inode *inode) {
  struct dir *dir = calloc(1, sizeof *dir);
  if (inode != NULL && dir != NULL) {
    dir->inode = inode;
    dir->pos = 0;
    return dir;
  } else {
    inode_close(inode);
    free(dir);
    return NULL;
  }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *dir_open_root(void) {
  return dir_open(inode_open(ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *dir_reopen(struct dir *dir) {
  return dir_open(inode_reopen(dir->inode));
}

/* Destroys DIR and frees associated resources. */
void dir_close(struct dir *dir) {
  if (dir != NULL) {
    inode_close(dir->inode);
    free(dir);
  }
}

/* Returns the inode encapsulated by DIR. */
struct inode *dir_get_inode(struct dir *dir) { return dir->inode; }

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool lookup(const struct dir *dir, const char *name,
                   struct dir_entry *ep, off_t *ofsp) {
  struct dir_entry e;
  size_t ofs;

  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  for (ofs = 0; inode_read_at(dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (e.in_use && !strcmp(name, e.name)) {
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
bool dir_lookup(const struct dir *dir, const char *name, struct inode **inode) {
  struct dir_entry e;

  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  if (lookup(dir, name, &e, NULL))
    *inode = inode_open(e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool dir_add(struct dir *dir, const char *name, block_sector_t inode_sector) {
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen(name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup(dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.

     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at(dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy(e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at(dir->inode, &e, sizeof e, ofs) == sizeof e;

done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool dir_remove(struct dir *dir, const char *name) {
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  /* Find directory entry. */
  if (!lookup(dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open(e.inode_sector);
  if (inode == NULL)
    goto done;

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at(dir->inode, &e, sizeof e, ofs) != sizeof e)
    goto done;

  /* Remove inode. */
  inode_remove(inode);
  success = true;

done:
  inode_close(inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool dir_readdir(struct dir *dir, char name[NAME_MAX + 1]) {
  struct dir_entry e;

  while (inode_read_at(dir->inode, &e, sizeof e, dir->pos) == sizeof e) {
    dir->pos += sizeof e;
    if (e.in_use) {
      strlcpy(name, e.name, NAME_MAX + 1);
      return true;
    }
  }
  return false;
}

struct dir *dir_open_path(const char *path) {
  // 1. 创建路径字符串的可修改副本，因为 strtok_r 会修改原字符串
  //    注意: char s[l+1] 是一个变长数组(VLA)，这是 C99 标准的功能。
  int l = strlen(path);
  char s[l + 1];
  strlcpy(s, path, l + 1);

  // 2. 根据是绝对路径还是相对路径，确定起始目录
  struct dir *curr;
  if (path[0] == '/') {
    // 绝对路径，从根目录开始
    curr = dir_open_root();
  } else {
    // 相对路径，从当前进程的工作目录开始
    struct thread *t = thread_current();
    if (t->cwd == NULL) {
      // 如果当前进程没有工作目录（例如主内核线程），则默认为根目录
      curr = dir_open_root();
    } else {
      // 重新打开当前工作目录，以获取一个新的、可以被关闭的句柄
      curr = dir_reopen(t->cwd);
    }
  }

  // 如果起始目录无法打开，则直接失败
  if (curr == NULL) {
    return NULL;
  }

  // 3. 使用 strtok_r 循环解析并遍历路径的每个组件
  char *token, *p;
  for (token = strtok_r(s, "/", &p); token != NULL;
       token = strtok_r(NULL, "/", &p)) {
    struct inode *inode = NULL;

    // 3a. 在当前目录(curr)中查找名为 token 的条目
    if (!dir_lookup(curr, token, &inode)) {
      dir_close(curr); // 清理资源
      return NULL;     // 找不到该条目，路径无效
    }

    // 3b. 【重要】检查查找到的 inode 是否确实是一个目录
    //      如果不是目录，就不能继续深入遍历
    if (!inode_is_directory(inode)) {
      inode_close(inode); // 清理 lookup 返回的 inode
      dir_close(curr);    // 清理当前目录
      return NULL;
    }

    // 3c. 打开下一级目录
    struct dir *next = dir_open(inode);
    if (next == NULL) {
      // inode_close(inode); // dir_open 失败时会负责关闭 inode
      dir_close(curr);
      return NULL; // 打开失败
    }

    // 3d. 关闭当前层级的目录，并将指针移到下一级目录
    dir_close(curr);
    curr = next;
  }

  // 5. 成功遍历完所有路径组件，返回最终打开的目录句柄
  return curr;
}