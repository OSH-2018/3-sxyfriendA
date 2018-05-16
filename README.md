# 3-sxyfriendA

## 最新一次修改
1、更改了write中的文件st.size的计算方法
        
	if（node->st.size <offset + size）
        node->st.size = offset +size;
	
   使得其不会发生截断
   
2、更改了root指针相关问题，去除root指针，将文件头结点信息存储于super block的第6个位置

3、更改了出错返回值，根据需要返回-ENOENT，-ENOSPC，-ENOMEM

## 文件系统结构
该文件系统共64k个block，每个block的大小为64k。其中第0个block为super block，第1至第128个block为inode block，之后的block为data block。

当前的super block中只有五个整数，第一个整数保存每块的大小，第二个整数保存块数，第三个整数保存使用量，第四个整数保存数据块剩余量，第五个整数保存文件块剩余量。该文件系统的基础数据可在宏定义中进行修改，包括inode block的个数，总block数以及每个block的大小。

文件结点的结构体如下所示

       struct filenode {
       char filename[40];//name
	     int32_t filenode_num;//编号
	     int32_t used_num;//本块中已使用的页的个数 
       struct stat st;//文件信息       struct filenode *next_file;//指向下一个文件的文件信息
       int32_t data[(BLOCK_SIZE-300)/4];//总存储量约1G(不足)
       };//filenode 
      
其中filename为文件名，filenode_num为本块的编号，即对应于程序中指针mem的编号，used_num为本块中已使用的页的个数，在之后的写操作中用于判别是否有足够用的空间，st用于存储文件信息，next_file用于将所有文件串成一个链表，便于之后文件的查找，data为存储数据的数据块编号。

data block中为文件数据每块中存储数据恰为64k，每个文件的存储上限约为1G(不足1G,实际大小为16k(BLOCK_SIZE-300)/4)，最大文件数目暂定为128个。

## 函数

oshfs_init 用于创建文件系统，初始化超级块
oshfs_getattr用于获取文件信息

oshfs_reddir用于获取所有文件的filename

oshfs_mknod用于创建新的文件结点

oshfs_write用于写文件

oshfs_read用于读文件

oshfs_truncate用于改变文件大小

oshfs_unlink用于删除文件

find_empty_inode_block用于寻找空的inode结点

find_empty_data_block用于寻找空的data结点

realloc_block重新分配data block块

## 重要函数的算法
realloc_block，通过文件改变后的大小计算所需数据块，再跟原有数据块数目进行比较，若原有数据块少，则再分配新的数据块，若原有数据块多，则释放多余的数据块。

oshfs_write,先计算新的所需数据块的个数以及offset所在位置，使用realloc_block分配内存空间，再通过mem指针以及memcpy函数进行写入即可。

oshfs_read，先计算offset所在位置，再从对应位置开始使用memcpy,一块一块得取出数据到buf，并在其中按序排列即可。

oshfs_unlink,先处理文件链表，将对应文件从文件链表中删除，并保持文件链表的连续性，再释放对应的data block的内存空间，最后释放inode block对应的内存空间。

find_empty_indoe_block，由于个数较少，每次遍历以便所有指向inode块的mem指针即可

find_empty_data_block,设置全局变量存储最新增加的data block的编号，每次寻找data block直至找到空data block或遍历所有data block止(当遍历到最后一个block时)下一个判断是否为空的块为第129块（即第一个数据块）。

## 文件系统特点
1、data block每块存储数据恰为64k 

2、由于filenode中存在一个按顺序指向对应data block结点的数组，故在读执行读写操作时不需要遍历offset前的所有数据块，效率较高

3、super block存在，可以存储有关文件系统的信息，当前仍存在较大空间存储信息，可在之后进行拓展

4、inode block存在，便于维护、修改文件的各种原信息，当前filenode结点中仍存在约100字节的位置，可供文件信息的拓展

## 附
1、开始时先实现的为Linux中文件管理系统的模式，在inode之间新增加一个链表，用next_block指针相连，通过这种方式可以使得单一文件可以同时具有多个文件结点，通过这种方法，可以使得能处理的文件大小上线提高，实现时发现read和write函数中关于offset和size的数据结点位置处理较为麻烦……中途放弃，改为每个文件只是用一个结点

2、鉴于能处理的最大文件大小，无法处理当前示例代码中的2GB的文件，但当将2000改为1000后示例代码可正常执行。
