#define FUSE_USE_VERSION 26
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fuse.h>
#include <sys/mman.h> 
#include<time.h> 
#define BLOCK_SIZE (64 * 1024) //每个block大小为64k  
#define IBLOCK_NUM 128         //设置128个块用于存储文件信息 ，共8000字节 
#define BLOCK_NUM  (64 * 1024)// 共分为64*1024个block 

struct filenode {
    char filename[40];//name
	int32_t filenode_num;//编号
	int32_t next_num;
	int32_t used_num;//本块中已使用的页的个数 
    struct stat st;//文件信息
    struct filenode *next_file;//指向下一个文件的文件信息
    int32_t data[(BLOCK_SIZE-300)/4];//总存储量约1G(不足)
     //int32_t flag;//flag==1表示被使用
	//struct filenode *next_block;//指向下一个该文件的信息block,next_block为空表示该块已可表示所有信息
	//int32_t filenode_num;//本文件所使用的块的个数 
	//除去前面各信息占据位置后即为能存储的页号信息数 
};//filenode 

static const size_t size = 4 * 1024 * 1024 * (size_t)1024;//总大小为4G 
static void *mem[64 * 1024] ;//指向页的point 

//static struct filenode *root = NULL;//root指向最新建立的的文件的第一页
//int datablock_num_now = 128;//当前处理的datablock的编码 
	
static struct filenode *get_filenode(const char *name)
{
    struct filenode *node = (struct filenode*)mem[*( (int32_t*)mem[0] + 5 )];//point to root
	if(*( (int32_t*)mem[0] + 5 )==0)//don't have file
	return NULL;
    while(node->filenode_num) {
        if(strcmp(node->filename, name + 1) != 0)
	    {
			if(node->next_num) break;       //the next inode block is the super block break.
            else node = node->next_file;
		}
        else
            return node;
    }
    return NULL;
}

int find_empty_inode_block()
{//寻找空的文件信息块，总个数为IBLOCK_NUM=128个，在内存中位置为1至128页
    int32_t inode_empty_num; //当前剩余的文件信息块剩余个数 
    //int32_t use_flag;
	struct filenode *node;
    int32_t i;
    inode_empty_num = *( (int*)mem[0] + 4 );
	if(inode_empty_num <= 0) return -1 ;//无剩余块，无法分配新的文件
	for(i=1;i<=IBLOCK_NUM;i++)
	{ //由于IBLOCK块较少，直接按顺序查找未使用项
	    //node =  (struct filenode *)mem[i] 
		//use_flag = node->flag;
		if(mem[i] == NULL)//mem[i]未被分配 
		{
			//node->flag == 1;       //表示已经使用 
			mem[i] = mmap(NULL, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			*( (int32_t*)mem[0] + 4 ) -=1;//可使用的文件属性块个数减一 
			return i;
		}
	}
	return -1;	
}
int find_empty_data_block()
{
	int32_t data_empty_num;  //当前数据信息块剩余个数
	int32_t datablock_num_now;
	int i; 
	data_empty_num =  *((int32_t*)mem[0] + 3);
	datablock_num_now = *((int32_t*)mem[0] + 6);
	if(data_empty_num<=0) return -1;
	else
	{
		for(i=datablock_num_now+1;i!=datablock_num_now;i++)
		{
			if(i == BLOCK_NUM + 1)
			i=128;
			if(mem[i]==NULL)
			{
				datablock_num_now=i;
				mem[i] = mmap(NULL, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			    *( (int32_t*)mem[0] + 3 ) -=1;//可使用的文件数据块个数减一 
                *( (int32_t*)mem[0] + 6 ) =i;//point to the neweat block
				return i;
			}
		}
	}
	return -1;
} 

static void create_filenode(const char *filename, const struct stat *st)
{	//创建文件节点
	int32_t k=find_empty_inode_block();//分配块存放文件属性
	if (k<=0 || k>128) {
		return;
	}//错误情况结束该函数 
	if(mem[k]==NULL)
	return ;
	struct filenode *new = (struct filenode *)mem[k];	
	strcpy(new->filename, filename);//复制文件名
	memcpy(&(new->st), st, sizeof(struct stat));//复制文件属性
	//new->flag = 0;
	new->used_num = 0;
	new->filenode_num = k; 
//	new->next_block  = NULL;
    new->next_num = *( (int32_t*)mem[0] + 5 );
	new->next_file = (struct filenode*)mem[*( (int32_t*)mem[0] + 5 )];	
    *((int32_t*)mem[0] + 5 ) = new->filenode_num;//采用头插法，新节点插在根节点之前
	
}

int realloc_block(struct filenode *node,int n)//块重新分配,n3为分配后所需文件属性块个数 
{
	int m = node->used_num;//原文件存储所需要的数据块数
	int i,k=0;
	//struct filenode *temp = node;
	int num=0;
	//int p;//p为所要删除的块 
	if(m<n) 
	{//如果要求的空间比已有的大,分配新增的块
		k=*((int32_t*)mem[0] + 3);//获取空闲块的数量
		if (k<n-m) {
			return -1;
		}//剩余数据块数量不足，函数终止 
		for (i=m;i<n;i++) 
		{//再分配n-m个块 
			k=find_empty_data_block();
			if(k==-1) return -ENOSPC;
			if(mem[k]==NULL) return -ENOMEM;
			node->data[i]=k;
		}//分配新增的块
		node->used_num=n;
	}
	else if(m==n) ;
	else
	{ 
		for (i=n;i<m;i++)//共删去m-n块 
		{ 
		    int t = node->data[i];
			memset(mem[t], 0, BLOCK_SIZE);
		    munmap(mem[t], BLOCK_SIZE);
		    mem[t] = NULL ;
			node->data[i] = 0;
			*( (int32_t*)mem[0] + 3 ) += 1 ;//数据块剩余量+1； 
			//node->used_num-=1; 
		}
	}	
	node->used_num=n;
	return 0;
}

/*int short_block(struct filenode *node,int n)
{
	int m = node->used_num;//原文件存储所需要的数据块数
	int i,k=0;
	//struct filenode *temp = node;
	int num=0;
	//int p;//p为所要删除的块 
		for (i=n;i<m;i++)//共删去m-n块 
		{ 
		    int t = node->data[i];
			memset(mem[t], 0, BLOCK_SIZE);
		    munmap(mem[t], BLOCK_SIZE);
		    mem[t] = NULL ;
			node->data[i] = 0;
			*( (int32_t*)mem[0] + 3 ) += 1 ;//数据块剩余量+1； 
		}
		node->used_num=n;

	return 0; 
}*/
static void *oshfs_init(struct fuse_conn_info *conn)
{
		mem[0] = mmap(NULL, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		//if(mem[0]==NULL)return -ENOMEM;
		memset(mem[0], 0, BLOCK_SIZE);
    //将第一个块设置为超级块，用于存放文件系统的各种属性 
	*( (int32_t*)mem[0] + 0 ) = BLOCK_SIZE ;  //第一个整数保存每块的大小 
	*( (int32_t*)mem[0] + 1 ) = BLOCK_NUM ;   //第二个整数保存块数
	*( (int32_t*)mem[0] + 2 ) = 1+IBLOCK_NUM; //第三个整数保存使用量
	*( (int32_t*)mem[0] + 3 ) = BLOCK_SIZE-1-IBLOCK_NUM; //第四个整数保存数据块剩余量
	*( (int32_t*)mem[0] + 4 ) = IBLOCK_NUM;//第五个整数保存文件块剩余量
	*( (int32_t*)mem[0] + 5 ) = 0;//point to the newest inode block;
	*( (int32_t*)mem[0] + 6 ) = 128;//point to the newest data block
	//1到128块为存储文件信息的块 
	//除第一块外各块中各位均为0 
    return 0;
} 


static int oshfs_getattr(const char *path, struct stat *stbuf)
{//获取文件属性，与示例代码相同 
    int ret = 0;
    struct filenode *node = get_filenode(path);//试图找到与path的同名结点，若不存在则返回null 
    if(strcmp(path, "/") == 0) {//若路径为"/"则设置空stbuf 
        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_mode = S_IFDIR | 0755;
    } 
	else if(node) 
	{//若路径不为"/"则将内存中node ->st数据拷贝到stbuff中 
        memcpy(stbuf,&(node->st), sizeof(struct stat));
    } else 
	{
        ret = -ENOENT;
    }
    return ret;
}

static int oshfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{//读取目录中所有文件的信息，与示例代码相同 
    struct filenode *node = (struct filenode*)mem[*( (int32_t*)mem[0] + 5 )];
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
	if(*( (int32_t*)mem[0] + 5 )==0) return 0;//no file
    while(node) {
        filler(buf, node->filename,&(node->st), 0);
		if(node->next_num==0)break; //the last file
        node = node->next_file;
    }
    return 0;
}

static int oshfs_mknod(const char *path, mode_t mode, dev_t dev)
{//创建文件与示例代码完全相同 
	struct stat st;
	st.st_mode = S_IFREG | 0644;
	st.st_uid = fuse_get_context()->uid;
	st.st_gid = fuse_get_context()->gid;
	st.st_nlink = 1;
	st.st_size = 0;
	create_filenode(path + 1, &st);
	return 0;
}

static int oshfs_open(const char *path, struct fuse_file_info *fi)
{
	return 0;
}

static int oshfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{//改写文件内容 
	struct filenode *node = get_filenode(path);
	if(node==NULL)
	return -ENOENT;
	int i,k;
	int m,n;
	int offset0,seek;
	if(offset + size > node->st.st_size) //if new file is bigger change the file size
	node->st.st_size = offset + size;//修改文件大小
	n=(node->st.st_size - 1)/BLOCK_SIZE+1;//计算新的大小所需要的数据块数（取上整） 
	k=realloc_block(node,n);//重新分配文件所占的data页
	if (k<0) {
		return -ENOSPC;
	}//发生错误结束 
	m=offset/BLOCK_SIZE;//偏移位置所在的块
	offset0=offset%BLOCK_SIZE;//块内偏移量
	seek=0;//已经复制的字节数
	while (size>seek){
		i=node->data[m];
		if (size-seek>BLOCK_SIZE-offset0) k=BLOCK_SIZE-offset0;//当前块装不下 
		else k=size-seek;//当前块装得下，计算当前块要复制的字节数
		memcpy(mem[i]+offset0,buf+seek,k);
		seek+=k;
		offset0=0;//当处理之后的块时偏移量归0 
		m++;
	}
	return size;
}


static int oshfs_truncate(const char *path, off_t size)//改变文件大小（变小去尾，变多加空） 
{
	struct filenode *node = get_filenode(path);
	int k,n;
	n=(size - 1)/BLOCK_SIZE+1;//计算新的大小所需要的数据块数（取上整）
	k=realloc_block(node,n);//重新分配文件所占的页
	node->st.st_size = size;
    return 0;
}

static int oshfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    struct filenode *node = get_filenode(path);
	if(node==NULL)
	return -ENOENT;
    int m,offset0,seek,ret,i;
	m=offset/BLOCK_SIZE;//偏移位置所在的块
	offset0=offset%BLOCK_SIZE;//块内偏移量
	seek=0;//已经复制的字节数
	if(size+offset >= node->st.st_size)
	    size = node->st.st_size -offset;//若超出文件内容，读到文件内容结束为止 
	while (size>seek){
		i=node->data[m];
		if (size-seek>BLOCK_SIZE-offset0) ret=BLOCK_SIZE-offset0;//当前块读不完所有信息 
		else ret=size-seek;//当前块读完所有信息，计算当前块要读的信息数 
		memcpy(buf+seek,mem[i]+offset0,ret);
		seek+=ret;
		offset0=0;//当处理之后的块时偏移量归0 
		m++;
	}	
}

static int oshfs_unlink(const char *path)
{
    struct filenode *node = get_filenode(path);
	if(node==NULL)
	return -ENOENT;
    int used_number,t,k;
    int i;
    struct filenode *temp=(struct filenode*)mem[*( (int32_t*)mem[0] + 5 )]; 
	used_number = node->used_num;//该文件使用的块的个数

	if(temp!=node)  //保证文件系统的链表连续 
	{
		while(temp->next_file != node)
		    temp=temp->next_file;

		temp->next_file=node->next_file;
		temp->next_num = node->next_num;
	}
    else *( (int32_t*)mem[0] + 5 ) = node->next_num;//保证根节点指向最后一个文件

	for(i=0;i<=used_number-1;i++)
	{
		t = node->data[i];
		memset(mem[t], 0, BLOCK_SIZE);
		munmap(mem[t], BLOCK_SIZE);
		mem[t] = NULL ;
		node->data[i] = 0;
		*( (int32_t*)mem[0] + 3 ) += 1 ;//数据块剩余量+1；
	}

	k = node->filenode_num;
	memset(mem[k],0,BLOCK_SIZE);
	munmap(mem[k],BLOCK_SIZE);
	mem[k]=NULL; 

	*( (int32_t*)mem[0] + 4 ) += 1 ;//文件信息块剩余量+1；

    return 0;
}

static const struct fuse_operations op = {
    .init = oshfs_init,//初始化一个文件系统 
    .getattr = oshfs_getattr,//获取文件属性 
    .readdir = oshfs_readdir,//阅读符号链接的目标 
    .mknod = oshfs_mknod,//创建一个文件结点 
    .open = oshfs_open,//文件打开操作 
    .write = oshfs_write,//向一个打开的文件进行写入操作 
    .truncate = oshfs_truncate,//改变文件大小 
    .read = oshfs_read,// 从一个打开的文件中读出数据 
    .unlink = oshfs_unlink,//移除一个文件 
};

int main(int argc, char *argv[])
{
    return fuse_main(argc, argv, &op, NULL);
}
