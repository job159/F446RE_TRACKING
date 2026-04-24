const fs = require('fs');
const path = require('path');
const {
  Document, Packer, Paragraph, TextRun, Table, TableRow, TableCell,
  AlignmentType, LevelFormat, HeadingLevel, BorderStyle, WidthType,
  ShadingType, VerticalAlign, PageBreak,
} = require('docx');

/* =========================================================================
 *  論文格式設定
 *  - 中文：新細明體 (PMingLiU) / 英文、數字：Times New Roman
 *  - 本文 12 pt、行距 1.5、段落首行縮排
 *  - 表格細黑框、無底色
 * ========================================================================= */
const FONT_CN = "PMingLiU";
const FONT_EN = "Times New Roman";
const FONT_CODE = "Consolas";
const SIZE_BODY = 24;       /* 12pt (半點為單位) */
const SIZE_CAPTION = 22;    /* 11pt 圖表標題 */
const SIZE_CODE = 20;       /* 10pt 程式碼區 */
const LINE = 360;           /* 1.5 倍行距 (240 = 單倍，480 = 雙倍) */
const INDENT_FIRST = 480;   /* 首行縮排 2 字元 (480 twips ≈ 2 中文字) */

const thinBorder = { style: BorderStyle.SINGLE, size: 4, color: "000000" };
const thinBorders = { top: thinBorder, bottom: thinBorder, left: thinBorder, right: thinBorder };

/* ---------- 共用段落工廠 ---------- */

function runCN(text, extra = {}) {
  return new TextRun({ text, font: FONT_CN, size: SIZE_BODY, ...extra });
}

function body(text, opts = {}) {
  return new Paragraph({
    alignment: AlignmentType.BOTH,
    indent: { firstLine: opts.noIndent ? 0 : INDENT_FIRST },
    spacing: { line: LINE, after: 80 },
    children: [runCN(text, opts.run || {})],
  });
}

/* 由多段 run 組成的本文段落 */
function bodyRuns(runs, opts = {}) {
  const children = runs.map(r => {
    if (typeof r === 'string') return runCN(r);
    return new TextRun({ font: FONT_CN, size: SIZE_BODY, ...r });
  });
  return new Paragraph({
    alignment: AlignmentType.BOTH,
    indent: { firstLine: opts.noIndent ? 0 : INDENT_FIRST },
    spacing: { line: LINE, after: 80 },
    children,
  });
}

/* 章節標題：H1 = 第 N 章，H2 = N.M，H3 = N.M.K */
function chapterTitle(text) {
  return new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { before: 240, after: 240, line: LINE },
    children: [new TextRun({ text, font: FONT_CN, bold: true, size: 32 })],
    heading: HeadingLevel.HEADING_1,
  });
}

function H2(text) {
  return new Paragraph({
    spacing: { before: 240, after: 120, line: LINE },
    children: [new TextRun({ text, font: FONT_CN, bold: true, size: 28 })],
    heading: HeadingLevel.HEADING_2,
  });
}

function H3(text) {
  return new Paragraph({
    spacing: { before: 180, after: 100, line: LINE },
    children: [new TextRun({ text, font: FONT_CN, bold: true, size: 26 })],
    heading: HeadingLevel.HEADING_3,
  });
}

/* 圖 / 表 標題：置中、小一號 */
function caption(text) {
  return new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { before: 80, after: 160, line: 240 },
    children: [new TextRun({ text, font: FONT_CN, bold: true, size: SIZE_CAPTION })],
  });
}

/* 程式碼塊 / 公式 */
function codeLine(text) {
  return new Paragraph({
    alignment: AlignmentType.LEFT,
    indent: { left: 480 },
    spacing: { line: 240, before: 0, after: 0 },
    children: [new TextRun({ text, font: FONT_CODE, size: SIZE_CODE })],
  });
}

/* 列表 (編號 / 條列) */
function numItem(text, level = 0) {
  return new Paragraph({
    numbering: { reference: "nums", level },
    spacing: { line: LINE, after: 60 },
    children: [runCN(text)],
  });
}
function bullet(text, level = 0) {
  return new Paragraph({
    numbering: { reference: "dots", level },
    spacing: { line: LINE, after: 60 },
    children: [runCN(text)],
  });
}

function pageBreak() {
  return new Paragraph({ children: [new PageBreak()] });
}

/* =========================================================================
 *  流程圖（以表格呈現：單欄方塊 + 箭頭，黑白樣式）
 * ========================================================================= */

function flowBox(text, widthDxa, type = "proc") {
  /* type: start/end/proc/decision */
  const br = { style: BorderStyle.SINGLE, size: type === "decision" ? 8 : 6, color: "000000" };
  const brs = { top: br, bottom: br, left: br, right: br };
  return new TableCell({
    borders: brs,
    width: { size: widthDxa, type: WidthType.DXA },
    margins: { top: 120, bottom: 120, left: 160, right: 160 },
    verticalAlign: VerticalAlign.CENTER,
    children: [new Paragraph({
      alignment: AlignmentType.CENTER,
      spacing: { line: 240 },
      children: [new TextRun({ text, font: FONT_CN, size: SIZE_BODY })],
    })],
  });
}

function arrowRow(totalWidth) {
  const noBorder = { style: BorderStyle.NONE, size: 0, color: "FFFFFF" };
  return new TableRow({
    children: [new TableCell({
      borders: { top: noBorder, bottom: noBorder, left: noBorder, right: noBorder },
      width: { size: totalWidth, type: WidthType.DXA },
      children: [new Paragraph({
        alignment: AlignmentType.CENTER,
        spacing: { before: 40, after: 40, line: 200 },
        children: [new TextRun({ text: "↓", font: FONT_EN, size: SIZE_BODY })],
      })],
    })],
  });
}

function flowChart(items, totalWidth) {
  const noBorder = { style: BorderStyle.NONE, size: 0, color: "FFFFFF" };
  const rows = [];
  items.forEach((it, i) => {
    rows.push(new TableRow({ children: [flowBox(it.text, totalWidth, it.type)] }));
    if (i !== items.length - 1) rows.push(arrowRow(totalWidth));
  });
  return new Table({
    alignment: AlignmentType.CENTER,
    width: { size: totalWidth, type: WidthType.DXA },
    columnWidths: [totalWidth],
    borders: {
      top: noBorder, bottom: noBorder, left: noBorder, right: noBorder,
      insideHorizontal: noBorder, insideVertical: noBorder,
    },
    rows,
  });
}

/* =========================================================================
 *  Y 型分支流程圖：上面若干序列步驟 → 一個判斷 → 左右兩條路 → 合流 → 結尾
 *  spec = {
 *    pre:    [ {type, text}, ... ]    判斷之前的單欄步驟
 *    decision: "..."                  判斷文字
 *    left:   { label, steps: [...], tail: "..." }
 *    right:  { label, steps: [...], tail: "..." }
 *    merge:  "..."                    合流後的終點
 *  }
 * ========================================================================= */
function branchingFlowChart(spec, totalWidth) {
  const noBorder = { style: BorderStyle.NONE, size: 0, color: "FFFFFF" };
  const innerNoBorder = { top: noBorder, bottom: noBorder, left: noBorder, right: noBorder };
  const halfWidth = Math.floor(totalWidth / 2);
  const rows = [];

  /* --- pre steps (single column, spans 2) --- */
  const singleSpan = (textObj) => new TableRow({
    children: [new TableCell({
      ...flowBox(textObj.text, totalWidth, textObj.type),
      columnSpan: 2,
    })],
  });
  /* docx-js 不支援把 TableCell 解構再覆蓋 — 改用直接建立 */
  const singleSpanCell = (textObj) => {
    const br = { style: BorderStyle.SINGLE, size: textObj.type === "decision" ? 8 : 6, color: "000000" };
    return new TableRow({
      children: [new TableCell({
        borders: { top: br, bottom: br, left: br, right: br },
        width: { size: totalWidth, type: WidthType.DXA },
        columnSpan: 2,
        margins: { top: 120, bottom: 120, left: 160, right: 160 },
        verticalAlign: VerticalAlign.CENTER,
        children: [new Paragraph({
          alignment: AlignmentType.CENTER,
          spacing: { line: 240 },
          children: [new TextRun({ text: textObj.text, font: FONT_CN, size: SIZE_BODY })],
        })],
      })],
    });
  };
  const arrowSpan = () => new TableRow({
    children: [new TableCell({
      borders: innerNoBorder,
      width: { size: totalWidth, type: WidthType.DXA },
      columnSpan: 2,
      children: [new Paragraph({
        alignment: AlignmentType.CENTER,
        spacing: { before: 30, after: 30, line: 200 },
        children: [new TextRun({ text: "↓", font: FONT_EN, size: SIZE_BODY })],
      })],
    })],
  });

  /* 雙欄：各自內容 */
  const twoColRow = (leftText, rightText, opts = {}) => {
    const makeCell = (txt, type) => {
      const br = { style: BorderStyle.SINGLE, size: type === "decision" ? 8 : 6, color: "000000" };
      const hasBox = type !== "label" && type !== "arrow";
      return new TableCell({
        borders: hasBox ? { top: br, bottom: br, left: br, right: br } : innerNoBorder,
        width: { size: halfWidth, type: WidthType.DXA },
        margins: hasBox
          ? { top: 120, bottom: 120, left: 120, right: 120 }
          : { top: 30, bottom: 30, left: 60, right: 60 },
        verticalAlign: VerticalAlign.CENTER,
        children: [new Paragraph({
          alignment: AlignmentType.CENTER,
          spacing: { line: 240 },
          children: [new TextRun({
            text: txt,
            font: type === "arrow" ? FONT_EN : FONT_CN,
            size: SIZE_BODY,
            italics: type === "label",
          })],
        })],
      });
    };
    return new TableRow({
      children: [makeCell(leftText, opts.leftType || "proc"), makeCell(rightText, opts.rightType || "proc")],
    });
  };

  /* 1. pre steps */
  spec.pre.forEach((it, i) => {
    rows.push(singleSpanCell(it));
    rows.push(arrowSpan());
  });

  /* 2. decision */
  rows.push(singleSpanCell({ text: spec.decision, type: "decision" }));

  /* 3. Yes/No label row */
  rows.push(twoColRow(
    `← ${spec.left.label}`,
    `${spec.right.label} →`,
    { leftType: "label", rightType: "label" },
  ));

  /* 4. 兩條分支的步驟列 */
  const leftSteps = spec.left.steps;
  const rightSteps = spec.right.steps;
  const maxSteps = Math.max(leftSteps.length, rightSteps.length);
  for (let i = 0; i < maxSteps; i++) {
    const L = leftSteps[i];
    const R = rightSteps[i];
    rows.push(twoColRow(L ? L.text : "", R ? R.text : "",
      { leftType: L ? (L.type || "proc") : "arrow", rightType: R ? (R.type || "proc") : "arrow" }));
    if (i !== maxSteps - 1) rows.push(twoColRow("↓", "↓", { leftType: "arrow", rightType: "arrow" }));
  }

  /* 5. 兩分支各自的尾段文字 (例如「→ 合流」) */
  rows.push(twoColRow(spec.left.tail || "↓", spec.right.tail || "↓",
    { leftType: "arrow", rightType: "arrow" }));

  /* 6. 合流後的終點 */
  if (spec.merge) {
    rows.push(singleSpanCell({ text: spec.merge, type: spec.mergeType || "end" }));
  }

  return new Table({
    alignment: AlignmentType.CENTER,
    width: { size: totalWidth, type: WidthType.DXA },
    columnWidths: [halfWidth, totalWidth - halfWidth],
    borders: {
      top: noBorder, bottom: noBorder, left: noBorder, right: noBorder,
      insideHorizontal: noBorder, insideVertical: noBorder,
    },
    rows,
  });
}

/* =========================================================================
 *  論文風格表格 (細黑線、無底色)
 * ========================================================================= */

function paperTable({ headers, rows, columnWidths, code: codeCols = [] }) {
  const headerRow = new TableRow({
    tableHeader: true,
    children: headers.map((h, i) => new TableCell({
      borders: thinBorders,
      width: { size: columnWidths[i], type: WidthType.DXA },
      margins: { top: 80, bottom: 80, left: 100, right: 100 },
      verticalAlign: VerticalAlign.CENTER,
      children: [new Paragraph({
        alignment: AlignmentType.CENTER,
        spacing: { line: 240 },
        children: [new TextRun({ text: h, font: FONT_CN, bold: true, size: SIZE_CAPTION })],
      })],
    })),
  });

  const dataRows = rows.map(r => new TableRow({
    children: r.map((txt, ci) => new TableCell({
      borders: thinBorders,
      width: { size: columnWidths[ci], type: WidthType.DXA },
      margins: { top: 60, bottom: 60, left: 100, right: 100 },
      verticalAlign: VerticalAlign.CENTER,
      children: [new Paragraph({
        alignment: ci === 0 ? AlignmentType.LEFT : AlignmentType.LEFT,
        spacing: { line: 240 },
        children: [new TextRun({
          text: txt,
          font: codeCols.includes(ci) ? FONT_CODE : FONT_CN,
          size: SIZE_CAPTION,
        })],
      })],
    })),
  }));

  return new Table({
    alignment: AlignmentType.CENTER,
    width: { size: columnWidths.reduce((a, b) => a + b, 0), type: WidthType.DXA },
    columnWidths,
    rows: [headerRow, ...dataRows],
  });
}

/* =========================================================================
 *  頁面設定：A4 / 標準 1 inch 四邊界
 * ========================================================================= */

const sectionProps = {
  page: {
    size: { width: 11906, height: 16838 },               /* A4 */
    margin: { top: 1440, right: 1440, bottom: 1440, left: 1440 },
  },
};

const contentWidth = 11906 - 1440 - 1440; /* = 9026 DXA */

/* =========================================================================
 *  文件內容
 * ========================================================================= */

const children = [];

/* ---------- 封面 / 標題 / 摘要 ---------- */

children.push(new Paragraph({
  alignment: AlignmentType.CENTER,
  spacing: { before: 600, after: 480, line: LINE },
  children: [new TextRun({
    text: "基於 STM32F446RE 與 TMC2209 之雙軸太陽追蹤系統設計與實作",
    font: FONT_CN, bold: true, size: 40,
  })],
}));

children.push(new Paragraph({
  alignment: AlignmentType.CENTER,
  spacing: { after: 360, line: LINE },
  children: [new TextRun({
    text: "Design and Implementation of a Dual-axis Solar Tracking System Based on STM32F446RE and TMC2209",
    font: FONT_EN, italics: true, size: 24,
  })],
}));

children.push(H2("摘　要"));
children.push(body(
  "本文提出一套以 STM32F446RE 微控制器為核心之雙軸太陽能追蹤系統。系統採用四顆光敏電阻 (LDR) 構成象限式光源感測陣列，經由內建之 ADC1 與 ADC2 以 DMA 循環模式同步取樣，再輔以開機自動校正程序與雜訊門檻門檻，取得抗環境雜訊之誤差訊號。驅動端採用兩顆 TMC2209 步進馬達驅動器，於 1/32 微步模式下提供兩軸獨立運動，並透過 UART 寫入暫存器設定電流、斷路與 StealthChop 參數。控制律採用三段式純比例控制搭配誤差飽和與軟限位，於避免極端姿態及失步之同時兼顧響應速度。本文亦提出完整之從 Bring-up、單元測試至機構適配之實作流程，作為後續延伸工作之參考。"));

children.push(bodyRuns([
  { text: "關鍵字：", bold: true },
  "太陽追蹤、STM32、TMC2209、步進馬達、比例控制、光敏電阻陣列。",
]));

children.push(bodyRuns([
  { text: "Keywords: ", bold: true, font: FONT_EN, italics: true },
  { text: "Solar tracking, STM32, TMC2209, stepper motor, proportional control, LDR array.", font: FONT_EN, italics: true },
]));

children.push(pageBreak());

/* =====================================================================
 *  第一章：系統架構與流程
 * ===================================================================== */

children.push(chapterTitle("第一章　系統架構與控制流程"));

children.push(H2("1.1　系統方塊圖"));
children.push(body("本系統由感測、控制、驅動與機構四個子系統構成。感測端以四顆光敏電阻構成象限陣列，經類比訊號至 STM32 之 ADC。控制端以 STM32F446RE 之 Cortex-M4 核心執行主迴圈。驅動端由兩顆 TMC2209 各自透過 UART 接收設定命令、透過 STEP/DIR 腳接收運動命令。機構則由兩軸齒輪組合，分別對應水平 (Tilt) 與垂直 (Azimuth) 兩個自由度。"));

children.push(H2("1.2　系統啟動與主迴圈流程"));
children.push(body("系統自重置起始，依序完成時脈與周邊初始化、應用模組初始化，並進入以主迴圈為中心之無限循環。圖 1-1 為系統啟動至進入主迴圈之整體流程。"));

children.push(flowChart([
  { type: 'start', text: "Reset" },
  { type: 'proc',  text: "HAL_Init、SystemClock_Config\n(HSI + PLL → SYSCLK 84 MHz)" },
  { type: 'proc',  text: "MX_GPIO_Init、MX_DMA_Init\nMX_ADC1_Init、MX_ADC2_Init\nMX_TIM1_Init、MX_TIM3_Init\nMX_USART2_UART_Init、MX_UART4_Init、MX_UART5_Init" },
  { type: 'proc',  text: "AppMain_Init：\nTelemetry/ADC/SerialCmd/LDR/TrackerController/\nManualControl/MotorControl 初始化" },
  { type: 'proc',  text: "進入主迴圈 while(1) { AppMain_Task(); }" },
], 6000));
children.push(caption("圖 1-1　系統啟動與初始化流程"));

children.push(H2("1.3　主迴圈 (AppMain_Task) 內部流程"));
children.push(body("主迴圈於每次呼叫時依序處理：按鈕輸入、序列指令、ADC 取樣、模式切換與控制運算，最後更新遙測快照。其流程如圖 1-2 所示。"));

children.push(flowChart([
  { type: 'start', text: "AppMain_Task()" },
  { type: 'proc',  text: "poll_button()：偵測 PC13 下緣，切換 Manual 段" },
  { type: 'proc',  text: "SerialCmd_PollRx()：自 USART2 接收指令" },
  { type: 'proc',  text: "AppAdc_Task()：讀取 DMA buffer 並執行低通濾波" },
  { type: 'proc',  text: "handle_cmd()：解析指令佇列" },
  { type: 'decision', text: "(now − last_ctrl_tick) ≥ ctrl_period_ms ?" },
  { type: 'proc',  text: "run_control(now)：依模式分派 IDLE／TRACKING／MANUAL" },
  { type: 'proc',  text: "update_snapshot() 與 Telemetry_Task()" },
], 6400));
children.push(caption("圖 1-2　主迴圈 (AppMain_Task) 資料處理流程"));

children.push(pageBreak());

children.push(H2("1.4　系統模式狀態機"));
children.push(body("系統共包含三種工作模式：IDLE、TRACKING 與 MANUAL，並以開機校正子狀態 IDLE_CALIBRATING 作為系統冷啟動之過渡。狀態間之轉移如圖 1-3。"));

children.push(flowChart([
  { type: 'start', text: "Boot\n→ MODE_IDLE / IDLE_CALIBRATING" },
  { type: 'proc',  text: "累積 LDR baseline 與 noise floor (5 s)" },
  { type: 'decision', text: "到達 SYS_BOOT_CALIBRATION_MS？" },
  { type: 'proc',  text: "finalize_cal() 完成校正\n依 mode_after_cal 進入 TRACKING 或 MANUAL" },
  { type: 'decision', text: "收到 RECAL / MODE 指令？" },
  { type: 'proc',  text: "進入對應模式：IDLE / TRACKING / MANUAL" },
  { type: 'end',   text: "模式間可由指令或 B1 按鈕切換" },
], 6400));
children.push(caption("圖 1-3　系統工作模式狀態轉移圖"));

children.push(H2("1.5　追蹤控制週期流程"));
children.push(body("於 TRACKING 模式下，每控制週期執行一次完整之感測-判斷-控制鏈路。首先更新 LDR frame 並計算誤差與有效性旗標，而後依 is_valid 分成兩條路徑：光源無效時停止馬達並重置控制器，光源有效時依序執行誤差飽和、增益選擇、軟限位與 ramp，最後累加位置估算。流程如圖 1-4 所示。"));

children.push(branchingFlowChart({
  pre: [
    { type: 'start', text: "進入 run_control(now)，mode = TRACKING" },
    { type: 'proc',  text: "步驟 1：LdrTracking_UpdateFrame\nraw[4] → delta[4] → total、contrast → error_x、error_y\n並依 total ≥ 140 且 contrast ≥ 28 設定 is_valid" },
  ],
  decision: "is_valid == 1 ?",
  left: {
    label: "No (光源無效)",
    steps: [
      { type: 'proc', text: "TrackerController_Reset\n(純 P 無歷史，重置為保險)" },
      { type: 'proc', text: "MotorControl_StopAll\n兩軸 PWM CCR = 0" },
    ],
    tail: "↓",
  },
  right: {
    label: "Yes (光源有效)",
    steps: [
      { type: 'proc', text: "步驟 2：TrackerController_Run\nex、ey ← clamp(error, ±TRACK_ERR_CAP)\n依 |err| 選 KP_SMALL / MEDIUM / LARGE\nout = kp × err × OUTPUT_GAIN × POS/NEG_SCALE\nclamp(out, ±MAX_STEP_HZ)" },
      { type: 'proc', text: "步驟 3：MotorControl_ApplyCommand\n軟限位 clamp (越界該方向 hz = 0)\nramp 漸進到新速度 (RAMP_STEP_HZ)\n位置累加：axis_pos += hz × dt" },
    ],
    tail: "↓",
  },
  merge: "本週期結束，回到主迴圈等待下一個控制 tick",
  mergeType: "end",
}, 8400));
children.push(caption("圖 1-4　追蹤模式單一控制週期流程 (Y 型分支)"));

children.push(pageBreak());

/* =====================================================================
 *  第二章：實驗與實作步驟
 * ===================================================================== */

children.push(chapterTitle("第二章　實驗與實作步驟"));

children.push(body("本章依循工程實務，將開發流程分為四個階段：平台 Bring-up、單元測試、閉迴路驗證與機構適配。各階段各有驗證目標，且須在前一階段完成後方可進入下一階段。"));

children.push(H2("2.1　第一階段：平台 Bring-up"));
children.push(body("目的為確認微控制器可穩定上傳程式、除錯連線正常、並驗證基本輸入輸出路徑。"));
children.push(numItem("於 STM32CubeIDE 建立專案，選用 MCU 型號 STM32F446RET6，並啟用 NUCLEO-F446RE 板載配置。"));
children.push(numItem("於 .ioc 設定中選擇 HSI + PLL，SYSCLK 設定為 84 MHz；將 SYS Debug 設為 Serial Wire；啟用 USART2 作為 ST-Link VCP，波特率 115200、8N1。"));
children.push(numItem("配置 PC13 為數位輸入並啟用內部上拉，作為板載 B1 按鈕；配置 PB8、PC6、PC8、PC9 為推挽輸出。"));
children.push(numItem("撰寫最小可執行程式：B1 按下時翻轉 LED，並於 USART2 輸出字串以驗證除錯路徑。"));

children.push(H2("2.2　第二階段：感測與致動單元測試"));

children.push(H3("2.2.1　ADC 與 LDR 陣列"));
children.push(body("將光敏電阻與固定電阻構成分壓電路，連接至 PC1、PC2、PC3、PC4 四腳。ADC1 設 IN12、IN13 兩通道，ADC2 設 IN11、IN14 兩通道，兩者均開啟 Scan + Continuous 模式，並以 DMA 循環模式自動填入 buffer。考量 ADC 轉換速率極快，於 DMA HT 與 TC 中斷關閉後，由主迴圈週期性讀取 buffer，可避免中斷氾濫佔用 CPU。"));
children.push(body("以遙測輸出四通道之 raw 值，以遮光與照光方式測試通道方向與校正；若硬體分壓為反向 (越亮 ADC 值越低)，則設定 ADC_INVERT = 1 於軟體層翻轉。"));

children.push(H3("2.2.2　TMC2209 驅動器與步進馬達"));
children.push(body("TMC2209 以 12 V 邏輯電源、12 V 馬達電源供電，EN 腳預設拉高 (禁能)。UART 波特率設為 115200，依序寫入 GCONF、IHOLD_IRUN、CHOPCONF 與 PWMCONF 四組暫存器，每筆指令間隔 1 ms，以確保寫入穩定。驅動時脈由 TIM1_CH1 與 TIM3_CH1 之 PWM 輸出提供，Prescaler 設為 83 以取得 1 MHz 計時基，透過動態修改 ARR 可得不同之 step 頻率。"));
children.push(body("測試時先於桌面空載運轉，以 Manual 指令 F1 至 F7 (正轉) 與 R1 至 R7 (反轉) 依序送出七段速度 (50、100、200、400、600、1000、5000 Hz)，逐段確認方向正確性、噪音水準及是否發生失步。"));

children.push(H2("2.3　第三階段：閉迴路追蹤驗證"));
children.push(numItem("系統上電後自動執行 5 秒光源基準校正，期間須維持環境光穩定。"));
children.push(numItem("透過 USART2 終端送 CAL? 查詢校正結果 (baseline 與 noise_floor)；送 STATUS 查詢當前模式、total、contrast。"));
children.push(numItem("以手電筒模擬日照，由不同角度照射 LDR 陣列，觀察 error_x、error_y 與輸出之 hz 命令。"));
children.push(numItem("確認微小偏差 (< 死區 0.020) 下馬達停轉；較大偏差下依三段式 KP 產生對應速度。"));
children.push(numItem("低光或無光源時，total 或 contrast 低於門檻，is_valid 應為 0，馬達自動停止。"));

children.push(H2("2.4　第四階段：機構適配與軟限位設定"));
children.push(body("本階段之目的在於將控制層所採用之位置單位 (microstep) 與實際機構可動範圍對齊，並透過下列設定參數完成機構參數化。所涉及之巨集皆位於 tracking_config.h，以及 stepper_tmc2209.c 中之 CHOPCONF 欄位。"));

children.push(body("(a) 微步數與每圈步數之換算：每圈步數由 TMC2209 之 CHOPCONF.MRES 欄位決定 (本系統 MRES = 3，對應 1/32 微步，每圈 6400 步)。若將 MRES 改為其他值，每圈步數需對應調整，軟限位與控制端 hz 皆需同步換算。"));

children.push(body("(b) 軟限位範圍：由 M1_LIMIT_STEPS 與 M2_LIMIT_STEPS 設定。換算公式為："));
children.push(codeLine("LIMIT_STEPS = (每圈步數) × gear_ratio × (允許機構角度 / 360°)"));
children.push(body("本系統採用之配置如下表所示。若日後機構改造、齒輪比變更，或欲放寬/收緊可動範圍，直接修改此兩巨集即可。"));

children.push(paperTable({
  headers: ["軸", "巨集", "值", "意義"],
  columnWidths: [1400, 2000, 1400, contentWidth - 1400 - 2000 - 1400],
  code: [1, 2],
  rows: [
    ["M1 水平 (Tilt)",  "M1_LIMIT_STEPS",  "800",   "1:1 齒比，機構 ±45°"],
    ["M2 垂直 (Azm)",   "M2_LIMIT_STEPS",  "9600",  "1:3 齒比，機構 ±180° (馬達 ±540°)"],
  ],
}));
children.push(caption("表 2-1　軟限位參數設定"));

children.push(body("(c) 軟限位啟/停：由 M1_LIMIT_ENABLE 與 M2_LIMIT_ENABLE 控制，預設為 1。於機構測試初期可先設為 0 以繞過限位，待機構與位置估算驗證正確後再啟用。"));

children.push(body("(d) 初始位置假設：系統開機時預設機構位於可動範圍之中點，位置累計器歸零。若實際起始姿態偏移，可透過 HOME 指令 (將 axis1_position_steps、axis2_position_steps 強制設為 0) 重新標定。Manual 模式亦會累加位置估算，若 Manual 操作已突破軟限位邊界，切回 TRACKING 時該方向將被 clamp，必須先反向退回或以 HOME 指令重置。"));

children.push(body("(e) 追蹤方向修正：若機構裝設方向與預期相反，可將 M1_TRACK_DIR 或 M2_TRACK_DIR 由 +1 改為 −1。此設定僅作用於 TRACKING 模式之 hz 輸出符號，對 Manual 模式無影響；因此修改後 Manual 操作邏輯不變，僅追蹤方向反轉。"));

children.push(pageBreak());

/* =====================================================================
 *  第三章：硬體設計與暫存器設定
 * ===================================================================== */

children.push(chapterTitle("第三章　硬體設計與暫存器設定"));

children.push(H2("3.1　TMC2209 暫存器設定"));
children.push(body("系統於馬達初始化時，依序寫入 GCONF、IHOLD_IRUN、CHOPCONF 與 PWMCONF 四組暫存器；每筆指令為 8 位元組之 UART frame (含 CRC-8)，寫入間隔 1 ms。關鍵值如表 3-1 所示。"));

children.push(paperTable({
  headers: ["暫存器", "位址", "寫入值", "功能說明"],
  columnWidths: [1500, 1200, 1800, contentWidth - 1500 - 1200 - 1800],
  code: [1, 2],
  rows: [
    ["GCONF",      "0x00", "0x000000C0",
     "bit6 pdn_disable = 1、bit7 mstep_reg_select = 1，啟用 UART 控制 microstep。"],
    ["IHOLD_IRUN", "0x10", "0x00041006",
     "ihold = 6、irun = 16、iholddelay = 4，對應驅動電流。"],
    ["CHOPCONF",   "0x6C", "0x13000053",
     "MRES 欄位設為 3，對應 1/32 微步；其餘取預設基底。"],
    ["PWMCONF",    "0x70", "0xC10D0024",
     "啟用 PWM_AUTOSCALE 與 PWM_AUTOGRAD，低速時使用 StealthChop 達成近乎靜音運轉。"],
  ],
}));
children.push(caption("表 3-1　TMC2209 初始化暫存器設定"));

children.push(H3("3.1.1　關鍵欄位分析"));
children.push(bullet("IRUN = 16/32 (約 50% 全電流)：依 Vref 與 sense 電阻而定，於本硬體約 1.5 A RMS。連續運轉若散熱不足可再降至 12~14。"));
children.push(bullet("IHOLD = 6/32 (約 19%)：提供靜止保持扭矩，兼顧熱功耗。"));
children.push(bullet("IHOLDDELAY = 4：停轉後約 0.5 秒內由 irun 線性下降至 ihold，可避免甫停止即失扭。"));
children.push(bullet("CHOPCONF.MRES = 3 對應 1/32 微步；若改為 MRES = 0 (1/256)，須對應將控制端之 hz 放大 8 倍。"));
children.push(bullet("PWMCONF 開啟 StealthChop 自動調參，使低速段近乎無聲；高速段自動切換為 SpreadCycle 以維持扭矩。"));

children.push(H2("3.2　STM32F446RE 週邊設定"));
children.push(bullet("RCC：HSI 16 MHz → PLL (PLLM = 16, PLLN = 336, PLLP = 4)，SYSCLK = 84 MHz，APB1 = 42 MHz (TIM 倍頻為 84 MHz)，APB2 = 84 MHz。"));
children.push(bullet("ADC1：IN12 (PC2, rank 2)、IN13 (PC3, rank 1)；Scan + Continuous + DMA，取樣時間 28 cycles。"));
children.push(bullet("ADC2：IN11 (PC1, rank 2)、IN14 (PC4, rank 1)；組態與 ADC1 相同。DMA 分別使用 DMA2 Stream0 與 Stream3，circular half-word 模式。"));
children.push(bullet("TIM1 (APB2, 84 MHz)：Prescaler 83 → 計時基 1 MHz，PWM1 CH1 輸出 M1 STEP 脈波。"));
children.push(bullet("TIM3 (APB1 TIM clk, 84 MHz)：設定同 TIM1，輸出 M2 STEP；Duty 固定取 (ARR + 1)/2。"));
children.push(bullet("USART2：115200 8N1，做為 ST-Link VCP 命令與遙測之雙向通道。"));
children.push(bullet("UART4、UART5：115200 8N1，分別對應 M1、M2 之 TMC2209 UART 控制。"));

children.push(H2("3.3　IO 腳位配置"));
children.push(body("表 3-2 彙整本系統使用之 IO 腳位、對應之訊號與功能描述。"));

children.push(paperTable({
  headers: ["Pin", "GPIO Label", "Signal", "用途說明"],
  columnWidths: [1100, 1600, 1800, contentWidth - 1100 - 1600 - 1800],
  code: [0, 1, 2],
  rows: [
    ["PC3",  "LDR_TL",     "ADC1_IN13",  "左上光敏電阻 (通道 0)"],
    ["PC4",  "LDR_TR",     "ADC2_IN14",  "右上光敏電阻 (通道 1)"],
    ["PC2",  "LDR_DR",     "ADC1_IN12",  "右下光敏電阻 (通道 2)"],
    ["PC1",  "LDR_DL",     "ADC2_IN11",  "左下光敏電阻 (通道 3)"],
    ["PA8",  "M1_STEP",    "TIM1_CH1",   "馬達 1 步進脈波 (水平 / Tilt 軸)"],
    ["PC6",  "M1_DIR",     "GPIO Output","馬達 1 方向訊號"],
    ["PB8",  "M1_EN",      "GPIO Output","馬達 1 致能 (低準位啟動)"],
    ["PC10", "M1_UART_TX", "UART4_TX",   "馬達 1 TMC2209 UART 寫入"],
    ["PC11", "M1_UART_RX", "UART4_RX",   "馬達 1 TMC2209 UART 讀取"],
    ["PA6",  "M2_STEP",    "TIM3_CH1",   "馬達 2 步進脈波 (垂直 / Azimuth 軸)"],
    ["PC8",  "M2_DIR",     "GPIO Output","馬達 2 方向訊號"],
    ["PC9",  "M2_EN",      "GPIO Output","馬達 2 致能"],
    ["PC12", "M2_UART_TX", "UART5_TX",   "馬達 2 TMC2209 UART 寫入"],
    ["PD2",  "M2_UART_RX", "UART5_RX",   "馬達 2 TMC2209 UART 讀取"],
    ["PC13", "USER_BTN",   "GPIO Input", "板載 B1 按鈕，用於切換 Manual 段"],
    ["PA2",  "CMD_TX",     "USART2_TX",  "ST-Link VCP 命令 / 遙測傳出"],
    ["PA3",  "CMD_RX",     "USART2_RX",  "ST-Link VCP 命令接收"],
    ["PA13", "TMS",        "SWDIO",      "SWD 除錯資料"],
    ["PA14", "TCK",        "SWCLK",      "SWD 除錯時脈"],
    ["PB3",  "SWO",        "JTDO-SWO",   "SWD trace 輸出"],
  ],
}));
children.push(caption("表 3-2　IO 腳位配置表"));

children.push(body("詳細晶片腳位實體佈局，可參考 STM32CubeMX 所產生之 IO 圖。", { noIndent: false }));

children.push(pageBreak());

/* =====================================================================
 *  第四章：模式切換、初始化與光源自適應演算法
 * ===================================================================== */

children.push(chapterTitle("第四章　模式切換、初始化與光源自適應演算法"));

children.push(H2("4.1　工作模式與狀態機"));
children.push(body("系統定義三種互斥之工作模式，並以子狀態細分 IDLE 之行為。各模式之用途與限制整理如下。"));
children.push(bullet("MODE_IDLE：馬達全停；含兩個子狀態 IDLE_CALIBRATING (進行中之光源校正) 與 IDLE_WAIT_CMD (閒置待命)。"));
children.push(bullet("MODE_TRACKING：閉迴路追蹤模式，每控制週期依序執行 LDR frame 更新、控制器運算與馬達命令下達。"));
children.push(bullet("MODE_MANUAL：手動模式，依選定之段別 (F1 至 F7 為正轉，R1 至 R7 為反轉) 輸出固定 hz，不受光源影響。"));
children.push(body("B1 按鈕於任何模式下按下均可立即切換至 Manual 下一段，無須等待校正完成。若於校正中收到 TRACKING 指令，系統將其排隊至 mode_after_cal，待校正完成後自動切入。"));

children.push(H2("4.2　初始化流程 (AppMain_Init)"));
children.push(numItem("以 memset 清空全域狀態結構 g。"));
children.push(numItem("Telemetry_Init：以 USART2 為輸出、週期 100 ms 為遙測頻率。"));
children.push(numItem("AppAdc_Init：啟動 ADC1 與 ADC2 之 DMA，關閉 HT 與 TC 中斷。"));
children.push(numItem("SerialCmd_Init、LdrTracking_Init 並 ForceRecalibration、TrackerController_Init、ManualControl_Init。"));
children.push(numItem("MotorControl_Init：兩軸分別呼叫 StepperTmc2209_Init，寫入四組暫存器、拉低 EN、啟動 TIM PWM、設為 Stage 0。"));
children.push(numItem("設定 mode = IDLE、idle_sub = CALIBRATING、mode_after_cal = TRACKING，5 秒後自動進入追蹤。"));

children.push(H2("4.3　光源自適應演算法"));

children.push(H3("4.3.1　感測器座標定義"));
children.push(body("由正面觀察，四顆 LDR 編號如圖 4-1 所示。通道定義直接對應程式中 error 計算之左右上下組合。"));

{
  const W = 3200, CW = W / 2;
  const cell = (txt) => new TableCell({
    borders: thinBorders,
    width: { size: CW, type: WidthType.DXA },
    margins: { top: 320, bottom: 320, left: 160, right: 160 },
    verticalAlign: VerticalAlign.CENTER,
    children: [new Paragraph({
      alignment: AlignmentType.CENTER,
      children: [new TextRun({ text: txt, font: FONT_CN, bold: true, size: SIZE_BODY })],
    })],
  });
  children.push(new Table({
    alignment: AlignmentType.CENTER,
    width: { size: W, type: WidthType.DXA },
    columnWidths: [CW, CW],
    rows: [
      new TableRow({ children: [cell("0：左上 (LDR_TL)"), cell("1：右上 (LDR_TR)")] }),
      new TableRow({ children: [cell("3：左下 (LDR_DL)"), cell("2：右下 (LDR_DR)")] }),
    ],
  }));
}
children.push(caption("圖 4-1　LDR 四象限編號 (由正面看)"));

children.push(H3("4.3.2　自動校正演算法"));
children.push(body("系統於開機後 5 秒 (SYS_BOOT_CALIBRATION_MS) 內，每控制週期累積一次統計量，如式 (4-1)："));
children.push(codeLine("cal_sum[i]  += raw[i]"));
children.push(codeLine("cal_min[i]   = min(cal_min[i], raw[i])"));
children.push(codeLine("cal_max[i]   = max(cal_max[i], raw[i])"));
children.push(codeLine("cal_samples += 1                            ……(4-1)"));
children.push(body("校正結束時執行 FinalizeCalibration，依式 (4-2) 計算每通道之 baseline 與 noise floor："));
children.push(codeLine("baseline[i]    = cal_sum[i] / cal_samples"));
children.push(codeLine("span           = cal_max[i] - cal_min[i]"));
children.push(codeLine("noise_floor[i] = max(span + MARGIN, MIN_NOISE_FLOOR) ……(4-2)"));
children.push(body("baseline 反映當下環境光平均值，noise_floor 包含通道量測雜訊幅值及額外安全邊界，以避免誤將雜訊視為光源訊號。"));

children.push(H3("4.3.3　光源訊號有效性判定"));
children.push(body("每取樣週期由式 (4-3) 計算四通道之光增量 delta："));
children.push(codeLine("floor    = baseline[i] + noise_floor[i]"));
children.push(codeLine("delta[i] = max(raw[i] - floor, 0)"));
children.push(codeLine("total    = Σ delta[i]"));
children.push(codeLine("contrast = max(delta) − min(delta)              ……(4-3)"));
children.push(body("當 total 與 contrast 任一低於設定門檻時，視為無有效光源，is_valid = 0，馬達停止追蹤。門檻預設值 TRACK_VALID_TOTAL_MIN = 140、TRACK_DIRECTION_CONTRAST_MIN = 28。"));

children.push(H3("4.3.4　方向誤差計算"));
children.push(body("將四通道 delta 依上下左右分組求和，得到式 (4-4) 之正規化誤差："));
children.push(codeLine("left  = delta[0] + delta[3]       // TL + DL"));
children.push(codeLine("right = delta[1] + delta[2]       // TR + DR"));
children.push(codeLine("top   = delta[0] + delta[1]       // TL + TR"));
children.push(codeLine("bot   = delta[3] + delta[2]       // DL + DR"));
children.push(codeLine("error_x = (right − left) / total"));
children.push(codeLine("error_y = (top   − bot ) / total                ……(4-4)"));
children.push(body("透過除以 total 正規化，使誤差在不同光強下維持可比尺度，值域理論上為 −1 至 +1。極端陰影導致誤差爆衝時，將於第五章所述之飽和環節予以夾制。"));

children.push(pageBreak());

/* =====================================================================
 *  第五章：控制策略與馬達約束
 * ===================================================================== */

children.push(chapterTitle("第五章　控制策略與馬達約束"));

children.push(H2("5.1　控制律概述"));
children.push(body("考量步進馬達系統無需積分項消除穩態誤差 (位置由離散 step 累積、具自然記憶性)，且為避免積分飽和與微分雜訊放大，本文採用純比例 (P-only) 控制律，輔以三段式增益、死區、輸出飽和與機構軟限位，以達到快速收斂與穩定之間的平衡。"));
children.push(bullet("兩軸彼此獨立：error_y 驅動 axis1 (垂直)，error_x 驅動 axis2 (水平)。"));
children.push(bullet("各 cycle 輸出僅取決於當下誤差，無歷史狀態，故系統對上一時刻之動作不產生記憶，避免來回擺盪。"));
children.push(bullet("兩軸無互相抑制設計 (dominance logic)，可同時動作。"));

children.push(H2("5.2　三段式比例增益與死區"));
children.push(body("對單一軸而言，函式 run_axis(error) 依式 (5-1) 計算 hz 命令："));
children.push(codeLine("abs_e = |error|"));
children.push(codeLine("if   abs_e ≤ DEADBAND       : return 0            // 死區停機"));
children.push(codeLine("elif abs_e ≤ ERR_SMALL      : kp = KP_SMALL"));
children.push(codeLine("elif abs_e ≤ ERR_MEDIUM     : kp = KP_MEDIUM"));
children.push(codeLine("else                        : kp = KP_LARGE"));
children.push(codeLine("out   = kp × error × OUTPUT_GAIN"));
children.push(codeLine("out  *= (out ≥ 0) ? POS_SCALE : NEG_SCALE"));
children.push(codeLine("out   = clamp(out, ±MAX_STEP_HZ)                   ……(5-1)"));
children.push(body("POS_SCALE 與 NEG_SCALE 用以補償機構左右裝配之不對稱：若實測發現某方向阻力較大，可調整對應方向之尺度係數。表 5-1 列出本系統實際使用之參數。"));

children.push(paperTable({
  headers: ["軸", "KP_SMALL", "KP_MEDIUM", "KP_LARGE", "OUTPUT_GAIN", "POS_SCALE", "NEG_SCALE", "MAX_HZ"],
  columnWidths: [1400, 1000, 1100, 1000, 1200, 1100, 1100, 1126],
  code: [1, 2, 3, 4, 5, 6, 7],
  rows: [
    ["M1 水平 (Tilt)",  "100", "400", "800", "1.0", "1.10", "1.24", "60000"],
    ["M2 垂直 (Azm)",   "60",  "280", "560", "1.0", "1.02", "1.16", "60000"],
  ],
}));
children.push(caption("表 5-1　兩軸控制器參數 (PID_ERR_DEADBAND = 0.020, ERR_SMALL = 0.060, ERR_MEDIUM = 0.150)"));

children.push(H2("5.3　誤差飽和：陰影場景之防呆"));
children.push(body("當一側 LDR 完全處於陰影，delta 趨近於 0，另一側若強照光，依式 (4-4) error 將逼近 ±1.0，造成馬達全速衝向實際上僅為陰影邊緣之方向。為避免此現象，TrackerController 於誤差代入控制律前先行飽和，如式 (5-2)："));
children.push(codeLine("ex = clamp(error_x, −TRACK_ERR_CAP, +TRACK_ERR_CAP)   // ±0.7"));
children.push(codeLine("ey = clamp(error_y, −TRACK_ERR_CAP, +TRACK_ERR_CAP)   ……(5-2)"));
children.push(body("此層為物理量之夾制，與第 4.3.3 節 total / contrast 門檻互為備援：任一層失效時，另一層仍可提供保護。"));

children.push(H2("5.4　軟限位：機構保護"));
children.push(body("本系統未配置機械式限位開關，改以軟體估測之位置累計為準，於 MotorControl_ApplyCommand 之進入處先行檢查，如式 (5-3)："));
children.push(codeLine("if axis1_pos ≥ +M1_LIMIT_STEPS and hz1 > 0 : hz1 = 0"));
children.push(codeLine("if axis1_pos ≤ −M1_LIMIT_STEPS and hz1 < 0 : hz1 = 0"));
children.push(codeLine("// axis2 同上"));
children.push(codeLine("axis_pos += (float)hz × dt_ms / 1000                  ……(5-3)"));
children.push(bullet("M1_LIMIT_STEPS = 800 → 對應機構 ±45° (1:1 齒比)。"));
children.push(bullet("M2_LIMIT_STEPS = 9600 → 對應機構 ±180° (1:3 齒比，馬達 ±540°)。"));
children.push(bullet("Manual 模式亦累加位置；若 Manual 已越界，返回 TRACKING 時將被限制往同一方向，須反向退回或透過 HOME 指令歸零。"));

children.push(H2("5.5　速度 ramp 與方向切換"));
children.push(body("為避免速度躍升造成失步，所有速度變化均以線性 ramp 執行：每 RAMP_DELAY_MS (1 ms) 最多增量 RAMP_STEP_HZ (800 Hz)。方向切換時採三段處理：先由目前速度 ramp 至最低段速、切換 DIR 腳位、等待 DIR_SETTLE_MS (2 ms) 以待磁路反向穩定、再 ramp 至目標速度。停止 (hz = 0) 則直接設定 CCR 為 0 並產生 update event，使 PWM 立即停止輸出。"));

children.push(H2("5.6　電流、扭矩與熱設計考量"));
children.push(bullet("IRUN = 16/32：對應約 50 % 全電流，於連續運轉下搭配散熱片可穩定工作。若長時間高負載致溫升過高，可再降至 12~14。"));
children.push(bullet("IHOLD = 6/32：提供靜止保持力矩，同時使靜止功耗降至 ~ 19 %。"));
children.push(bullet("IHOLDDELAY = 4：停轉後約 0.5 秒線性下降，避免甫停即失扭。"));
children.push(bullet("微步與扭矩之取捨：於相同電流下，微步數越多則每步轉矩切換幅度越小、定位解析度越高，但靜態轉矩略降，故選用 1/32 為折衷。"));

children.push(H2("5.7　頻率與角速度換算"));
children.push(body("於 Prescaler = 83 條件下，TIM 計時基 counter_clk = 1 MHz，step 頻率與 ARR 之關係如式 (5-4)："));
children.push(codeLine("ARR     = counter_clk / hz − 1"));
children.push(codeLine("ω_mech  = hz × 1.8° / microstep / gear_ratio          ……(5-4)"));
children.push(body("例如 M2 軸於 hz = 5000 時，機構角速度為 5000 × 1.8 / 32 / 3 ≈ 93.75°/s。"));

children.push(H2("5.8　控制週期之選擇"));
children.push(body("控制週期由 SYS_CONTROL_PERIOD_MS 決定，系統允許 1、2、5 ms 三種設定。週期越短則反應越快，惟 UART 寫 TMC 與 ramp 所佔時間比重升高，故預設採用 5 ms。ADC 濾波採一階低通，new 權重為 9/10，舊值為 1/10；於 5 ms 取樣下時間常數約 5 ms，足以濾除 50/60 Hz 電源干擾而不延遲系統響應。"));

children.push(pageBreak());

/* =====================================================================
 *  第六章：模式比較與成果
 * ===================================================================== */

children.push(chapterTitle("第六章　模式比較與實驗成果"));

children.push(H2("6.1　三種工作模式之比較"));
children.push(body("表 6-1 比較三種工作模式之用途、使用之功能模組與主要特色。"));

children.push(paperTable({
  headers: ["模式", "主要用途", "使用模組", "特色與限制"],
  columnWidths: [1800, 2400, 2400, contentWidth - 1800 - 2400 - 2400],
  rows: [
    ["IDLE / CAL", "開機前 5 秒自動校正環境光基準",
     "LdrTracking_Accumulate、Finalize", "馬達全停，要求環境光穩定"],
    ["TRACKING", "閉迴路光源追蹤",
     "LDR + TrackerController + MotorControl",
     "純比例控制、三段 KP、死區、軟限位"],
    ["MANUAL", "手動速度段切換 (F1~F7 / R1~R7)",
     "ManualControl + MotorControl",
     "固定 hz 開迴路，位置估算仍累加"],
    ["異常防呆", "低光源或陰影場景",
     "saturate_err、is_valid 判定",
     "馬達停轉、保護機構"],
  ],
}));
children.push(caption("表 6-1　系統工作模式比較"));

children.push(H2("6.2　量化實驗結果"));
children.push(body("表 6-2 為本系統後續實測所需填入之量化指標，目前先行保留作為實驗規劃依據。"));

children.push(paperTable({
  headers: ["指標", "定義與量測方式", "量測結果"],
  columnWidths: [2800, 3826, contentWidth - 2800 - 3826],
  rows: [
    ["靜態追蹤精度",       "光源固定時，error_x 與 error_y 之 RMS (等效角度)", ""],
    ["階躍響應時間",       "光源階躍後，|err| 收斂至死區以內所需時間 (ms)",   ""],
    ["無效光源拒絕率",     "遮光測試中 is_valid = 0 之比例",                   ""],
    ["手動最大速度",       "F7 段下無失步最高 hz 與對應 °/s",                   ""],
    ["靜止保持扭矩",       "IHOLD 模式下施力直至失步之 N·cm",                   ""],
    ["連續運轉溫升",       "持續 30 分鐘後 TMC2209 表面溫度 (°C)",              ""],
    ["軟限位有效性",       "撞限位後是否僅擋外衝方向、反向可退回",              ""],
  ],
}));
children.push(caption("表 6-2　預定量化指標與量測結果 (待實驗填入)"));

children.push(H2("6.3　結論與未來工作"));
children.push(body("本文完成以 STM32F446RE 為核心之雙軸太陽追蹤系統實作，提出以光敏電阻陣列為感測端、TMC2209 步進馬達為致動端之完整硬體與軟體框架。所採用之三段式比例控制搭配誤差飽和與軟限位，可於兼顧響應速度與穩定性之同時，防止極端姿態造成之機構損傷。"));
children.push(body("未來工作可自下列方向延伸："));
children.push(bullet("加入有限積分項消除穩態偏差，或採用增益自適應機制於不同光強條件下自動調整。"));
children.push(bullet("導入 Encoder 或 Hall 感測以提供真實位置回授，取代 hz × dt 之位置估算。"));
children.push(bullet("結合時間與地理資訊，建立日出日落預估之備援搜索策略，提升陰雨日之追蹤可用性。"));
children.push(bullet("啟用 TMC2209 之 CoolStep 與 StallGuard，以無感式撞限位偵測取代純軟限位。"));

/* =========================================================================
 *  建立 Document
 * ========================================================================= */

const doc = new Document({
  creator: "F446RE_TRACKING",
  title: "基於 STM32F446RE 與 TMC2209 之雙軸太陽追蹤系統",
  styles: {
    default: {
      document: { run: { font: FONT_CN, size: SIZE_BODY } },
    },
    paragraphStyles: [
      { id: "Heading1", name: "Heading 1", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { size: 32, bold: true, font: FONT_CN, color: "000000" },
        paragraph: { spacing: { before: 240, after: 240, line: LINE }, outlineLevel: 0 } },
      { id: "Heading2", name: "Heading 2", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { size: 28, bold: true, font: FONT_CN, color: "000000" },
        paragraph: { spacing: { before: 240, after: 120, line: LINE }, outlineLevel: 1 } },
      { id: "Heading3", name: "Heading 3", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { size: 26, bold: true, font: FONT_CN, color: "000000" },
        paragraph: { spacing: { before: 180, after: 100, line: LINE }, outlineLevel: 2 } },
    ],
  },
  numbering: {
    config: [
      { reference: "nums",
        levels: [
          { level: 0, format: LevelFormat.DECIMAL, text: "(%1)", alignment: AlignmentType.LEFT,
            style: { paragraph: { indent: { left: 720, hanging: 420 } } } },
        ] },
      { reference: "dots",
        levels: [
          { level: 0, format: LevelFormat.BULLET, text: "‧", alignment: AlignmentType.LEFT,
            style: { paragraph: { indent: { left: 540, hanging: 270 } } } },
        ] },
    ],
  },
  sections: [{ properties: sectionProps, children }],
});

Packer.toBuffer(doc).then(buf => {
  const out = path.join(__dirname, "F446RE_TRACKING_Paper_v3.docx");
  fs.writeFileSync(out, buf);
  console.log("OK:", out, buf.length, "bytes");
});
