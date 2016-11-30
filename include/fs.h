#ifndef __FS_H
#define __FS_H

#include <type.h>

/* page global directory */
typedef uint32_t pde_t;
/* page talbe entry */
typedef uint32_t pte_t;

// ################################# minix.h #################################

/* Minix v1 file system structure
 *  zone:  0           1              2             2 + imap_blk        ...         ...
 *  +--------------------------------------------------------------------------------------+
 *  | bootsector | superblock | inode bitmap ... | zone bitmap ... | inodes ... | data ... | 
 *  +--------------------------------------------------------------------------------------+
 *  1 zone = 2 block = 1024 byte
 */

#define ROOT_DEV 0
#define ROOT_INO 1

/* size of a virtual block (zone) (byte) */
#define BSIZE 1024
/* size of a block in disk (byte) */
#define PHY_BSIZE 512 

struct super_block {
    uint16_t ninodes;       // number of inodes
    uint16_t nzones;        // number of zones
    uint16_t imap_blk;      // space uesd by inode map (block)
    uint16_t zmap_blk;      // space used by zone map   (block)
    uint16_t fst_data_zone; // first zone with `file` data
    uint16_t log_zone_size; // size of a data zone = 1024 << log_zone_size

    uint32_t max_size;      // max file size (byte)
    uint16_t magic;         // magic number
    uint16_t state;         // (?) mount state
};

/* in-disk minix inode */
struct d_inode {
    uint16_t mode;  // file type and RWX(unused) bit
    uint16_t uid;   // identifies the user who owns the file (unused)
    uint32_t size;  // file size (byte)
    uint32_t mtime; // time since Jan. 1st, 1970 (second) (unused)
    uint8_t gid;    // owner's group (unused)
    uint8_t nlinks; // number of dircetory link to it

    /* zone[0] - zone[6] point to to direct blocks
     * - for indoe which is virtual device, zone[0] used to store device index
     * zone[7] points to a indirect block table
     * zone[8]  points to a double indirect block table (unused)
     * so this file system's real max file size = (7 + 512) * 1024 byte = 519 kb
     */
    uint16_t zone[9]; 
};

/* inode status flag */
#define I_BUSY 0x1
#define I_VALID 0x2

/* in-memorty inode */
struct inode {
    uint16_t dev;   // be 0 forever
    uint32_t ino;
    uint16_t ref;
    uint16_t flags;
    uint16_t atime; // (unused)
    uint16_t ctime; // (unused)
    
    uint16_t mode;
    uint16_t uid;   // (unused)
    uint32_t size;
    uint32_t mtime; // (unused)
    uint8_t gid;    // (unused)
    uint8_t nlinks;
    uint16_t zone[9];
};

#define NAME_LEN 14
#define NDIRECT 7
#define NINDIRECT (BSIZE/sizeof(uint16_t))  // 1024/2 = 512
#define NDUAL_INDIRECT (BSIZE*BSIZE/sizeof(uint16_t)) // (unused)

#define MAXFILE (NDIRECT + NINDIRECT)       // 512 + 7 kb

/* minix directroy entry */
struct dir_entry {
    uint16_t ino;
    char name[NAME_LEN];
};

/* inode per block */
#define IPB (BSIZE/sizeof(struct inode))
/* bit number a  block contain (1 byte has 8 bits)*/
#define BPB (BSIZE*8)

/* block contain inode i 
 * NB: inode number starts at 1 */
#define IBLK(sb, i) (2 + ((sb).imap_blk) + ((sb).zmap_blk) + ((i) - 1)/IPB)

/* bitmap contain inode i*/
#define IMAP_BLK(sb, i) (2 + (i - 1)/BPB)
/* bitmap contain block z */
#define ZMAP_BLK(sb, b) (2 + sb.imap_blk + (b)/BPB)


// ################################# stat.h #################################

/* - - - - - - - | - - - - - - - - - 
 * r d   p         r w x r w x r w x
 */
/*ref: http://minix1.woodhull.com/manpages/man2/stat.2.html */
/* http://faculty.qu.edu.qa/rriley/cmpt507/minix/stat_8h-source.html#l00016 */

/* octal */
#define S_IFMT  0170000    /* type of file */
#define S_IFIFO 0010000    /* named pipe */
#define S_IFCHR 0020000    /* character special */
#define S_IFDIR 0040000    /* directory */
#define S_IFBLK 0060000    /* block special */
#define S_IFREG 0100000    /* regular */
#define S_IFLNK 0120000    /* symbolic link (Minix-vmd) */

#define S_RWX 0000777

#define S_ISREG(m)      (((m) & S_IFMT) == S_IFREG)     /* is a reg file */
#define S_ISDIR(m)      (((m) & S_IFMT) == S_IFDIR)     /* is a directory */
#define S_ISCHR(m)      (((m) & S_IFMT) == S_IFCHR)     /* is a char spec */
/* all device are regard as char device */
// #define S_ISBLK(m)      (((m) & S_IFMT) == S_IFBLK)     /* is a block spec */
// #define S_ISLNK(m)      (((m) & S_IFMT) == S_IFLNK)     /* is a symlink */
// #define S_ISFIFO(m)     (((m) & S_IFMT) == S_IFIFO)     /* is a pipe/FIFO */

 struct stat {
     uint16_t dev;
     uint16_t ino;
     uint16_t mode;
     uint32_t size;
};

// ################################# inode.h #################################

#define NINODE 500 // length of inodes cache

void inode_init();
struct inode* ialloc(uint16_t dev);
struct inode* iget(uint16_t dev, uint16_t ino);
struct inode* idup(struct inode *ip);
void iput(struct inode *ip);
void ilock(struct inode *ip);
void iunlock(struct inode *ip);
void iunlockput(struct inode *ip);
void iupdate(struct inode *ip);
int iread (struct inode *ip, char *dest, uint32_t off, uint32_t n);
int iwrite(struct inode *ip, char *src, uint32_t off, uint32_t n);
void istat(struct inode *ip, struct stat *st);
void print_i(struct inode *ip);


// ################################# p2i.h #################################

struct inode *p2i(char *path);
struct inode *p2ip(char *path, char *name);


// ################################# sysfile.h #################################

#define O_RONLY     0x0
#define O_WONLY     0x1
#define O_RW        0x2
#define O_CREATE    0x4

int sys_open();
int sys_write();
int sys_read();
int sys_close();
int sys_fstat();
int sys_mkdir();
int sys_mknod();
int sys_chdir();
int sys_link();
int sys_unlink();
int sys_dup();
int sys_pipe();


// ################################# dir.h #################################

struct inode* dir_lookup(struct inode *dirip, char *name, uint32_t *poff);
int dir_link(struct inode *dirip, char *name, struct inode *fip);
int dir_isempty(struct inode *dip);


// ################################# file.h #################################

#define NFILE 128

#define F_NONE  0x0
#define F_PIPE  0x1 
#define F_INODE 0x2

struct file {
    uint8_t type;
    uint16_t ref;
    uint8_t readable;
    uint8_t writeable;
    struct pipe *pipe;
    struct inode *ip;
    uint32_t off;
};


void file_init();
struct file *falloc();
struct file *fdup();
int fread(struct file *fp, char *addr, uint32_t n);
int fwrite(struct file *fp, char *addr, uint32_t n);
int fstat(struct file *fp, struct stat *st);
void fclose(struct file *fp);


// ################################# buf.h #################################

#define B_BUSY  0x1 // buffer is locked
#define B_VALID 0x2 // has been read form disk
#define B_DIRTY 0x4 // need to be written to disk

struct buf {
    char flags;     // B_BUSY B_VALID B_DIRTY
    char dev;       // only one disk, dev = 0
    uint32_t blkno;   // linear block address = sector number
    struct buf *prev;   // LRU Cache List 双向
    struct buf *next;
    struct buf *qnext;  // 磁盘操作请求队列, 单向
    char data[BSIZE];
};


// ################################# bcache.h #################################

#define NBUF 128

void bcache_init();
struct buf* bread(char dev, uint32_t blkno);
void bwrite(struct buf* b);
void brelse(struct buf *b);


// ################################# bitmap.h #################################

int balloc(uint16_t dev);
void bfree(uint16_t dev, uint16_t blkno);
int _ialloc(uint16_t dev);
void _ifree(uint16_t dev, uint16_t ino);


// ################################# sb.h #################################

void read_sb(int dev, struct super_block *sb);

#endif
