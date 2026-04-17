#include "App/serial_cmd.h"
#include <string.h>

/* 去前後空白並轉大寫,複製到 dst,回傳長度 */
static uint8_t trim_upper(char *dst, uint8_t dst_size, const uint8_t *src, uint8_t len)
{
  uint8_t s = 0, e = len;
  while (s < len && (src[s] == ' ' || src[s] == '\t')) s++;
  while (e > s && (src[e-1] == ' ' || src[e-1] == '\t')) e--;

  uint8_t n = 0;
  while (s < e && n < dst_size - 1)
  {
    char c = (char)src[s++];
    if (c >= 'a' && c <= 'z') c -= 32;
    dst[n++] = c;
  }
  dst[n] = '\0';
  return n;
}

/* 解析十進位整數 */
static int32_t parse_int(const char *s, uint8_t *ok)
{
  *ok = 0;
  if (s == NULL || *s == '\0') return 0;

  int32_t v = 0;
  while (*s)
  {
    if (*s < '0' || *s > '9') return 0;
    v = v * 10 + (*s - '0');
    s++;
  }
  *ok = 1;
  return v;
}

/* 解析 stage:
 *   "1".."14"        -> 0..13
 *   "F1".."F7"       -> 0..6
 *   "R1".."R7"       -> 7..13 */
static int32_t parse_stage(const char *s, uint8_t *ok)
{
  *ok = 0;
  if (s == NULL || *s == '\0') return 0;

  if (s[0] == 'F' || s[0] == 'R')
  {
    uint8_t tmp_ok;
    int32_t n = parse_int(&s[1], &tmp_ok);
    if (!tmp_ok || n < 1 || n > 7) return 0;
    *ok = 1;
    return (s[0] == 'F') ? (n - 1) : (TMC_DIR_SPLIT_STAGE + n - 1);
  }

  uint8_t tmp_ok;
  int32_t n = parse_int(s, &tmp_ok);
  if (!tmp_ok || n < 1 || n > TMC_SPEED_STAGE_COUNT) return 0;
  *ok = 1;
  return n - 1;
}

static void enqueue(SerialCmd_HandleTypeDef *h, SerialCmdId_t id, int32_t arg)
{
  if (h->q_count >= SERIAL_CMD_QUEUE_LENGTH) return;
  h->queue[h->q_tail].id   = id;
  h->queue[h->q_tail].arg0 = arg;
  h->queue[h->q_tail].arg1 = 0;
  h->q_tail = (h->q_tail + 1) % SERIAL_CMD_QUEUE_LENGTH;
  h->q_count++;
}

/* 解析一行指令 */
static void parse_line(SerialCmd_HandleTypeDef *h, const uint8_t *line, uint8_t len)
{
  /* 需要包含 stepper header 才能用 TMC_SPEED_STAGE_COUNT */
  char txt[SERIAL_CMD_RX_LINE_MAX];
  if (trim_upper(txt, sizeof(txt), line, len) == 0) return;

  uint8_t ok = 0;

  /* ----- 模式 ----- */
  if (!strcmp(txt, "IDLE")   || !strcmp(txt, "0")) { enqueue(h, SERIAL_CMD_MODE_IDLE,     0); return; }
  if (!strcmp(txt, "TRACK")  || !strcmp(txt, "1")) { enqueue(h, SERIAL_CMD_MODE_TRACKING, 0); return; }
  if (!strcmp(txt, "MANUAL") || !strcmp(txt, "2")) { enqueue(h, SERIAL_CMD_MODE_MANUAL,   0); return; }

  /* ----- 校正/查詢 ----- */
  if (!strcmp(txt, "RECAL"))  { enqueue(h, SERIAL_CMD_RECALIBRATE,   0); return; }
  if (!strcmp(txt, "STATUS")) { enqueue(h, SERIAL_CMD_STATUS_QUERY,  0); return; }
  if (!strcmp(txt, "CAL?"))   { enqueue(h, SERIAL_CMD_CAL_QUERY,     0); return; }
  if (!strcmp(txt, "CFG?"))   { enqueue(h, SERIAL_CMD_CONFIG_QUERY,  0); return; }
  if (!strcmp(txt, "HELP"))   { enqueue(h, SERIAL_CMD_HELP,          0); return; }

  /* ----- 控制週期: "PERIOD 5" -> 5ms ----- */
  if (!strncmp(txt, "PERIOD ", 7))
  {
    int32_t ms = parse_int(&txt[7], &ok);
    if (ok) enqueue(h, SERIAL_CMD_CONTROL_PERIOD, ms);
    return;
  }

  /* ----- 手動 stage: "MAN F3" / "MAN R5" / "MAN 10" ----- */
  if (!strncmp(txt, "MAN ", 4))
  {
    int32_t s = parse_stage(&txt[4], &ok);
    if (ok) enqueue(h, SERIAL_CMD_MANUAL_STAGE, s);
    return;
  }

  /* ----- 簡寫: F1..F7 / R1..R7 ----- */
  if (txt[0] == 'F' || txt[0] == 'R')
  {
    int32_t s = parse_stage(txt, &ok);
    if (ok) enqueue(h, SERIAL_CMD_MANUAL_STAGE, s);
  }
}

/* ---------- public ---------- */

void SerialCmd_Init(SerialCmd_HandleTypeDef *h, UART_HandleTypeDef *huart)
{
  memset(h, 0, sizeof(*h));
  h->huart = huart;
}

void SerialCmd_PollRx(SerialCmd_HandleTypeDef *h)
{
  if (h->huart == NULL) return;

  /* 清除 UART 錯誤旗標 */
  if (__HAL_UART_GET_FLAG(h->huart, UART_FLAG_ORE)) __HAL_UART_CLEAR_OREFLAG(h->huart);
  if (__HAL_UART_GET_FLAG(h->huart, UART_FLAG_NE))  __HAL_UART_CLEAR_NEFLAG(h->huart);
  if (__HAL_UART_GET_FLAG(h->huart, UART_FLAG_FE))  __HAL_UART_CLEAR_FEFLAG(h->huart);

  while (__HAL_UART_GET_FLAG(h->huart, UART_FLAG_RXNE))
  {
    uint8_t ch = (uint8_t)(h->huart->Instance->DR & 0xFF);

    if (ch == '\r' || ch == '\n')
    {
      if (h->rx_len > 0)
      {
        parse_line(h, h->rx_buf, h->rx_len);
        h->rx_len = 0;
      }
    }
    else if (ch == '\b' || ch == 0x7F)
    {
      if (h->rx_len > 0) h->rx_len--;
    }
    else if (h->rx_len < SERIAL_CMD_RX_LINE_MAX)
    {
      h->rx_buf[h->rx_len++] = ch;
    }
    else
    {
      h->rx_len = 0; /* overflow 丟棄 */
    }
  }
}

uint8_t SerialCmd_Dequeue(SerialCmd_HandleTypeDef *h, SerialCmd_t *out)
{
  if (h->q_count == 0) return 0;
  *out = h->queue[h->q_head];
  h->q_head = (h->q_head + 1) % SERIAL_CMD_QUEUE_LENGTH;
  h->q_count--;
  return 1;
}
