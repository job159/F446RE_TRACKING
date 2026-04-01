#include "App/serial_cmd.h"

#include <string.h>

static uint8_t SerialCmd_IsWhitespace(char ch)
{
  return (uint8_t)((ch == ' ') || (ch == '\t'));
}

static char SerialCmd_ToUpper(char ch)
{
  if ((ch >= 'a') && (ch <= 'z'))
  {
    return (char)(ch - ('a' - 'A'));
  }

  return ch;
}

static uint8_t SerialCmd_CopyTrimmedUpper(
    char *dst,
    uint8_t dst_size,
    const uint8_t *src,
    uint8_t src_len)
{
  uint8_t start = 0U;
  uint8_t end = src_len;
  uint8_t index = 0U;

  if ((dst == NULL) || (src == NULL) || (dst_size == 0U))
  {
    return 0U;
  }

  while ((start < src_len) && (SerialCmd_IsWhitespace((char)src[start]) != 0U))
  {
    start++;
  }

  while ((end > start) && (SerialCmd_IsWhitespace((char)src[end - 1U]) != 0U))
  {
    end--;
  }

  while ((start < end) && (index < (uint8_t)(dst_size - 1U)))
  {
    dst[index] = SerialCmd_ToUpper((char)src[start]);
    index++;
    start++;
  }

  dst[index] = '\0';
  return index;
}

static int32_t SerialCmd_ParseDecimal(const char *text, uint8_t *ok)
{
  int32_t value = 0;

  if (ok != NULL)
  {
    *ok = 0U;
  }

  if ((text == NULL) || (*text == '\0'))
  {
    return 0;
  }

  while (*text != '\0')
  {
    if ((*text < '0') || (*text > '9'))
    {
      return 0;
    }

    value = (value * 10) + (int32_t)(*text - '0');
    text++;
  }

  if (ok != NULL)
  {
    *ok = 1U;
  }

  return value;
}

static int32_t SerialCmd_ParsePeriodMs(const char *text, uint8_t *ok)
{
  char number_text[8];
  size_t length;
  size_t index;

  if (ok != NULL)
  {
    *ok = 0U;
  }

  if ((text == NULL) || (*text == '\0'))
  {
    return 0;
  }

  length = strlen(text);
  if ((length >= 2U) && (text[length - 2U] == 'M') && (text[length - 1U] == 'S'))
  {
    length -= 2U;
  }

  if ((length == 0U) || (length >= sizeof(number_text)))
  {
    return 0;
  }

  for (index = 0U; index < length; index++)
  {
    number_text[index] = text[index];
  }

  number_text[length] = '\0';
  return SerialCmd_ParseDecimal(number_text, ok);
}

static int32_t SerialCmd_ParseManualStageToken(const char *text, uint8_t *ok)
{
  if (ok != NULL)
  {
    *ok = 0U;
  }

  if ((text == NULL) || (*text == '\0'))
  {
    return 0;
  }

  if ((text[0] >= '1') && (text[0] <= '8') && (text[1] == '\0'))
  {
    if (ok != NULL)
    {
      *ok = 1U;
    }

    return (int32_t)(text[0] - '1');
  }

  if ((text[0] == 'F') && (text[1] >= '1') && (text[1] <= '4') && (text[2] == '\0'))
  {
    if (ok != NULL)
    {
      *ok = 1U;
    }

    return (int32_t)(text[1] - '1');
  }

  if ((text[0] == 'R') && (text[1] >= '1') && (text[1] <= '4') && (text[2] == '\0'))
  {
    if (ok != NULL)
    {
      *ok = 1U;
    }

    return (int32_t)(4 + (text[1] - '1'));
  }

  return 0;
}

static void SerialCmd_Enqueue(
    SerialCmd_HandleTypeDef *handle,
    const SerialCmd_t *command)
{
  if ((handle == NULL) || (command == NULL))
  {
    return;
  }

  if (handle->queue_count >= SERIAL_CMD_QUEUE_LENGTH)
  {
    return;
  }

  handle->queue[handle->queue_tail] = *command;
  handle->queue_tail = (uint8_t)((handle->queue_tail + 1U) % SERIAL_CMD_QUEUE_LENGTH);
  handle->queue_count++;
}

static void SerialCmd_ParseAndQueue(
    SerialCmd_HandleTypeDef *handle,
    const uint8_t *line,
    uint8_t line_length)
{
  char command_text[SERIAL_CMD_RX_LINE_MAX];
  SerialCmd_t command = {SERIAL_CMD_NONE, 0, 0};
  uint8_t ok = 0U;

  if (SerialCmd_CopyTrimmedUpper(command_text,
                                 (uint8_t)sizeof(command_text),
                                 line,
                                 line_length) == 0U)
  {
    return;
  }

  if ((strcmp(command_text, "IDLE") == 0) || (strcmp(command_text, "0") == 0) || (strcmp(command_text, "MODE 0") == 0))
  {
    command.id = SERIAL_CMD_MODE_IDLE;
  }
  else if ((strcmp(command_text, "TRACK") == 0) || (strcmp(command_text, "1") == 0) || (strcmp(command_text, "MODE 1") == 0))
  {
    command.id = SERIAL_CMD_MODE_TRACKING;
  }
  else if ((strcmp(command_text, "MANUAL") == 0) || (strcmp(command_text, "2") == 0) || (strcmp(command_text, "MODE 2") == 0))
  {
    command.id = SERIAL_CMD_MODE_MANUAL;
  }
  else if ((strcmp(command_text, "RECAL") == 0) || (strcmp(command_text, "CAL") == 0))
  {
    command.id = SERIAL_CMD_RECALIBRATE;
  }
  else if ((strcmp(command_text, "STATUS") == 0) || (strcmp(command_text, "STAT?") == 0))
  {
    command.id = SERIAL_CMD_STATUS_QUERY;
  }
  else if ((strcmp(command_text, "CALDATA") == 0) || (strcmp(command_text, "CAL?") == 0))
  {
    command.id = SERIAL_CMD_CAL_QUERY;
  }
  else if ((strcmp(command_text, "CONFIG") == 0) || (strcmp(command_text, "CFG?") == 0))
  {
    command.id = SERIAL_CMD_CONFIG_QUERY;
  }
  else if ((strncmp(command_text, "CTL ", 4U) == 0) || (strncmp(command_text, "PERIOD ", 7U) == 0))
  {
    const char *period_text = (command_text[0] == 'C') ? &command_text[4] : &command_text[7];

    command.arg0 = SerialCmd_ParsePeriodMs(period_text, &ok);
    if (ok != 0U)
    {
      command.id = SERIAL_CMD_CONTROL_PERIOD;
    }
  }
  else if (strcmp(command_text, "HELP") == 0)
  {
    command.id = SERIAL_CMD_HELP;
  }
  else if (strncmp(command_text, "MAN ", 4U) == 0)
  {
    command.arg0 = SerialCmd_ParseManualStageToken(&command_text[4], &ok);
    if (ok != 0U)
    {
      command.id = SERIAL_CMD_MANUAL_STAGE;
    }
  }
  else if ((command_text[0] == 'F') && (command_text[1] >= '1') && (command_text[1] <= '4') && (command_text[2] == '\0'))
  {
    command.id = SERIAL_CMD_MANUAL_STAGE;
    command.arg0 = (int32_t)(command_text[1] - '1');
  }
  else if ((command_text[0] == 'R') && (command_text[1] >= '1') && (command_text[1] <= '4') && (command_text[2] == '\0'))
  {
    command.id = SERIAL_CMD_MANUAL_STAGE;
    command.arg0 = (int32_t)(4 + (command_text[1] - '1'));
  }
  else if (strncmp(command_text, "STAGE ", 6U) == 0)
  {
    command.arg0 = SerialCmd_ParseDecimal(&command_text[6], &ok);
    if ((ok != 0U) && (command.arg0 >= 0) && (command.arg0 <= 7))
    {
      command.id = SERIAL_CMD_MANUAL_STAGE;
    }
  }

  if (command.id != SERIAL_CMD_NONE)
  {
    SerialCmd_Enqueue(handle, &command);
  }
}

void SerialCmd_Init(
    SerialCmd_HandleTypeDef *handle,
    UART_HandleTypeDef *huart)
{
  if (handle == NULL)
  {
    return;
  }

  (void)memset(handle, 0, sizeof(*handle));
  handle->huart = huart;
}

void SerialCmd_PollRx(SerialCmd_HandleTypeDef *handle)
{
  if ((handle == NULL) || (handle->huart == NULL))
  {
    return;
  }

  if (__HAL_UART_GET_FLAG(handle->huart, UART_FLAG_ORE) != RESET)
  {
    __HAL_UART_CLEAR_OREFLAG(handle->huart);
  }

  if (__HAL_UART_GET_FLAG(handle->huart, UART_FLAG_NE) != RESET)
  {
    __HAL_UART_CLEAR_NEFLAG(handle->huart);
  }

  if (__HAL_UART_GET_FLAG(handle->huart, UART_FLAG_FE) != RESET)
  {
    __HAL_UART_CLEAR_FEFLAG(handle->huart);
  }

  while (__HAL_UART_GET_FLAG(handle->huart, UART_FLAG_RXNE) != RESET)
  {
    uint8_t rx_byte = (uint8_t)(handle->huart->Instance->DR & 0xFFU);

    if ((rx_byte == '\r') || (rx_byte == '\n'))
    {
      if (handle->rx_length > 0U)
      {
        SerialCmd_ParseAndQueue(handle, handle->rx_line, handle->rx_length);
        handle->rx_length = 0U;
      }
    }
    else if ((rx_byte == '\b') || (rx_byte == 0x7FU))
    {
      if (handle->rx_length > 0U)
      {
        handle->rx_length--;
      }
    }
    else if (handle->rx_length < SERIAL_CMD_RX_LINE_MAX)
    {
      handle->rx_line[handle->rx_length] = rx_byte;
      handle->rx_length++;
    }
    else
    {
      handle->rx_length = 0U;
    }
  }
}

uint8_t SerialCmd_Dequeue(
    SerialCmd_HandleTypeDef *handle,
    SerialCmd_t *command)
{
  if ((handle == NULL) || (command == NULL) || (handle->queue_count == 0U))
  {
    return 0U;
  }

  *command = handle->queue[handle->queue_head];
  handle->queue_head = (uint8_t)((handle->queue_head + 1U) % SERIAL_CMD_QUEUE_LENGTH);
  handle->queue_count--;
  return 1U;
}
