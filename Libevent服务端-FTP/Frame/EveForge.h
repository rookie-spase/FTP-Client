#ifndef __EVFORGE_H__
#define __EVFORGE_H__

#include <event2/event.h>
#include <event2/bufferevent.h>

int EveForge_ExtractPacket(evbuffer* buffer, size_t* packet_size, char** packet_data); // 从缓冲区中提取数据包 ，数据结构：[包大小(4字节)][包数据]
void EveForge_ReadBuffer(evbuffer* buffer); // 输出格式: Block 次数 size-大小 ['内容']
int EveForge_ParseSocket(evbuffer* buffer, ev_uint32_t *addr, unsigned short* port);  // 读取buffer的ip和端口，然后放到后面两个参数。结构：一共8个字节，前2个字节表示{ip类型}，之后2个字节为{端口}，最后4个字节为{ip地址}
void EveForge_ReadLineWithBuffer(struct evbuffer* buffer);  // 一行一行的读取buffer之中的东西
int EveForge_CountStrInBuffer(struct evbuffer* buffer, const char* str) ; // 子串str在buffer中的出现次数
size_t EveForge_SendByte(evbuffer* buffer, const int size, const int fd);  // 发送buffer的size个字节到fd
size_t EveForge_SendStringFromBuffer(evbuffer* buffer, const char* target_string, const int size,const int fd);  // 从buffer的target_string开始发送size个字节到fd


struct file_resource *EveForge_NewResourceFromFile(const char *filename); // 打开一个文件，放到file_resource类型之中，然后返回指向file_resource的指针
void EveForge_FreeResource(struct file_resource* resource);  // 释放一个file_resource
static void cleanup(const void* data, size_t len, void* arg); // 对FreeResource的调用
void EveForge_SpoolResourceToEvbuffer(struct evbuffer* buf, struct file_resource* fr);  //让buf通过add_reference引用指定文件的数据。然后发送到fd.
            // 这段代码的使用比较复杂，请根据测试函数的提示使用
void EveForge_Send_File_to_fd(const char* filename, int fd);

void EveForge_AdvanceEvbufferAdd(evbuffer* buffer,int size);  // // 高级的evbuffer_add。：申请2k的空间，然后添加测试用的数据，最后提交。// 结果：buffer的size个字节都是0xFF
void EveForge_CountMegabytesCb(struct evbuffer* buf, const struct evbuffer_cb_info* info, void* arg); // 当数据从buf之中移出了1M就向终端输出一个'.'


#endif