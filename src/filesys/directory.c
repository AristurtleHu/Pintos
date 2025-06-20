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

static void name_split(const char *path, char *directory, char *filename);
static bool dir_add_subdir(block_sector_t inode_sector, struct dir_entry e,
                           struct dir *dir);
static bool dir_is_empty(const struct dir *dir);

// The first is reserved for the parent
#define START_POS sizeof(struct dir_entry)

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool dir_create(block_sector_t sector, size_t entry_cnt) {
  if (!inode_create(sector, entry_cnt * sizeof(struct dir_entry), true))
    return false;

  // self-referencing
  bool success = true;
  struct dir_entry e;
  e.inode_sector = sector;

  struct dir *dir = dir_open(inode_open(sector));
  if (inode_write_at(dir->inode, &e, START_POS, 0) != START_POS)
    success = false;

  dir_close(dir);

  return success;
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *dir_open(struct inode *inode) {
  struct dir *dir = calloc(1, sizeof *dir);
  if (inode != NULL && dir != NULL) {
    dir->inode = inode;
    dir->pos = START_POS;
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

  for (ofs = START_POS;
       inode_read_at(dir->inode, &e, sizeof e, ofs) == sizeof e;
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

  // current directory
  if (strcmp(name, ".") == 0)
    *inode = inode_reopen(dir->inode);

  // parent directory
  else if (strcmp(name, "..") == 0) {
    inode_read_at(dir->inode, &e, START_POS, 0);
    *inode = inode_open(e.inode_sector);
  }

  else if (lookup(dir, name, &e, NULL))
    *inode = inode_open(e.inode_sector);

  // Not found
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
bool dir_add(struct dir *dir, const char *name, block_sector_t inode_sector,
             bool is_dir) {
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

  // Update the child dir
  if (is_dir) {
    if (!dir_add_subdir(inode_sector, e, dir))
      goto done;
  }

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

  /* Prevent removing non-empty dir. */
  if (inode_is_directory(inode)) {
    struct dir *target = dir_open(inode);
    bool is_empty = dir_is_empty(target);
    dir_close(target);

    if (!is_empty)
      goto done;
  }

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

/* Adds a subdirectory entry to DIR with the given INODE_SECTOR
   and name E.
   Returns true if successful, false on failure. */
static bool dir_add_subdir(block_sector_t inode_sector, struct dir_entry e,
                           struct dir *dir) {
  struct dir *child_dir = dir_open(inode_open(inode_sector));
  if (child_dir == NULL)
    return false;

  e.inode_sector = inode_get_inumber(dir_get_inode(dir));
  if (inode_write_at(child_dir->inode, &e, sizeof e, 0) != sizeof e) {
    dir_close(child_dir);
    return false;
  }
  dir_close(child_dir);
  return true;
}

/* Returns whether the DIR is empty. */
static bool dir_is_empty(const struct dir *dir) {
  struct dir_entry e;
  off_t ofs;

  for (ofs = START_POS;
       inode_read_at(dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (e.in_use)
      return false;

  return true;
}

/* Split the name into dir and filename.
  Return nothing */
static void name_split(const char *name, char *directory, char *filename) {
  char *last_slash = strrchr(name, '/');

  if (last_slash == NULL) {
    // No slash found - entire name is filename
    strlcpy(directory, ".", 2);
    strlcpy(filename, name, NAME_MAX + 1);
  } else if (last_slash == name) {
    // Slash at beginning - root directory
    strlcpy(directory, "/", 2);
    strlcpy(filename, last_slash + 1, NAME_MAX + 1);
  } else {
    // Copy dir
    size_t dir_len = last_slash - name;
    strlcpy(directory, name, dir_len + 1);

    // Copy filename
    strlcpy(filename, last_slash + 1, NAME_MAX + 1);
  }
}

/* Opens the directory for given path. */
struct dir *dir_open_path(const char *path) {
  // copy of path, to tokenize
  int l = strlen(path);
  char s[l + 1];
  strlcpy(s, path, l + 1);

  // Start from root or current working directory
  struct dir *curr = (path[0] == '/' || thread_current()->cwd == NULL)
                         ? dir_open_root()
                         : dir_reopen(thread_current()->cwd);

  // tokenize
  char *token, *p;
  for (token = strtok_r(s, "/", &p); token != NULL;
       token = strtok_r(NULL, "/", &p)) {
    struct inode *inode = NULL;

    // not exist
    if (!dir_lookup(curr, token, &inode)) {
      dir_close(curr);
      return NULL;
    }

    struct dir *next = dir_open(inode);
    dir_close(curr);

    if (next == NULL)
      return NULL;

    curr = next;
  }

  // avoid removed directories
  if (inode_is_removed(dir_get_inode(curr))) {
    dir_close(curr);
    return NULL;
  }

  return curr;
}

/* Splits the path NAME into a directory and a file name.
   Return dir */
struct dir *dir_split(const char *name, char *file_name) {
  char directory[strlen(name)];
  name_split(name, directory, file_name);
  return dir_open_path(directory);
}