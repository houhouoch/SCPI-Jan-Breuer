/**********************************   Readme    **********************************/
SCPI库@Jan Breuer@https://github.com/j123b567/scpi-parser.git
https://www.yono233.cn/posts/novel/24_7_12_SCPI 这个是SCPI的一个小小的介绍和入门
/**********************************   移植    **********************************/
移植的过程也相对简单。
1、把libscpi这个文件夹整个拖入到工程，然后在MDK中把src的.C文件进行加入  再把相应的头文件进行加载就完事了！
2、这时候会报错，是因为下面这个结构体中没定义相关的函数，这边建议写一个.C文件 把SCPI_Write写上即可
scpi_interface_t scpi_interface = {
    .error = SCPI_Error,
    .write = SCPI_Write,
    .control = NULL,//SCPI_Control,
    .flush = NULL,//SCPI_Flush,
    .reset = NULL,//SCPI_Reset,
};
3  、完成上述步骤后还会报错
 //{.pattern = "SYSTem:COMMunication:TCPIP:CONTROL?", .callback = SCPI_SystemCommTcpipControlQ,},
把这个取消掉即可
/**********************************   解决    **********************************/
4、作者这边不想使用MicroLIB，之前看帖子说这个库会引起一些莫名其妙的bug
然后我在usart,c那边照着正点原子的写法 加上了一些函数，导致这里会报错
这里报错是指#pragma import(__use_no_semihosting)
然后我把这个取消掉，虽然程序没有错误了，上电运行不起来。这时候我进入调试模式，发现需要复位3次才能跑起来。
如果你嫌麻烦 可以把MicroLIB勾上。

这边作者其实有做处理的方法，就是添加以下几个函数来“欺骗”链接器，告诉它我们不需要文件系统支持

#if 1
#if (__ARMCC_VERSION >= 6010050)          
__asm(".global __use_no_semihosting\n\t"); 
__asm(".global __ARM_use_no_argv \n\t");   
#else
#pragma import(__use_no_semihosting)
    void _sys_exit(int x) { x = x; }
    void _ttywrch(int ch) { ch = ch; }
struct __FILE
{
    int handle;
};
FILE __stdout;
FILE __stdin;
FILE __stderr;

#endif
int fputc(int ch, FILE *f)
{
    while ((USART1->ISR & 0X40) == 0);   
    USART1->TDR = (uint8_t)ch;         
    return ch;
}
#endif
/**********************************   END    **********************************/

