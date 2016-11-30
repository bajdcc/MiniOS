
/* sb.c
 * read or dump superblock
 * superblock can not be modified
 */
 
// std
#include <type.h>
// libs
#include <string.h>
#include <print.h>
// fs
#include <fs.h>


/* read the super_block */
void read_sb(int dev, struct super_block *sb) {
    struct buf *bp;
    bp = bread(dev, 1);

    memcpy(sb, bp->data, sizeof(*sb));
    brelse(bp);
}
