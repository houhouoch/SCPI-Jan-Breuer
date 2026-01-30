#include "scpi/scpi.h"
#include "main.h" // 包含 HAL 库头文件
#include <stdio.h>
#include "scpi-def.h"
extern UART_HandleTypeDef huart1; // 声明你的串口句柄

/**
 * @brief 实现写函数，解析器需要输出数据时会调用它
 */
size_t SCPI_Write(scpi_t * context, const char * data, size_t len) {
    (void) context;
    /* 改为你的串口发送函数 */
    HAL_UART_Transmit(&huart1, (uint8_t *)data, len, 100);
    return len;
}

/**
 * @brief 实现错误回调，当发生语法错误等情况时调用
 */
int SCPI_Error(scpi_t * context, int_fast16_t err) {
    // 你可以在这里打印错误日志，方便调试
    fprintf(stderr, "**SCPI ERROR: %d\r\n", (int)err);
    return 0;
}


