#include "App/serial_cmd.h"
#include <string.h>

/* 去空白、轉大寫、複製到dst，回傳長度 */
static uint8_t trim_upper(char *dst, uint8_t dst_size, const uint8_t *src, uint8_t len)
{
  uint8_t s = 0, e = len;

  /* trim前後空白 */
  while (s < len && (src[s] == ' ' || src[s] == '\t')) s++;
  while (e > s && (src[e-1] == ' ' || src[e-1] == '\t')) e--;

  uint8_t n = 0;
  while (s < e && n < dst_size - 1)
  {
    char c = (char)src[s++];
    if (c >= 'a' && c <= 'z') c = c - 32;
    dst[n++] = c;
  }
  dst[n] = '\0';
  return n;
}

/* 解析十進位數字字串 */
static int32_t parse_int(const char *s, uint8_t *ok)
{
  if (ok) *ok = 0;
  if (s == NULL || *s == '\0') return 0;

  int32_t val = 0;
  while (*s)
  {
    if (*s < '0' || *s > '9') return 0;
    val = val * 10 + (*s - '0');
    s++;
  }
  if (ok) *ok = 1;
  return val;
}

/* 解析 "5MS" 或 "5" 這類period字串 */
static int32_t parse_period(const char *s, uint8_t *ok)
{
  if (ok) *ok = 0;
  if (s == NULL || *s == '\0') return 0;

  size_t len = strlen(s);
  char buf[8];

  /* 去掉結尾的 "MS" */
  if (len >= 2 && s[len-2] == 'M' && s[len-1] == 'S')
    len -= 2;

  if (len == 0 || len >= sizeof(buf)) return 0;

  memcpy(buf, s, len);
  buf[len] = '\0';
  return parse_int(buf, ok);
}

static int32_t parse_microsteps(const char *s, uint8_t *ok)
{
  int32_t v = parse_int(s, ok);
  if (ok != NULL && *ok == 0) return 0;

  switch (v)
  {
  case 1:
  case 2:
  case 4:
  case 8:
  case 16:
  case 32:
  case 64:
  case 128:
  case 256:
    return v;
  default:
    if (ok) *ok = 0;
    return 0;
  }
}

static uint8_t parse_current_args(const char *s, uint8_t *irun, uint8_t *ihold, uint8_t *iholddelay)
{
  if (s == NULL || irun == NULL || ihold == NULL || iholddelay == NULL) return 0;

  char buf[SERIAL_CMD_RX_LINE_MAX];
  size_t len = strlen(s);
  if (len >= sizeof(buf)) return 0;
  memcpy(buf, s, len + 1U);

  char *tok_irun = strtok(buf, " ");
  char *tok_ihold = strtok(NULL, " ");
  char *tok_delay = strtok(NULL, " ");
  if (tok_irun == NULL || tok_ihold == NULL || tok_delay == NULL || strtok(NULL, " ") != NULL)
    return 0;

  uint8_t ok = 0;
  int32_t parsed_irun = parse_int(tok_irun, &ok);
  if (!ok || parsed_irun < 0 || parsed_irun > 31) return 0;

  int32_t parsed_ihold = parse_int(tok_ihold, &ok);
  if (!ok || parsed_ihold < 0 || parsed_ihold > 31) return 0;

  int32_t parsed_delay = parse_int(tok_delay, &ok);
  if (!ok || parsed_delay < 0 || parsed_delay > 15) return 0;

  *irun = (uint8_t)parsed_irun;
  *ihold = (uint8_t)parsed_ihold;
  *iholddelay = (uint8_t)parsed_delay;
  return 1;
}

/* 解析手動stage: "1"~"8", "F1"~"F4", "R1"~"R4" */
static int32_t parse_stage(const char *s, uint8_t *ok)
{
  if (ok) *ok = 0;
  if (s == NULL || *s == '\0') return 0;

  /* 直接數字 1~8 */
  if (s[0] >= '1' && s[0] <= '8' && s[1] == '\0')
  {
    if (ok) *ok = 1;
    return s[0] - '1';
  }
  /* F1~F4: forward */
  if (s[0] == 'F' && s[1] >= '1' && s[1] <= '4' && s[2] == '\0')
  {
    if (ok) *ok = 1;
    return s[1] - '1';
  }
  /* R1~R4: reverse */
  if (s[0] == 'R' && s[1] >= '1' && s[1] <= '4' && s[2] == '\0')
  {
    if (ok) *ok = 1;
    return 4 + (s[1] - '1');
  }
  return 0;
}

static void enqueue(SerialCmd_HandleTypeDef *h, SerialCmdId_t id, int32_t arg)
{
  if (h->q_count >= SERIAL_CMD_QUEUE_LENGTH) return;

  h->queue[h->q_tail].id = id;
  h->queue[h->q_tail].arg0 = arg;
  h->queue[h->q_tail].arg1 = 0;
  h->q_tail = (h->q_tail + 1) % SERIAL_CMD_QUEUE_LENGTH;
  h->q_count++;
}

/* 解析一行指令，放進queue */
static void parse_line(SerialCmd_HandleTypeDef *h, const uint8_t *line, uint8_t len)
{
  char txt[SERIAL_CMD_RX_LINE_MAX];
  if (trim_upper(txt, sizeof(txt), line, len) == 0) return;

  uint8_t ok = 0;

  /* 模式切換 */
  if (strcmp(txt, "IDLE") == 0 || strcmp(txt, "0") == 0 || strcmp(txt, "MODE 0") == 0)
  { enqueue(h, SERIAL_CMD_MODE_IDLE, 0); return; }

  if (strcmp(txt, "TRACK") == 0 || strcmp(txt, "1") == 0 || strcmp(txt, "MODE 1") == 0)
  { enqueue(h, SERIAL_CMD_MODE_TRACKING, 0); return; }

  if (strcmp(txt, "MANUAL") == 0 || strcmp(txt, "2") == 0 || strcmp(txt, "MODE 2") == 0)
  { enqueue(h, SERIAL_CMD_MODE_MANUAL, 0); return; }

  if (strcmp(txt, "MSTEP") == 0 || strcmp(txt, "MICRO") == 0 ||
      strcmp(txt, "MICROSTEP") == 0 || strcmp(txt, "3") == 0 ||
      strcmp(txt, "MODE 3") == 0)
  { enqueue(h, SERIAL_CMD_MODE_MICROSTEP, 0); return; }

  /* 校正/查詢 */
  if (strcmp(txt, "RECAL") == 0 || strcmp(txt, "CAL") == 0)
  { enqueue(h, SERIAL_CMD_RECALIBRATE, 0); return; }

  if (strcmp(txt, "STATUS") == 0 || strcmp(txt, "STAT?") == 0)
  { enqueue(h, SERIAL_CMD_STATUS_QUERY, 0); return; }

  if (strcmp(txt, "CALDATA") == 0 || strcmp(txt, "CAL?") == 0)
  { enqueue(h, SERIAL_CMD_CAL_QUERY, 0); return; }

  if (strcmp(txt, "CONFIG") == 0 || strcmp(txt, "CFG?") == 0)
  { enqueue(h, SERIAL_CMD_CONFIG_QUERY, 0); return; }

  if (strcmp(txt, "MCHK") == 0 || strcmp(txt, "MSTEP?") == 0 ||
      strcmp(txt, "TMC?") == 0)
  { enqueue(h, SERIAL_CMD_MICROSTEP_CHECK, 0); return; }

  if (strcmp(txt, "HELP") == 0)
  { enqueue(h, SERIAL_CMD_HELP, 0); return; }

  /* 控制週期: "CTL 5MS" 或 "PERIOD 2MS" */
  if (strncmp(txt, "CTL ", 4) == 0 || strncmp(txt, "PERIOD ", 7) == 0)
  {
    const char *p = &txt[7];
    if (txt[0] == 'C') p = &txt[4];
    int32_t ms = parse_period(p, &ok);
    if (ok) enqueue(h, SERIAL_CMD_CONTROL_PERIOD, ms);
    return;
  }

  /* 細分: "MS 16", "MSTEP 32", "MICRO 256" */
  if (strncmp(txt, "MS ", 3) == 0 || strncmp(txt, "MSTEP ", 6) == 0 ||
      strncmp(txt, "MICRO ", 6) == 0 || strncmp(txt, "MICROSTEP ", 10) == 0)
  {
    const char *p = &txt[3];
    if (strncmp(txt, "MICROSTEP ", 10) == 0) p = &txt[10];
    else if (strncmp(txt, "MSTEP ", 6) == 0) p = &txt[6];
    else if (strncmp(txt, "MICRO ", 6) == 0) p = &txt[6];

    int32_t ms = parse_microsteps(p, &ok);
    if (ok) enqueue(h, SERIAL_CMD_MICROSTEP_SET, ms);
    return;
  }

  /* 電流: "CUR 16 6 4" = IRUN IHOLD IHOLDDELAY */
  if (strncmp(txt, "CUR ", 4) == 0 || strncmp(txt, "CURRENT ", 8) == 0)
  {
    const char *p = &txt[4];
    if (strncmp(txt, "CURRENT ", 8) == 0) p = &txt[8];

    uint8_t irun = 0;
    uint8_t ihold = 0;
    uint8_t iholddelay = 0;
    if (parse_current_args(p, &irun, &ihold, &iholddelay))
    {
      int32_t packed = ((int32_t)iholddelay << 16) |
                       ((int32_t)irun << 8) |
                       (int32_t)ihold;
      enqueue(h, SERIAL_CMD_CURRENT_SET, packed);
    }
    return;
  }

  /* 手動stage: "MAN F2", "MAN 3" */
  if (strncmp(txt, "MAN ", 4) == 0)
  {
    int32_t stg = parse_stage(&txt[4], &ok);
    if (ok) enqueue(h, SERIAL_CMD_MANUAL_STAGE, stg);
    return;
  }

  /* 簡寫: "F1"~"F4" */
  if (txt[0] == 'F' && txt[1] >= '1' && txt[1] <= '4' && txt[2] == '\0')
  { enqueue(h, SERIAL_CMD_MANUAL_STAGE, txt[1] - '1'); return; }

  /* 簡寫: "R1"~"R4" */
  if (txt[0] == 'R' && txt[1] >= '1' && txt[1] <= '4' && txt[2] == '\0')
  { enqueue(h, SERIAL_CMD_MANUAL_STAGE, 4 + (txt[1] - '1')); return; }

  /* "STAGE 0"~"STAGE 7" */
  if (strncmp(txt, "STAGE ", 6) == 0)
  {
    int32_t v = parse_int(&txt[6], &ok);
    if (ok && v >= 0 && v <= 7)
      enqueue(h, SERIAL_CMD_MANUAL_STAGE, v);
    return;
  }
}

/* ---- public ---- */

void SerialCmd_Init(SerialCmd_HandleTypeDef *h, UART_HandleTypeDef *huart)
{
  memset(h, 0, sizeof(*h));
  h->huart = huart;
}

void SerialCmd_PollRx(SerialCmd_HandleTypeDef *h)
{
  if (h->huart == NULL) return;

  /* 清除可能的錯誤flag */
  if (__HAL_UART_GET_FLAG(h->huart, UART_FLAG_ORE))
    __HAL_UART_CLEAR_OREFLAG(h->huart);
  if (__HAL_UART_GET_FLAG(h->huart, UART_FLAG_NE))
    __HAL_UART_CLEAR_NEFLAG(h->huart);
  if (__HAL_UART_GET_FLAG(h->huart, UART_FLAG_FE))
    __HAL_UART_CLEAR_FEFLAG(h->huart);

  /* 一次把所有收到的byte讀完 */
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
      h->rx_len = 0;   /* 溢出就丟掉 */
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
