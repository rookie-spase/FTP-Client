#include "EveForge.h"

// ==================================================================================================================
#include <event2/buffer.h>
#include <stdlib.h>

// 数据结构：[包大小(4字节)][包数据]
// 从缓冲区中提取数据包
// 参数:
// buffer - 指向evbuffer的指针
// packet_size - 指向数据包大小的指针
// packet_data - 指向数据包数据的指针
// 返回值:
// 成功时返回0，失败时返回-1
int EveForge_ExtractPacket(evbuffer* buffer, size_t* packet_size, char** packet_data)
{
    size_t total_length = evbuffer_get_length(buffer);
    // 检查是否至少有4字节来读取包大小
    if (total_length < 4) {
        return -1;
    }
    // 读取包大小
    ev_uint32_t packet_length;
    evbuffer_copyout(buffer, &packet_length, 4);
    // 检查总长度是否足够包含整个包
    if (packet_length + 4 > total_length) {
        return -1;
    }
    // 分配内存以存储包数据
    char* data = (char*)malloc(packet_length);
    if (data == NULL) {
        return -1; // 内存分配失败
    }
    // 移除包大小信息并提取包数据
    evbuffer_drain(buffer, 4);
    evbuffer_remove(buffer, data, packet_length);
    *packet_data = data;
    *packet_size = packet_length;
    return 0;
}
// ==================================================================================================================

// 输出一个buffer的内容
// 输出格式: Block 次数 size-大小 ['内容']
void EveForge_ReadBuffer(evbuffer* buffer) 
{
    static unsigned int count;

    struct iovec Read_Ptr[2];
    int n_vecs = evbuffer_peek(buffer, 5, NULL, Read_Ptr, 2);  // 获取实际填充的 iovec 数量

    for (int i = 0; i < n_vecs; ++i) {  // 只遍历实际返回的 iovec 数量
        printf("Block %d size-%ld\t:['%s']\n", ++count, Read_Ptr[i].iov_len, (char*)Read_Ptr[i].iov_base);
    }
}


// ==================================================================================================================

// 读取buffer的ip和端口，然后放到后面两个参数
// 包结构：一共8个字节，前2个字节表示{ip类型}，之后2个字节为{端口}，最后4个字节为{ip地址}
// 参数:
// buffer - 指向evbuffer的指针
// port - 指向端口号的指针
// addr - 指向IP地址的指针
// 返回值:
// 成功时返回0，失败时返回-1或-2
#include <string.h>
int EveForge_ParseSocket(evbuffer* buffer, ev_uint32_t *addr, unsigned short* port)
{
        unsigned char* buf = evbuffer_pullup(buffer,8);
        
        if(buf == nullptr){
                return -1;
        }
        
        if(buf[0] != 4 || buf[1] != 1){
                perror("Unknown Contrast");
                return -2;
        }
        memcpy(port,buf+2,2);
        memcpy(addr,buf+4,4);
        *port = ntohs(*port);
        *addr = ntohl(*addr);

        evbuffer_drain(buffer,8);

        printf("addr:[%u]\nport:[%u]\n",*addr,*port);
        return 0;
}
// ==================================================================================================================

// 一行一行读取Buffer之中的东西
// 读取出的格式：Str 第几行: ['内容']
void EveForge_ReadLineWithBuffer(struct evbuffer* buffer)
{
    static unsigned int count = 0;

    size_t len;
    while (evbuffer_get_length(buffer) > 0) {  // 检查缓冲区是否为空
        char* str = evbuffer_readln(buffer, &len, EVBUFFER_EOL_LF);  // 以\n作为一行的结尾
        if (str != NULL) {
            printf("Str %u: ['%s']\n", ++count, str);
            free(str);  // 释放由 evbuffer_readln 返回的字符串
        }
    }
}

// ==================================================================================================================

// 子串str在buffer中的出现次数
int EveForge_CountStrInBuffer(struct evbuffer* buffer, const char* str) 
{
    int len = strlen(str);
    if (len <= 0) {  // str 不能为空
        perror("str is empty");
        return -1;
    }

    int count = 0;
    struct evbuffer_ptr pos = evbuffer_search(buffer, str, len, NULL);  // 从头开始搜索

    while (pos.pos != -1) {  // 当找到字符串时
        ++count;
        // 从当前找到的位置后移动一个字符开始下一次搜索
        evbuffer_ptr_set(buffer, &pos,  1, EVBUFFER_PTR_ADD);
        pos = evbuffer_search(buffer, str, len, &pos);
    }
    return count;
}
// ==================================================================================================================
#include <unistd.h>
// 发送buffer的size个字节到fd
size_t EveForge_SendByte(evbuffer* buffer, const int size, const int fd)
{
    if (buffer == NULL || fd < 0) {
        // 错误处理：检查参数有效性
        return -1;
    }
    evbuffer_iovec* vec_out;
    size_t total_writed = 0;

    int vec_size = evbuffer_peek(buffer, size, NULL, NULL, 0);  // 这个返回值表示，读取buffer中size个字节需要多少个evbuffer_iovc
    if (vec_size <= 0) return -1;

    vec_out = (evbuffer_iovec*)malloc(sizeof(evbuffer_iovec) * vec_size);
    if (vec_out == NULL) return -1;

    int n = evbuffer_peek(buffer, size, NULL, vec_out, vec_size);   // 实际用到的vec_ioc的数量
    if (n <= 0) {
        free(vec_out);
        return -1;
    }
    
    for (int i = 0; i < n; ++i) {
        size_t len = vec_out[i].iov_len;

        if ((total_writed + len) > size) {   //  确保发送的数据严格发送的是size个字节
            len = size - total_writed;
        }
        
        int current_writed = write(fd, vec_out[i].iov_base, len);

        if (current_writed <= 0) {  // 如果啥都没写
            break;
        }
        total_writed += current_writed;
    }

    free(vec_out);

    return total_writed;
}

// 从buffer的target_string开始发送size个字节到fd
size_t EveForge_SendStringFromBuffer(evbuffer* buffer, const char* target_string, const int size,const int fd) 
{
    if (buffer == NULL || target_string == NULL || fd < 0) {
        // 错误处理：检查参数有效性
        return -1;
    }
    // 查找目标字符串在buffer中的位置
    size_t buffer_length = evbuffer_get_length(buffer);
    unsigned char* buffer_data = evbuffer_pullup(buffer, buffer_length);
    unsigned char* found_position = (unsigned char*)memmem(buffer_data, buffer_length, target_string, strlen(target_string));
    if (found_position == NULL) {
        // 如果没有找到字符串
        return -1;
    }
    // 计算目标字符串的位置偏移
    size_t offset = found_position - buffer_data;
    // 从找到的位置开始，准备一个新的evbuffer
    evbuffer* temp_buffer = evbuffer_new();
    if (temp_buffer == NULL) {
        return -1;
    }
    // 从原始buffer中复制数据到临时buffer
    if (evbuffer_add(temp_buffer, found_position, size) == -1) {
        evbuffer_free(temp_buffer);
        return -1;
    }
    // 使用Send_Byte发送数据
    size_t sent_bytes = EveForge_SendByte(temp_buffer, size, fd);
    evbuffer_free(temp_buffer);
    return sent_bytes;
}




// ==================================================================================================================
#include <stdlib.h>
#include <string.h>
#include <event2/buffer.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

struct file_resource {
    int reference_count;
    char *data;
    size_t size;
};

struct file_resource *EveForge_NewResourceFromFile(const char *filename)
{
    int fd = open(filename, O_RDONLY);
    if (fd == -1) return NULL;

    struct stat statbuf;
    if (fstat(fd, &statbuf) == -1) {
        close(fd);
        return NULL;
    }

    struct file_resource* resource = (struct file_resource*)malloc(sizeof(struct file_resource));
    if (!resource) {
        close(fd);
        return NULL;
    }

    resource->data = (char*)malloc(statbuf.st_size);
    if (!resource->data) {
        free(resource);
        close(fd);
        return NULL;
    }

    if (read(fd, resource->data, statbuf.st_size) != statbuf.st_size) {
        free(resource->data);
        free(resource);
        close(fd);
        return NULL;
    }

    resource->size = statbuf.st_size;
    resource->reference_count = 1;
    close(fd);

    return resource;
}

void EveForge_FreeResource(struct file_resource* resource)
{
    resource->reference_count -= 1;
    if (resource->reference_count <= 0) {
        free(resource->data);
        free(resource);
    }
}

static void cleanup(const void* data, size_t len, void* arg)
{
    EveForge_FreeResource((struct file_resource*)arg);
}

// 让buf通过add_reference引用指定文件的数据。然后发送到fd.
void EveForge_SpoolResourceToEvbuffer(struct evbuffer* buf, struct file_resource* fr)
{
    ++fr->reference_count;
    evbuffer_add_reference(buf, fr->data, fr->size, cleanup, fr);
}

// 测试函数
void EveForge_Send_File_to_fd(const char* filename, int fd)
{
    struct evbuffer* test_buf = evbuffer_new();
    struct file_resource* test_resource = EveForge_NewResourceFromFile(filename);

    if (test_resource && test_buf) {
        EveForge_SpoolResourceToEvbuffer(test_buf, test_resource);
        printf("Resource spooled to evbuffer with reference count: %d\n", test_resource->reference_count);
        EveForge_SendByte(test_buf, test_resource->size, fd);
    }

    if (test_buf) {
        evbuffer_free(test_buf);
    }

    if (test_resource) {
        EveForge_FreeResource(test_resource);
    }
}


// ==================================================================================================================

// 生成数据到指定的内存区域
int EveForge_GenerateData(void* data, int length) 
{
    if (data == NULL || length <= 0) {
        return -1;
    }

    // 填充数据，这里简单使用0xFF作为示例
    memset(data, 0xFF, length);
    return 0;
}

// 高级的evbuffer_add。：申请2k的空间，然后添加测试用的数据，最后提交。
// 结果：buffer的size个字节都是0xFF
void EveForge_AdvanceEvbufferAdd(evbuffer* buffer,int size)
{
  struct evbuffer* buffer1 = buffer;
    int apply_for_place = size;
    struct evbuffer_iovec vec[2];

    int actualy_used_vec_num = evbuffer_reserve_space(buffer1, apply_for_place, vec, 2);  // 申请2k的数据
    
    for(int i = 0; i < actualy_used_vec_num; ++i){
        int len = vec[i].iov_len;
        if(len > apply_for_place){
            len = apply_for_place;  
        }
        
        if(EveForge_GenerateData(vec[i].iov_base, len) < 0){
            return;
        }
        vec[i].iov_len = len;
        apply_for_place -= len;
    }

    evbuffer_commit_space(buffer1, vec, actualy_used_vec_num);  // 将2k的数据提交到buffer1

    //Read_Buffer(buffer1);
    
    // evbuffer_free(buffer1);
}


// ==================================================================================================================


// 当数据从buf之中移出了1M就向终端输出一个'.'
#include <stdio.h>
#include <stdlib.h>
#include <event2/buffer.h>
struct total_processed{  // 记录buf移出了多少个字节，且作为参数传递给buf的回调
    size_t n;
};
void EveForge_CountMegabytesCb(struct evbuffer* buf, const struct evbuffer_cb_info* info, void* arg)
{
    struct total_processed* tp = (struct total_processed*)arg;
    size_t old_count = tp->n;
    size_t new_count = info->n_added + evbuffer_get_length(buf);
    size_t Count_Of_1M = 0;
    
    tp->n += info->n_deleted;
    Count_Of_1M = (tp->n >> 20) - (old_count >> 20);  // 移出了几个1M
    
    for(size_t i = 0; i < Count_Of_1M; ++i){
        putc('.', stdout);
        putc('\n',stdout);
    }
}