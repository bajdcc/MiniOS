/* ide.c
 * This file is modified form xv6
 * use DMA, assume we hava only a disk
 */
 
// std
#include <type.h>
#include <asm.h>
// x86
#include <isr.h>
// lib
#include <string.h>
#include <print.h>
// driver
#include <ide.h> 
// fs
#include <fs.h>


/* 缓冲区定义在 bcache.c */
static struct buf *idequeue = NULL;    // buffer queue of disk request

/* wait for disk until it ready, checkerror when checkerr=1 */
static int ide_wait(int checkerr) {
    int timeout = 20000;
    int r;
    /* 循环检测直到不再IDE_BSY */
    while ((r = inb(IDE_PORT_STATUS)) & IDE_BSY) {
        --timeout;
    }
    if (checkerr && (r & (IDE_DF|IDE_ERR))) {
        return ERROR;
    }
    return OK;
}

static void ide_start(struct buf *b) {
    ide_wait(0);

    /* get physical block number */
    uint32_t phy_blkn = b->blkno*(BSIZE/PHY_BSIZE);

    outb(IDE_PORT_ALTSTATUS, 0);  // generate interrupt

    /* number of sectors, read 2 sector for once  */
    outb(IDE_PORT_SECT_COUNT, BSIZE/PHY_BSIZE);  

    outb(IDE_PORT_LBA0, phy_blkn & 0xff);
    outb(IDE_PORT_LBA1, (phy_blkn  >> 8) & 0xff);
    outb(IDE_PORT_LBA2, (phy_blkn >> 16) & 0xff);
    /* IDE_PORT_CURRENT = 0x101dhhhh d = driver hhhh = head*/
    outb(IDE_PORT_CURRENT, 0xe0 | ((b->dev & 1) << 4) | ((phy_blkn >> 24) & 0x0f)); 

    if(b->flags & B_DIRTY) {   // write
        outb(IDE_PORT_CMD, IDE_CMD_WRITE);
        outsl(IDE_PORT_DATA, b->data, BSIZE/4);
    } else {                   // read
        outb(IDE_PORT_CMD, IDE_CMD_READ);
    }
}

void ide_handler(struct int_frame *r) {
    struct buf *b;
    /* if queue is empty */
    if ((b = idequeue) == 0) {
        return; // nothing to do
    }
    /* remove the head of queue */
    idequeue = b->qnext;
    if (!(b->flags & B_DIRTY) && (ide_wait(1) >= 0)) {
        insl(IDE_PORT_DATA, b->data, BSIZE/4);
    }

    /* 缓冲区可用 */
    b->flags |= B_VALID;
    /* 数据是最新的 */
    b->flags &= ~B_DIRTY;

    if (idequeue) {
        ide_start(idequeue);
    }
}

void ide_init() {
    /* NB: 一切从简, 不需要检测磁盘的存在 */
    /* IRQ14 = Primary ATA Hard Disk */
    irq_install(IRQ_IDE, ide_handler);
    ide_wait(0);
}

void ide_rw(struct buf *b) {
    /* append b to idequeue */
    struct buf **pp;
    b->qnext = 0; 
    for (pp = &idequeue; *pp; pp = &(*pp)->qnext);
    *pp = b;
    
    /* if b is the head of queue, read/write it now */
    if (idequeue == b){
        ide_start(b);
    }
    // wait for the request to finish 
    while ((b->flags & (B_VALID|B_DIRTY)) != B_VALID) {
        hlt();
    }
}

struct buf buffer;  // used for test
void ide_test() {
    buffer.dev = 0;
    buffer.blkno= 10;
    buffer.flags = B_BUSY;
    ide_rw(&buffer);
    
    int i;
    
    for (i = 0; i < BSIZE; i++) {
        buffer.data[i] = 1;
    }
    buffer.flags = B_BUSY|B_DIRTY;
    ide_rw(&buffer);

    buffer.flags = B_BUSY;
    ide_rw(&buffer);
}
