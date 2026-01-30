# SCPI Parser 嵌入式移植与实战指南

本项目基于 [Jan Breuer / scpi-parser](https://github.com/j123b567/scpi-parser.git) 库进行移植，旨在为嵌入式设备（如 STM32）提供标准化的可编程仪器标准命令（SCPI）解析功能。

> **背景资料：** [SCPI 协议基础与快速入门](https://www.yono233.cn/posts/novel/24_7_12_SCPI)

---

## 📂 1. 环境准备与文件移植

1. **导入库文件**：
   将 `libscpi` 文件夹整体复制到你的工程目录下。
2. **IDE 配置 (以 MDK 为例)**：
   * 在工程分组中添加 `libscpi/src` 下的所有 `.c` 文件。
   * 在 `Include Paths` 中添加头文件所在路径。

---

## ⚙️ 2. 接口实现 (移植核心)

### 2.1 结构体实例化
在工程中新建一个底层接口文件（如 `scpi_port.c`），定义并实现 `scpi_interface_t`：

```c
scpi_interface_t scpi_interface = {
    .error = SCPI_Error,    // 错误处理回调
    .write = SCPI_Write,    // 底层发送函数（需自行实现串口打印）
    .control = NULL,
    .flush = NULL,
    .reset = NULL,
};
```

### 2.2 常见错误修复
若编译提示 `SCPI_SystemCommTcpipControlQ` 未定义，是因为库默认开启了 TCP 控制，而在嵌入式单片机中通常不使用。
* **解决方法**：在指令表中找到并**注释掉**以下内容：
```c
// {.pattern = "SYSTem:COMMunication:TCPIP:CONTROL?", .callback = SCPI_SystemCommTcpipControlQ,},
```

---

## 🛠️ 3. 核心排障：MicroLIB 与半主机模式

> [!CAUTION] 
> **避坑警示**：若不使用 MicroLIB 且未处理半主机模式，程序可能会死在启动阶段，或者出现“需要手动复位 3 次才能运行”的玄学现象。

如果你不想勾选 MicroLIB（为了规避潜在的库 Bug），必须在 `usart.c` 或相关底层文件中添加以下代码，通过添加以下几个函数来“欺骗”链接器，告诉它我们不需要文件系统支持：

```c
#if 1
#if (__ARMCC_VERSION >= 6010050)          
    /* AC6 环境（ArmClang） */
    __asm(".global __use_no_semihosting\n\t"); 
    __asm(".global __ARM_use_no_argv \n\t");   
#else
    /* AC5 环境 */
    #pragma import(__use_no_semihosting)
    void _sys_exit(int x) { x = x; }
    void _ttywrch(int ch) { ch = ch; }
    struct __FILE { int handle; };
    FILE __stdout;
    FILE __stdin;
    FILE __stderr;
#endif

/* 兼容 printf 的底层发送实现 */
int fputc(int ch, FILE *f) {
    while ((USART1->ISR & 0X40) == 0); // 等待发送完成
    USART1->TDR = (uint8_t)ch;
    return ch;
}
#endif
```

---

## 🚀 4. 使用示例

### 4.1 初始化 SCPI
在系统启动时配置解析器句柄，定义设备身份信息：

```c
void SCPI_Config_Init(void) {
    SCPI_Init(&scpi_context,
              scpi_commands,            // 你的指令映射表
              &scpi_interface,          // 上面定义的接口
              &scpi_units_def,          // 单位定义（可设为 NULL 或默认）
              "UNI-TREND", "UDP6900", "SN123456", "V1.0.0", // *IDN? 信息
              scpi_input_buffer, SCPI_INPUT_BUFFER_LENGTH,
              scpi_error_queue_data, SCPI_ERROR_QUEUE_SIZE);
}
```

### 4.2 接收回调与数据输入 (DMA+Idle 模式)
在串口异步接收中实时喂入数据。**注意：如果你启用了 D-Cache，必须执行作废操作以保证数据一致性。**

```c
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    if (huart->Instance == USART1) {
        /* 1. 【核心修正】若开启 D-Cache，必须先作废 Cache，确保读到 RAM 里的最新 DMA 数据 */
        SCB_InvalidateDCache_by_Addr((uint32_t*)g_usart_rx_buf, USART_REC_LEN);

        /* 2. 安全处理：手动补齐字符串结束符 */
        if(Size < USART_REC_LEN) {
            g_usart_rx_buf[Size] = '\0'; 
        } else {
            g_usart_rx_buf[USART_REC_LEN - 1] = '\0';
        }
        
        /* 3. 调用库函数进行解析 */
        SCPI_Input(&scpi_context, (char*)g_usart_rx_buf, Size);
        
        /* 4. 重新开启 DMA 空闲接收 */
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, g_usart_rx_buf, USART_REC_LEN);
    }
}
```

---

## ✅ 5. 验证
1. 打开串口调试助手。
2. 发送标准查询指令：`*IDN?`
3. 预期返回：`UNI-TREND,UDP6900,SN123456,V1.0.0`。

---
/********************************** END **********************************/
