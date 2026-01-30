# SCPI Parser 移植与应用开发指南

本项目基于 [Jan Breuer / scpi-parser](https://github.com/j123b567/scpi-parser.git) 库进行移植。为了方便后续维护和多接口扩展（如增加 TCP、USB），我们将工程拆分为 **库文件 (Scpi_Lib)** 与 **应用层 (Scpi_APP)**。

> **参考入门资料：** [SCPI 的一个小小介绍和入门](https://www.yono233.cn/posts/novel/24_7_12_SCPI)

---

## 📂 1. 工程架构说明

为了结构清晰，建议按以下方式组织代码：
* **Scpi_Lib/**：直接存放官方库文件，保持原汁原味，方便以后升级库版本。
* **Scpi_APP/**：存放我们的业务逻辑，包括：
    * `scpi-port.c`：底层读写接口。
    * `scpi-def.c`：指令映射表（后续所有新命令都在这加）。

---

## 🛠️ 2. 移植步骤 (保姆级记录)

移植过程相对简单，主要处理几个编译器报错：

1.  **导入文件**：把 `libscpi` 文件夹整个拖入工程，在 MDK 中加入 `src` 下的所有 `.C` 文件，并加载头文件路径。
2.  **定义接口结构体**：
    由于库中没定义具体的读写函数，会报错。建议在 `scpi-port.c` 中写好 `SCPI_Write` 等函数，并完成结构体定义：
    ```c
    scpi_interface_t scpi_interface = {
        .error = SCPI_Error,
        .write = SCPI_Write,
        .control = NULL, // SCPI_Control,
        .flush = NULL,   // SCPI_Flush,
        .reset = NULL,   // SCPI_Reset,
    };
    ```
3.  **消除 TCPIP 报错**：
    完成上述步骤后还会报错，是因为单片机通常不跑默认的 TCPIP 控制。找到下面这行代码，将其**取消（注释）掉**即可：
    ```c
    // {.pattern = "SYSTem:COMMunication:TCPIP:CONTROL?", .callback = SCPI_SystemCommTcpipControlQ,},
    ```

---

## ⚠️ 3. 核心解决：关于 MicroLIB 与“复位 3 次”的问题

作者（我）这边**不想使用 MicroLIB**，因为有反馈说这个库会引起一些莫名其妙的 bug。

**遇到的坑**：
如果在 `usart.c` 里照着正点原子的写法加了半主机处理函数，会导致 `#pragma import(__use_no_semihosting)` 报错。如果简单地强行取消它，程序虽然没错误了，但上电跑不起来。**进入调试模式会发现，程序需要手动复位 3 次才能跑起来。**

**最终解决方法**：
如果你嫌麻烦，可以直接勾选 **MicroLIB**。但为了彻底避坑，建议添加以下函数来“欺骗”链接器，明确告诉它我们不需要文件系统支持：

```c
#if 1
#if (__ARMCC_VERSION >= 6010050)          
    /* 针对 AC6 (ArmClang) 的处理 */
    __asm(".global __use_no_semihosting\n\t"); 
    __asm(".global __ARM_use_no_argv \n\t");   
#else
    /* 针对 AC5 的处理 */
    #pragma import(__use_no_semihosting)
    void _sys_exit(int x) { x = x; }
    void _ttywrch(int ch) { ch = ch; }
    struct __FILE { int handle; };
    FILE __stdout; FILE __stdin; FILE __stderr;
#endif

/* 实现 fputc 供 printf 使用 */
int fputc(int ch, FILE *f) {
    while ((USART1->ISR & 0X40) == 0);    
    USART1->TDR = (uint8_t)ch;          
    return ch;
}
#endif
```

---

## 🚀 4. 使用与扩展

### 4.1 初始化 SCPI
设置一个初始化函数，参数含义如下：
```c
void SCPI_Config_Init(void) {
    /*
     * 参数含义：
     * &scpi_context: 解析器句柄
     * scpi_commands: 指令映射表（定义在 scpi-def.c）
     * &scpi_interface: 包含写函数和错误函数的接口
     * scpi_units_def: 单位定义（通常传 NULL 或默认值）
     * 后面四个字符串对应 *IDN? 的返回信息：厂商, 型号, 序列号, 版本
     */
    SCPI_Init(&scpi_context,
              scpi_commands,
              &scpi_interface,
              scpi_units_def,
              "UNI-TREND", "UDP6900", "SN123456", "V1.0.0",
              scpi_input_buffer, SCPI_INPUT_BUFFER_LENGTH,
              scpi_error_queue_data, SCPI_ERROR_QUEUE_SIZE);
}
```

### 4.2 数据输入 (以串口 DMA 为例)
最核心的是调用 `SCPI_Input`。在使用高性能 MCU（如 H7）时，必须注意 **Cache 一致性**：

```c
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    if (huart->Instance == USART1) {
        /* 1. 【核心修正】在读取数据前，必须作废该段内存的 Cache */
        /* 确保 CPU 读到的是从 RAM 拿到的最新 DMA 数据，而不是旧的缓存 */
        SCB_InvalidateDCache_by_Addr((uint32_t*)g_usart_rx_buf, USART_REC_LEN);

        /* 2. 安全处理：手动添加字符串结束符 */
        if(Size < USART_REC_LEN) {
            g_usart_rx_buf[Size] = '\0'; 
        } else {
            g_usart_rx_buf[USART_REC_LEN - 1] = '\0';
        }
        
        /* 3. 喂入解析器 */
        SCPI_Input(&scpi_context, (char*)g_usart_rx_buf, Size);
        
        /* 4. 重启 DMA 接收 (DMA_NORMAL 模式下需手动重启) */
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, g_usart_rx_buf, USART_REC_LEN);
    }
}
```

### 4.3 以后想加 TCP 怎么办？
本架构支持多链路接入。如果你想加 TCP：
1.  在 **Scpi_APP** 文件夹添加 TCP 的逻辑文件。
2.  在 TCP 收到数据的回调函数里，同样调用 `SCPI_Input(&scpi_context, tcp_data_ptr, size)`。
3.  在 `scpi-def.c` 里的指令映射表中添加你想要的 TCP 控制指令即可。

---

## ✅ 5. 验证
通过串口调试助手打印 `*IDN?` 看看是否能有对应的值打印出来。

/********************************** END **********************************/
