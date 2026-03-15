#include "App/uart_sequence.h"

#define UART_SEQUENCE_TX_BUFFER_SIZE 160U
#define UART_SEQUENCE_TX_TIMEOUT_MS 30U
#define UART_SEQUENCE_ANGLE_FRACTION_SCALE 10000U

static uint16_t UartSequence_AppendText(
    uint8_t *buffer,
    uint16_t capacity,
    uint16_t index,
    const char *text)
{
  while ((*text != '\0') && (index < capacity))
  {
    buffer[index] = (uint8_t)*text;
    index++;
    text++;
  }

  return index;
}

static uint16_t UartSequence_AppendU32(
    uint8_t *buffer,
    uint16_t capacity,
    uint16_t index,
    uint32_t value)
{
  uint8_t digits[10];
  uint8_t digit_count = 0U;

  do
  {
    digits[digit_count] = (uint8_t)('0' + (value % 10U));
    value /= 10U;
    digit_count++;
  } while ((value > 0U) && (digit_count < (uint8_t)sizeof(digits)));

  while ((digit_count > 0U) && (index < capacity))
  {
    digit_count--;
    buffer[index] = digits[digit_count];
    index++;
  }

  return index;
}

static uint16_t UartSequence_AppendS32(
    uint8_t *buffer,
    uint16_t capacity,
    uint16_t index,
    int32_t value)
{
  uint32_t magnitude;

  if (value < 0)
  {
    index = UartSequence_AppendText(buffer, capacity, index, "-");
    magnitude = (uint32_t)(-(int64_t)value);
    index = UartSequence_AppendU32(buffer, capacity, index, magnitude);
  }
  else
  {
    index = UartSequence_AppendU32(buffer, capacity, index, (uint32_t)value);
  }

  return index;
}

static uint16_t UartSequence_AppendFixedWidthU32(
    uint8_t *buffer,
    uint16_t capacity,
    uint16_t index,
    uint32_t value,
    uint8_t width)
{
  uint8_t digits[10];
  uint8_t i;

  if (width > (uint8_t)sizeof(digits))
  {
    width = (uint8_t)sizeof(digits);
  }

  for (i = 0U; i < width; i++)
  {
    digits[width - 1U - i] = (uint8_t)('0' + (value % 10U));
    value /= 10U;
  }

  for (i = 0U; i < width; i++)
  {
    if (index >= capacity)
    {
      break;
    }

    buffer[index] = digits[i];
    index++;
  }

  return index;
}

static uint16_t UartSequence_AppendAngleX10000(
    uint8_t *buffer,
    uint16_t capacity,
    uint16_t index,
    uint32_t angle_x10000)
{
  uint32_t integer_part;
  uint32_t fraction_part;

  integer_part = angle_x10000 / UART_SEQUENCE_ANGLE_FRACTION_SCALE;
  fraction_part = angle_x10000 % UART_SEQUENCE_ANGLE_FRACTION_SCALE;

  index = UartSequence_AppendU32(buffer, capacity, index, integer_part);
  index = UartSequence_AppendText(buffer, capacity, index, ".");
  index = UartSequence_AppendFixedWidthU32(buffer, capacity, index, fraction_part, 4U);

  return index;
}

void UartSequence_Init(
    UartSequence_HandleTypeDef *handle,
    UART_HandleTypeDef *huart,
    uint32_t period_ms)
{
  if ((handle == NULL) || (huart == NULL))
  {
    return;
  }

  handle->huart = huart;
  handle->period_ms = period_ms;
  handle->last_tick_ms = HAL_GetTick();
  handle->serial_number = 0U;
}

void UartSequence_Task(
    UartSequence_HandleTypeDef *handle,
    uint16_t adc1_value,
    uint16_t adc2_value,
    uint16_t adc3_value,
    uint16_t adc4_value,
    int32_t enc1_count,
    int32_t enc2_count,
    uint32_t enc1_angle_x10000,
    uint32_t enc2_angle_x10000,
    uint8_t motor_1_mode,
    uint8_t motor_2_mode)
{
  uint8_t tx_buffer[UART_SEQUENCE_TX_BUFFER_SIZE];
  uint16_t tx_length = 0U;
  uint32_t now_tick_ms;

  if ((handle == NULL) || (handle->huart == NULL))
  {
    return;
  }

  now_tick_ms = HAL_GetTick();
  if ((now_tick_ms - handle->last_tick_ms) < handle->period_ms)
  {
    return;
  }

  handle->last_tick_ms = now_tick_ms;

  tx_length = UartSequence_AppendU32(
      tx_buffer,
      UART_SEQUENCE_TX_BUFFER_SIZE,
      tx_length,
      handle->serial_number);
  tx_length = UartSequence_AppendText(
      tx_buffer,
      UART_SEQUENCE_TX_BUFFER_SIZE,
      tx_length,
      " m1:");
  tx_length = UartSequence_AppendU32(
      tx_buffer,
      UART_SEQUENCE_TX_BUFFER_SIZE,
      tx_length,
      motor_1_mode);
  tx_length = UartSequence_AppendText(
      tx_buffer,
      UART_SEQUENCE_TX_BUFFER_SIZE,
      tx_length,
      " m2:");
  tx_length = UartSequence_AppendU32(
      tx_buffer,
      UART_SEQUENCE_TX_BUFFER_SIZE,
      tx_length,
      motor_2_mode);
  tx_length = UartSequence_AppendText(
      tx_buffer,
      UART_SEQUENCE_TX_BUFFER_SIZE,
      tx_length,
      " adc1:");
  tx_length = UartSequence_AppendU32(
      tx_buffer,
      UART_SEQUENCE_TX_BUFFER_SIZE,
      tx_length,
      adc1_value);
  tx_length = UartSequence_AppendText(
      tx_buffer,
      UART_SEQUENCE_TX_BUFFER_SIZE,
      tx_length,
      " adc2:");
  tx_length = UartSequence_AppendU32(
      tx_buffer,
      UART_SEQUENCE_TX_BUFFER_SIZE,
      tx_length,
      adc2_value);
  tx_length = UartSequence_AppendText(
      tx_buffer,
      UART_SEQUENCE_TX_BUFFER_SIZE,
      tx_length,
      " adc3:");
  tx_length = UartSequence_AppendU32(
      tx_buffer,
      UART_SEQUENCE_TX_BUFFER_SIZE,
      tx_length,
      adc3_value);
  tx_length = UartSequence_AppendText(
      tx_buffer,
      UART_SEQUENCE_TX_BUFFER_SIZE,
      tx_length,
      " adc4:");
  tx_length = UartSequence_AppendU32(
      tx_buffer,
      UART_SEQUENCE_TX_BUFFER_SIZE,
      tx_length,
      adc4_value);
  tx_length = UartSequence_AppendText(
      tx_buffer,
      UART_SEQUENCE_TX_BUFFER_SIZE,
      tx_length,
      " enc1:");
  tx_length = UartSequence_AppendS32(
      tx_buffer,
      UART_SEQUENCE_TX_BUFFER_SIZE,
      tx_length,
      enc1_count);
  tx_length = UartSequence_AppendText(
      tx_buffer,
      UART_SEQUENCE_TX_BUFFER_SIZE,
      tx_length,
      " enc2:");
  tx_length = UartSequence_AppendS32(
      tx_buffer,
      UART_SEQUENCE_TX_BUFFER_SIZE,
      tx_length,
      enc2_count);
  tx_length = UartSequence_AppendText(
      tx_buffer,
      UART_SEQUENCE_TX_BUFFER_SIZE,
      tx_length,
      " ang1:");
  tx_length = UartSequence_AppendAngleX10000(
      tx_buffer,
      UART_SEQUENCE_TX_BUFFER_SIZE,
      tx_length,
      enc1_angle_x10000);
  tx_length = UartSequence_AppendText(
      tx_buffer,
      UART_SEQUENCE_TX_BUFFER_SIZE,
      tx_length,
      " ang2:");
  tx_length = UartSequence_AppendAngleX10000(
      tx_buffer,
      UART_SEQUENCE_TX_BUFFER_SIZE,
      tx_length,
      enc2_angle_x10000);
  tx_length = UartSequence_AppendText(
      tx_buffer,
      UART_SEQUENCE_TX_BUFFER_SIZE,
      tx_length,
      "\r\n");

  if (HAL_UART_Transmit(handle->huart, tx_buffer, tx_length, UART_SEQUENCE_TX_TIMEOUT_MS) == HAL_OK)
  {
    handle->serial_number++;
  }
}
