// std
#include <type.h>
#include <asm.h>
// lib
#include <string.h>
#include <print.h>
// fs
#include <fs.h>
// dev
#include <dev.h>

/* inodes cache */
static struct inode icache[NINODE];

extern struct dev dtable[];  // extern variable form dev/dev.c, list of all device

void inode_init() {
    memset(icache, 0, sizeof(struct inode)*NINODE);
}

/* find the inode with specific dev and inum in memory, 
 * if not found, use first empty inode */
struct inode* iget(uint16_t dev, uint16_t ino) {
    struct inode *ip, *empty;

    empty = 0;
    for (ip = &icache[0]; ip < &icache[NINODE]; ip++) {
        /* this inode is cached */
        if (ip->ref > 0 && ip->dev == dev && ip->ino == ino) {
            ip->ref++;
            return ip;
        }
        /* remember the first free inode */
        if (empty == 0 && ip->ref == 0) {
            empty = ip;
        }
    }
    
    ip = empty;
    ip->dev = dev;
    ip->ino = ino;
    ip->ref = 1;
    ip->flags = 0;
    return ip;
}

/* alloc a inode with specific type */
struct inode* ialloc(uint16_t dev) {
    int ino;
    struct inode *ip;

    ino = _ialloc(dev);
    ip = iget(dev, ino);

    ip->nlinks = 0;
    ip->size = 0;
    ip->mode = S_RWX; 
    memset(ip->zone, 0, sizeof(ip->zone));
    return ip;
}

/* copy a in-memory inode to disk */
void iupdate(struct inode *ip) {
    struct buf *bp;
    struct d_inode *d_ip;
    struct super_block sb;

    read_sb(ip->dev , &sb);

    bp = bread(ip->dev, IBLK(sb, ip->ino));
    d_ip = (struct d_inode *)bp->data + (ip->ino - 1)%IPB;

    /* ip -> d_ip */
    d_ip->mode = ip->mode;
    d_ip->uid = ip->uid;
    d_ip->size = ip->size;
    d_ip->mtime= ip->mtime;
    d_ip->gid = ip->gid;
    d_ip->nlinks = ip->nlinks;
    memcpy(d_ip->zone, ip->zone, sizeof(ip->zone));

    bwrite(bp);
    brelse(bp);
}

/* truncate inode 
 * only called when the inode has no links to it 
 * and not any in-memory ref to it
 */
static void itrunc(struct inode *ip) {
    uint32_t i, j;
    struct buf *bp;
    uint16_t *zone2;
    
    /* free DIRECT block */
    for (i = 0; i < NDIRECT; i++) {
        if (ip->zone[i]) {
            bfree(ip->dev, ip->zone[i]);
            ip->zone[i] = 0;
        }
    }

    /* free INDIRECT block */
    if (ip->zone[NDIRECT]) {
        bp = bread(ip->dev,ip->zone[NDIRECT]);
        zone2 = (uint16_t *)bp->data;
        for (j = 0; j < NINDIRECT; j++) {
            if (zone2[j]) {
                bfree(ip->dev, zone2[j]);
                zone2[j] = 0;
            }
        }
        brelse(bp);
        bfree(ip->dev, ip->zone[NDIRECT]);
        ip->zone[NDIRECT] = 0;
    }

    /* DAUL INDIRECT block is unused*/

    ip->size = 0;
    iupdate(ip);
}

/* reference of ip + 1 */
struct inode* idup(struct inode *ip) {
    ip->ref++;
    return ip;
}

/* reference of ip - 1 
 * if it is the last ref, this node can be recycle (ref = 1) (?)
 * and if this inode have no link to it (nlinks = 0)
 * free this inode in the disk
 */
void iput(struct inode *ip) {
    if (ip->ref == 1 && ip->flags & I_VALID && ip->nlinks == 0) {
    
        /* can not free a locked inode */
        ip->flags |= I_BUSY;
        itrunc(ip);

        _ifree(ip->dev, ip->ino);

        // ip-> ref == 0
        // ip->flags = 0;
    }
    ip->ref--;
}

/* lock given inode with flag I_BUSY
 * read the inode form disk if necessary
 * non-ref inode can not be lock (ref = 0)
 */
void ilock(struct inode *ip) {
    struct buf *bp;
    struct d_inode *d_ip;
    struct super_block sb;

    read_sb(ip->dev, &sb);

    int timeout = 20000;
    while (ip->flags & I_BUSY) {
        timeout--;
        hlt();
    }
    ip->flags |= I_BUSY;

    /* need to be read form disk */
    if (!(ip->flags & I_VALID)) {
        bp = bread(ip->dev, IBLK(sb, ip->ino));
        d_ip = (struct d_inode *)bp->data + (ip->ino - 1)%IPB;

        /* ip -> dip */
        ip->mode = d_ip->mode;
        ip->uid = d_ip->uid;
        ip->size = d_ip->size;
        ip->mtime = d_ip->mtime;
        ip->gid = d_ip->gid;
        ip->nlinks = d_ip->nlinks;

        memcpy(ip->zone, d_ip->zone, sizeof(d_ip->zone));

        brelse(bp);
        ip->flags |= I_VALID;
    }
}

/* unlock a inode */
void iunlock(struct inode *ip) {
    ip->flags &= ~I_BUSY;
}

// (?)
void iunlockput(struct inode *ip) {
    iunlock(ip);
    iput(ip);
}

/* return the disk block address of the nth block in given inode
 * if not such  block, alloc one
 */
static uint32_t bmap(struct inode *ip, uint32_t bn) {
    uint16_t zno;
    uint16_t *zone2;
    struct buf *bp;

    if (bn < NDIRECT) {
        if ((zno = ip->zone[bn]) == 0) {
            zno = ip->zone[bn] = balloc(ip->dev);
        }
        return zno;
    }

    bn -= NDIRECT; 

    if ((ip->zone[NDIRECT]) == 0) {
        ip->zone[NDIRECT] = balloc(ip->dev);
    }

    bp = bread(ip->dev, ip->zone[NDIRECT]);
    zone2 = (uint16_t *)bp->data;

    if ((zno = zone2[bn]) == 0) {
        zno = zone2[bn] = balloc(ip->dev);
        bwrite(bp);
    }

    brelse(bp);
    return zno;
}

/* read data from inode */
int iread(struct inode *ip, char *dest, uint32_t off, uint32_t n) {
    uint32_t tot, m;
    struct buf *bp;

    /* is a char device? */
    if (S_ISCHR(ip->mode)) {
        return dtable[ip->zone[0]].read(ip, dest, n);
    }

    /* 偏移过大 || 溢出*/
    if (off > ip->size || off + n < off) {
        panic("iread: incorrect offet");
        return -1;
    }

    if (off + n > ip->size) {
        n = ip->size - off;
    }

    /*  tot: 已读取的字节数
     *  n: 要读取的字节数
     *  off: 当前文件指针偏移
     *  m: 当次循环要读取的字节数
     */
    for (tot = 0; tot < n; tot += m, off += m, dest += m) {
        bp = bread(ip->dev, bmap(ip, off/BSIZE));
        /* 每个循环读取的数据可分两种情况：
         * - 从当前文件偏移读到该扇区结束
         * - 从当前文件偏移读到 *满足要读取的字节数*
         * 每次选择较小的那个
         */
        m = min(n - tot, BSIZE - off%BSIZE);
        /* 从当前偏移相对于该扇区的偏移处起开始读取
         * (在中间的循环这个值常常是0) 
         */
        memcpy(dest, bp->data + off%BSIZE, m);
        brelse(bp);
    }

    return n;
}

/* write data to inode */
int iwrite(struct inode *ip, char *src, uint32_t off, uint32_t n) {
    uint32_t tot, m;
    struct buf *bp;

    /* is a char device? */
    if (S_ISCHR(ip->mode)) {
        return dtable[ip->zone[0]].write(ip, src, n);
    }

    if (off > ip->size || off + n < off ) {
        panic("iwrite: incorrect offset or read length\n");
        return ERROR;
    }

    if (off + n > MAXFILE*BSIZE) {
        panic("iwrite: size out of range\n");
        return ERROR;
    }

    for (tot = 0; tot < n; tot += m, src += m, off += m) {
        bp = bread(ip->dev, bmap(ip, off/BSIZE));
        m = min(n - tot, BSIZE - off%BSIZE);
        memcpy(bp->data + off%BSIZE, src, m);
        bwrite(bp);
        brelse(bp);
    }

    /* if alloc new block to ip, update it's size */
    // n > 0 ?
    if (n > 0 && off > ip->size) {
        ip->size = off;
        iupdate(ip);
    }
    
    return n;
}

void istat(struct inode *ip, struct stat *st) {
    st->dev = ip->dev;
    st->mode = ip->mode;
    st->ino = ip->ino;
    st->size = ip->size;
}
