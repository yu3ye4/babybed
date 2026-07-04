from pathlib import Path
import math

from PIL import Image, ImageDraw, ImageFont


BASE = Path(r"D:\RT-ThreadStudio\workspace\babybed")
OUTDIR = BASE / "output" / "figures_23"
OUTDIR.mkdir(parents=True, exist_ok=True)

W, H = 2200, 1300
BG = "white"
BLUE = "#2f80ed"
TEAL = "#2bb9a8"
GREEN = "#18a999"
NAVY = "#1f3c88"
DARK = "#20242a"
GRAY = "#667085"
RED = "#d94b45"
ORANGE = "#f28a2e"
LINE = "#d7e2ee"
SOFT_BLUE = "#f4f9ff"
SOFT_TEAL = "#f2fbf9"
SOFT_ORANGE = "#fff6ea"
SOFT_RED = "#fff2f0"
SOFT_GRAY = "#f8fafc"

FONT_PATHS = [
    r"C:\Windows\Fonts\msyh.ttc",
    r"C:\Windows\Fonts\simhei.ttf",
    r"C:\Windows\Fonts\simsun.ttc",
    r"C:\Windows\Fonts\arial.ttf",
]


def font(size):
    for p in FONT_PATHS:
        if Path(p).exists():
            try:
                return ImageFont.truetype(p, size=size)
            except Exception:
                pass
    return ImageFont.load_default()


F_TITLE = font(48)
F_SUB = font(22)
F_BIG = font(30)
F_MID = font(24)
F_SM = font(20)
F_XS = font(16)


def center_text(draw, xy, text, fnt, fill=DARK, spacing=6):
    x1, y1, x2, y2 = xy
    bbox = draw.multiline_textbbox((0, 0), text, font=fnt, spacing=spacing, align="center")
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    tx = x1 + (x2 - x1 - tw) / 2
    ty = y1 + (y2 - y1 - th) / 2 - 1
    draw.multiline_text((tx, ty), text, font=fnt, fill=fill, spacing=spacing, align="center")


def rounded_box(draw, xy, fill, outline=LINE, width=3, radius=24):
    draw.rounded_rectangle(xy, radius=radius, fill=fill, outline=outline, width=width)


def box(draw, xy, text, fill=SOFT_BLUE, outline=BLUE, text_fill=DARK, fnt=F_SM, width=3, radius=24):
    rounded_box(draw, xy, fill=fill, outline=outline, width=width, radius=radius)
    center_text(draw, xy, text, fnt, text_fill)


def arrow(draw, p1, p2, color=DARK, width=5, head=18):
    draw.line([p1, p2], fill=color, width=width)
    x1, y1 = p1
    x2, y2 = p2
    ang = math.atan2(y2 - y1, x2 - x1)
    a1 = (x2 - head * math.cos(ang - math.pi / 6), y2 - head * math.sin(ang - math.pi / 6))
    a2 = (x2 - head * math.cos(ang + math.pi / 6), y2 - head * math.sin(ang + math.pi / 6))
    draw.polygon([p2, a1, a2], fill=color)


def title_band(draw, title, subtitle=""):
    draw.text((92, 44), title, font=F_TITLE, fill=NAVY)
    draw.line((92, 118, W - 92, 118), fill=BLUE, width=4)
    draw.ellipse((74, 108, 96, 130), fill=BLUE)
    draw.ellipse((W - 98, 108, W - 76, 130), fill=BLUE)
    if subtitle:
        draw.text((96, 138), subtitle, font=F_SUB, fill=GRAY)


def save(img, filename):
    path = OUTDIR / filename
    img.save(path, "PNG", optimize=True)
    return path


def canvas(title, subtitle=""):
    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)
    title_band(d, title, subtitle)
    return img, d


def label_chip(draw, xy, text, fill=SOFT_GRAY, outline=LINE, text_fill=GRAY):
    rounded_box(draw, xy, fill=fill, outline=outline, width=2, radius=18)
    center_text(draw, xy, text, F_XS, text_fill, spacing=3)


def plain_label(draw, x, y, text, color):
    draw.text((x, y), text, font=font(21), fill=color)


def diamond(draw, xy, text, fill="white", outline=BLUE, text_fill=DARK, fnt=F_SM, width=3):
    x1, y1, x2, y2 = xy
    points = [
        ((x1 + x2) / 2, y1),
        (x2, (y1 + y2) / 2),
        ((x1 + x2) / 2, y2),
        (x1, (y1 + y2) / 2),
    ]
    draw.polygon(points, fill=fill, outline=outline)
    if width > 1:
        inset = width / 2
        inner = [
            ((x1 + x2) / 2, y1 + inset),
            (x2 - inset, (y1 + y2) / 2),
            ((x1 + x2) / 2, y2 - inset),
            (x1 + inset, (y1 + y2) / 2),
        ]
        draw.line(inner + [inner[0]], fill=outline, width=width)
    center_text(draw, (x1 + 14, y1 + 10, x2 - 14, y2 - 10), text, fnt, text_fill, spacing=4)


def fig1():
    img, d = canvas("\u56fe1 \u8f6f\u4ef6\u603b\u4f53\u67b6\u6784\u56fe")

    left = (85, 180, 790, 1185)
    mid = (900, 250, 1315, 1010)
    right = (1410, 180, 2115, 1185)
    rounded_box(d, left, SOFT_BLUE, outline="#97c2f6", width=3, radius=28)
    rounded_box(d, mid, SOFT_GRAY, outline="#b8c7d9", width=3, radius=28)
    rounded_box(d, right, SOFT_TEAL, outline="#92dbc8", width=3, radius=28)

    box(d, (240, 128, 640, 174), "设备端", fill="white", outline="white", text_fill=BLUE, fnt=font(34), width=0, radius=16)
    box(d, (935, 188, 1280, 238), "MQTT Broker", fill="white", outline="white", text_fill=GREEN, fnt=font(36), width=0, radius=16)
    box(d, (1545, 128, 1995, 174), "PC Web Console", fill="white", outline="white", text_fill=TEAL, fnt=font(34), width=0, radius=16)

    # device side: top control + two-stage smart pipeline
    box(d, (130, 230, 745, 315), "CM33 启动链路\n上电 / 拉起 CM55 / 共享内存", fill=SOFT_GRAY, outline=BLUE, fnt=font(24))
    box(d, (130, 335, 745, 420), "CM55 业务固件\nmain() / UI / WiFi / 日志", fill="white", outline=GREEN, fnt=font(24))

    # device-side four-layer smart stack
    input_nodes = [
        ((120, 450, 420, 535), "环境采集\nAHT20温湿度", SOFT_BLUE, BLUE),
        ((455, 450, 755, 535), "呼吸监测\nADS1115 / 滤波 / 统计", SOFT_TEAL, GREEN),
        ((120, 565, 420, 650), "视觉链路\nCamera / OpenCV预处理", "white", BLUE),
        ((455, 565, 755, 650), "哭声链路\nPDM Mic / 音频片段", SOFT_ORANGE, ORANGE),
    ]
    for xy, text, fill, outline in input_nodes:
        box(d, xy, text, fill=fill, outline=outline, fnt=font(22))

    box(d, (120, 680, 420, 770), "环境风险评估\nrisk / score / reason", fill="white", outline=BLUE, fnt=font(22))
    box(d, (455, 680, 755, 770), "Deep Craft视觉模型\nvision_posture / confidence / risk", fill=SOFT_TEAL, outline=GREEN, fnt=font(20))
    box(d, (120, 800, 420, 890), "Edge Impulse哭声模型\ncry event / confidence", fill=SOFT_ORANGE, outline=ORANGE, fnt=font(21))
    box(d, (455, 800, 755, 890), "多模态融合与告警\n环境 / 呼吸 / 视觉 / 哭声", fill=SOFT_RED, outline=RED, fnt=font(22))
    box(d, (130, 920, 745, 1010), "综合风险判断与MQTT发布\ntelemetry / breath / cry alert", fill="white", outline=TEAL, fnt=font(24))
    box(d, (130, 1035, 745, 1120), "IPC / Event / Log\n状态同步 / 命令缓冲 / 异常记录", fill=SOFT_GRAY, outline=LINE, fnt=font(21))

    # internal arrows
    arrow(d, (438, 315), (438, 335), color=GREEN, width=6)
    arrow(d, (210, 420), (210, 450), color=BLUE, width=5)
    arrow(d, (605, 420), (605, 450), color=GREEN, width=5)
    arrow(d, (210, 535), (210, 680), color=BLUE, width=5)
    arrow(d, (605, 535), (605, 800), color=ORANGE, width=5)
    arrow(d, (270, 650), (270, 680), color=BLUE, width=5)
    arrow(d, (605, 650), (605, 680), color=GREEN, width=5)
    arrow(d, (420, 725), (455, 725), color=GREEN, width=5)
    arrow(d, (420, 845), (455, 845), color=ORANGE, width=5)
    arrow(d, (605, 770), (605, 800), color=RED, width=5)
    arrow(d, (605, 890), (605, 920), color=RED, width=6)
    arrow(d, (438, 1010), (438, 1035), color=TEAL, width=5)

    # broker
    box(d, (945, 470, 1265, 610), "Broker\n1883\n订阅 / 转发 / 路由", fill="white", outline=GREEN, fnt=font(32), width=4)
    label_chip(d, (940, 690, 1270, 750), "数据转发", fill=SOFT_TEAL, outline="#99dacc", text_fill=GREEN)
    label_chip(d, (940, 785, 1270, 845), "命令路由", fill=SOFT_ORANGE, outline="#f2c48d", text_fill=ORANGE)

    # pc side
    box(d, (1470, 230, 2060, 320), "FastAPI 服务\n设备状态 / 指令接口", fill="white", outline=BLUE, fnt=font(24))
    box(d, (1470, 345, 2060, 435), "paho-mqtt 订阅\ntelemetry / alert / breath", fill=SOFT_BLUE, outline=TEAL, fnt=font(24))
    box(d, (1470, 460, 2060, 550), "历史记录 JSONL\n最近记录 / 回放", fill="white", outline=BLUE, fnt=font(24))
    box(d, (1470, 575, 2060, 680), "风险融合与状态整理\n环境 / 呼吸 / 视觉 / 哭声", fill=SOFT_TEAL, outline=GREEN, fnt=font(23))
    box(d, (1470, 715, 2060, 805), "Web UI 实时展示\n状态 / 风险 / 告警", fill="white", outline=BLUE, fnt=font(24))
    box(d, (1470, 830, 2060, 920), "辅助智能分析\n历史记录 / 分析建议", fill=SOFT_GRAY, outline=LINE, fnt=font(24))
    box(d, (1470, 960, 2060, 1050), "阈值命令下发\ncommand / threshold", fill=SOFT_ORANGE, outline=ORANGE, fnt=font(24))

    for p1, p2, c in [
        ((1765, 320), (1765, 345), BLUE),
        ((1765, 435), (1765, 460), TEAL),
        ((1765, 550), (1765, 575), BLUE),
        ((1765, 680), (1765, 715), GREEN),
        ((1765, 805), (1765, 830), BLUE),
        ((1765, 920), (1765, 960), ORANGE),
    ]:
        arrow(d, p1, p2, color=c, width=5)

    # cross-system arrows + labels
    arrow(d, (745, 965), (945, 555), color=TEAL, width=7)
    arrow(d, (1265, 545), (1470, 390), color=BLUE, width=6)
    arrow(d, (1265, 575), (1470, 620), color=GREEN, width=6)
    arrow(d, (1265, 600), (1470, 760), color=ORANGE, width=6)
    arrow(d, (1470, 1005), (1265, 790), color=ORANGE, width=6)


    return save(img, "fig1_architecture.png")
def fig2():
    img, d = canvas("图2 系统启动流程图", "从上电到 MQTT 就绪的完整依赖链，突出等待链路和失败分支")

    x, w = 250, 1020
    steps = [
        ("上电", "系统复位 / 时钟准备", BLUE, SOFT_BLUE),
        ("CM33 启动", "唤醒 CM55 / 建立共享内存", GREEN, SOFT_TEAL),
        ("CM55 main()", "进入业务主线程", BLUE, "white"),
        ("UI 初始化", "显示屏 / 交互资源加载", GREEN, SOFT_TEAL),
        ("WiFi 等待与连接", "轮询链路 / 连接路由器", ORANGE, SOFT_ORANGE),
        ("日志初始化", "打开运行日志 / 异常追踪", BLUE, SOFT_GRAY),
        ("小智 WebSocket", "等待语音服务链路", GREEN, SOFT_TEAL),
        ("AHT20 初始化", "温湿度传感器探测", BLUE, SOFT_BLUE),
        ("ADS1115 初始化", "呼吸采样通道就绪", GREEN, SOFT_TEAL),
        ("IPC / Event 初始化", "共享状态 / 事件队列", BLUE, "white"),
        ("告警初始化", "状态锁 / 信号量 / LED 线程", ORANGE, SOFT_ORANGE),
        ("MQTT 初始化", "连接 Broker / 订阅 command", GREEN, SOFT_TEAL),
    ]
    y = 190
    gap = 18
    for i, (t1, t2, c, fill) in enumerate(steps):
        h = 64 if i < 2 else 70
        box(d, (x, y, x + w, y + h), f"{t1}\n{t2}", fill=fill, outline=c, fnt=F_MID if i < 2 else F_SM, width=3)
        if i < len(steps) - 1:
            arrow(d, (x + w / 2, y + h), (x + w / 2, y + h + gap), color=c, width=5)
        y += h + gap

    # right-side state cards
    box(d, (1470, 250, 2070, 340), "等待链路\nWiFi / WebSocket / Broker", fill=SOFT_ORANGE, outline=ORANGE, fnt=F_SM)
    box(d, (1470, 380, 2070, 470), "失败分支\n记录日志并重试 / 退出", fill=SOFT_RED, outline=RED, fnt=F_SM)
    box(d, (1470, 510, 2070, 600), "初始化完成后进入循环\n采集 / 判断 / 上报", fill=SOFT_GRAY, outline=LINE, fnt=F_SM)
    arrow(d, (1270, 505), (1470, 295), color=ORANGE, width=5)
    arrow(d, (1270, 835), (1470, 555), color=BLUE, width=5)
    label_chip(d, (1470, 660, 2070, 710), "关键依赖：网络先就绪，业务后发布", fill="white", outline=LINE, text_fill=GRAY)

    return save(img, "fig2_boot_flow.png")


def fig3():
    img, d = canvas(
        "\u56fe3 \u73af\u5883\u76d1\u6d4b\u4e0e\u98ce\u9669\u8bc4\u4f30\u6d41\u7a0b\u56fe",
        "\u5c55\u793a\u6e29\u6e7f\u5ea6\u91c7\u6837\u3001\u9608\u503c\u5224\u65ad\u3001\u98ce\u9669\u751f\u6210\u4e0e\u591a\u6a21\u6001\u9884\u8b66\u8854\u63a5",
    )

    main = [
        (90, 230, 360, 340, "AHT20\u91c7\u6837\n\u6e29\u5ea6 / \u6e7f\u5ea6", BLUE, SOFT_BLUE),
        (420, 230, 700, 340, "\u6570\u636e\u6821\u9a8c\n\u5f02\u5e38\u503c\u5254\u9664", GREEN, SOFT_TEAL),
        (760, 230, 1040, 340, "\u5355\u4f4d\u6362\u7b97\ncenti -> \xb0C / %", BLUE, "white"),
        (1100, 230, 1380, 340, "\u9608\u503c\u6bd4\u8f83\n\u6e29\u5ea6 / \u6e7f\u5ea6", GREEN, SOFT_TEAL),
        (1440, 230, 1720, 340, "\u98ce\u9669\u8bc4\u5206\nscore / level", ORANGE, SOFT_ORANGE),
        (1780, 230, 2090, 340, "\u73af\u5883\u98ce\u9669\u7ed3\u679c\nrisk / score / reason", BLUE, "white"),
    ]
    for i, (x1, y1, x2, y2, text, out, fill) in enumerate(main):
        box(d, (x1, y1, x2, y2), text, fill=fill, outline=out, fnt=font(24))
        if i < len(main) - 1:
            arrow(d, (x2, 285), (main[i + 1][0], 285), color=out, width=5)

    box(d, (250, 450, 700, 580), "\u6e29\u5ea6\u5224\u65ad\n\u8fc7\u9ad8 / \u8fc7\u4f4e", fill=SOFT_ORANGE, outline=ORANGE, fnt=font(26))
    box(d, (820, 450, 1270, 580), "\u6e7f\u5ea6\u5224\u65ad\n\u8fc7\u9ad8 / \u8fc7\u4f4e", fill=SOFT_TEAL, outline=GREEN, fnt=font(26))
    box(d, (1390, 450, 1870, 580), "\u6f6e\u6e7f\u8d8b\u52bf\n\u504f\u6e7f / \u6b63\u5e38", fill=SOFT_RED, outline=RED, fnt=font(26))
    arrow(d, (1240, 340), (1240, 450), color=GREEN, width=5)
    arrow(d, (1580, 340), (1580, 450), color=ORANGE, width=5)

    bottom = [
        (170, 805, 600, 920, "Telemetry\u4e0a\u62a5\nMQTT publish", BLUE, SOFT_BLUE),
        (710, 805, 1180, 920, "PC\u7aef\u5c55\u793a\nrisk / score / reason", GREEN, SOFT_TEAL),
        (1290, 805, 2030, 920, "\u53c2\u4e0e\u591a\u6a21\u6001\u7efc\u5408\u9884\u8b66\n\u73af\u5883\u98ce\u9669 + \u89c6\u89c9 + \u54ed\u58f0 + \u547c\u5438", ORANGE, SOFT_ORANGE),
    ]
    for i, (x1, y1, x2, y2, text, out, fill) in enumerate(bottom):
        box(d, (x1, y1, x2, y2), text, fill=fill, outline=out, fnt=font(25))
        if i < len(bottom) - 1:
            arrow(d, (x2, 862), (bottom[i + 1][0], 862), color=out, width=5)

    arrow(d, (1935, 340), (1935, 805), color=BLUE, width=6)
    return save(img, "fig3_env_risk.png")
def fig4():
    img, d = canvas(
        "\u56fe4 \u547c\u5438\u4fe1\u53f7\u91c7\u96c6\u4e0e\u5206\u6790\u6d41\u7a0b\u56fe",
        "\u5c55\u793a\u547c\u5438\u91c7\u6837\u3001\u6ee4\u6ce2\u5904\u7406\u3001\u7279\u5f81\u8ba1\u7b97\u4e0e\u72b6\u6001\u5224\u65ad\u7684\u5b8c\u6574\u5206\u6790\u94fe",
    )

    # main horizontal flow
    chain = [
        (70, 220, 360, 360, "ADS1115\u91c7\u6837\napp_ads1115_read_a0_mv()", BLUE, SOFT_BLUE),
        (410, 220, 710, 360, "\u6297\u566a\u6ee4\u6ce2\n\u6ed1\u52a8\u5e73\u5747 / \u6291\u5236\u6296\u52a8", GREEN, SOFT_TEAL),
        (760, 220, 1050, 360, "\u57fa\u7ebf\u4fee\u6b63\n\u6f02\u79fb\u8865\u507f", BLUE, "white"),
        (1100, 220, 1400, 360, "\u73af\u5f62\u7f13\u51b2\u5199\u5165\n\u4fdd\u7559\u5386\u53f2\u7a97\u53e3", GREEN, SOFT_TEAL),
        (1450, 220, 1750, 360, "\u7edf\u8ba1\u8ba1\u7b97\n\u5cf0\u503c / \u80fd\u91cf / \u9891\u7387", BLUE, SOFT_BLUE),
        (1800, 220, 2100, 360, "\u72b6\u6001\u5224\u65ad\n\u6b63\u5e38 / \u5f31\u547c\u5438 / \u7591\u4f3c\u6682\u505c", ORANGE, SOFT_ORANGE),
    ]
    for i, (x1, y1, x2, y2, text, out, fill) in enumerate(chain):
        box(d, (x1, y1, x2, y2), text, fill=fill, outline=out, fnt=font(26))
        if i < len(chain) - 1:
            arrow(d, (x2, 290), (chain[i + 1][0], 290), color=out, width=6)

    # centered analysis/result row
    analysis = [
        (120, 470, 600, 635, "\u539f\u59cb\u6ce2\u5f62\n\u4fdd\u7559\u8f93\u5165\u7ec6\u8282", BLUE, SOFT_GRAY),
        (670, 470, 1210, 635, "\u6ee4\u6ce2\u6ce2\u5f62\n\u566a\u58f0\u6291\u5236\u540e\u8f93\u51fa", GREEN, SOFT_TEAL),
        (1280, 470, 1730, 635, "\u7edf\u8ba1\u91cf\n\u5cf0\u503c / \u5747\u503c / \u632f\u5e45", BLUE, SOFT_BLUE),
        (1800, 470, 2100, 635, "\u547c\u5438\u72b6\u6001\n\u72b6\u6001\u6807\u7b7e", ORANGE, SOFT_ORANGE),
    ]
    for x1, y1, x2, y2, text, out, fill in analysis:
        box(d, (x1, y1, x2, y2), text, fill=fill, outline=out, fnt=font(29))

    arrow(d, (1600, 360), (1600, 470), color=BLUE, width=6)
    arrow(d, (1950, 360), (1950, 470), color=ORANGE, width=6)

    # concise criteria row
    criteria = [
        (150, 730, 620, 835, "\u91c7\u6837\u7a97\u53e3\n\u56fa\u5b9a\u65f6\u95f4\u7a97 / \u8fde\u7eed\u5237\u65b0", BLUE, SOFT_BLUE),
        (710, 730, 1220, 835, "\u5224\u5b9a\u4f9d\u636e\n\u5e45\u503c / \u8282\u5f8b / \u6301\u7eed\u65f6\u95f4", GREEN, SOFT_TEAL),
        (1310, 730, 2100, 835, "\u72b6\u6001\u793a\u4f8b\n\u4f4e\u547c\u5438\uff1a\u5cf0\u503c\u504f\u5f31  \u3000\u7591\u4f3c\u6682\u505c\uff1a\u957f\u65f6\u95f4\u65e0\u6709\u6548\u8d77\u4f0f", ORANGE, SOFT_ORANGE),
    ]
    for x1, y1, x2, y2, text, out, fill in criteria:
        box(d, (x1, y1, x2, y2), text, fill=fill, outline=out, fnt=font(24))

    # bottom outputs row
    outputs = [
        (180, 950, 610, 1085, "\u8f93\u51fa1\n\u539f\u59cb\u6ce2\u5f62", BLUE, "white"),
        (690, 950, 1120, 1085, "\u8f93\u51fa2\n\u6ee4\u6ce2\u6ce2\u5f62", GREEN, "white"),
        (1200, 950, 1630, 1085, "\u8f93\u51fa3\n\u7edf\u8ba1\u91cf", BLUE, "white"),
        (1710, 950, 2090, 1085, "\u8f93\u51fa4\n\u547c\u5438\u72b6\u6001", ORANGE, "white"),
    ]
    for x1, y1, x2, y2, text, out, fill in outputs:
        box(d, (x1, y1, x2, y2), text, fill=fill, outline=out, fnt=font(27))
    return save(img, "fig4_breath_flow.png")
def fig5():
    img, d = canvas("图5 哭声告警流程图", "完整展示识别、置信度判断、冷却期、开始/停止告警和消息发布")

    left = [
        (120, 220, 430, 310, "识别结果输入\napp_alert_baby_cry_start()", BLUE, SOFT_BLUE),
        (120, 350, 430, 440, "置信度判断\ncrying score >= 阈值", GREEN, SOFT_TEAL),
        (120, 480, 430, 570, "冷却期判断\n避免重复触发", BLUE, "white"),
    ]
    for i, (x1, y1, x2, y2, text, out, fill) in enumerate(left):
        box(d, (x1, y1, x2, y2), text, fill=fill, outline=out, fnt=F_SM)
        if i < len(left) - 1:
            arrow(d, (275, y2), (275, left[i + 1][1]), color=out, width=5)

    box(d, (560, 315, 1020, 445), "开始告警\n写入 crying 状态\n唤醒 LED 闪烁线程", fill=SOFT_ORANGE, outline=ORANGE, fnt=F_SM)
    box(d, (1120, 315, 1600, 445), "MQTT 告警消息\n上报 baby_cry / level / time", fill=SOFT_RED, outline=RED, fnt=F_SM)
    box(d, (1680, 315, 2080, 445), "告警保持\n本地闪灯 + 状态锁存", fill=SOFT_TEAL, outline=GREEN, fnt=F_SM)
    arrow(d, (430, 390), (560, 390), color=GREEN, width=5)
    arrow(d, (1020, 380), (1120, 380), color=ORANGE, width=5)
    arrow(d, (1600, 380), (1680, 380), color=RED, width=5)

    box(d, (560, 560, 1020, 690), "停止告警\napp_alert_baby_cry_stop()", fill=SOFT_BLUE, outline=BLUE, fnt=F_SM)
    box(d, (1120, 560, 1600, 690), "清除状态\n关闭闪烁 / 释放冷却", fill="white", outline=LINE, fnt=F_SM)
    box(d, (1680, 560, 2080, 690), "静默分支\n条件不满足则保持安静", fill=SOFT_GRAY, outline=LINE, fnt=F_SM)
    arrow(d, (790, 445), (790, 560), color=BLUE, width=5)
    arrow(d, (1360, 445), (1360, 560), color=GREEN, width=5)
    arrow(d, (1880, 445), (1880, 560), color=BLUE, width=5)

    label_chip(d, (120, 760, 520, 820), "开始告警分支", fill=SOFT_ORANGE, outline="#f0cfa4", text_fill=ORANGE)
    label_chip(d, (570, 760, 970, 820), "停止告警分支", fill=SOFT_BLUE, outline="#9dc3f8", text_fill=BLUE)
    label_chip(d, (1020, 760, 1450, 820), "冷却期内重复识别将被抑制", fill=SOFT_GRAY, outline=LINE, text_fill=GRAY)
    label_chip(d, (1500, 760, 2080, 820), "告警链路 = 本地反馈 + MQTT 上报", fill=SOFT_TEAL, outline="#9adfd2", text_fill=GREEN)

    box(d, (180, 900, 610, 1030), "状态写入\nmutex 保护", fill="white", outline=BLUE, fnt=F_SM)
    box(d, (710, 900, 1140, 1030), "线程唤醒\nsemaphore 触发", fill="white", outline=GREEN, fnt=F_SM)
    box(d, (1250, 900, 1680, 1030), "LED 闪烁\n节奏控制", fill="white", outline=ORANGE, fnt=F_SM)
    box(d, (1790, 900, 2080, 1030), "告警结束\n恢复待机", fill="white", outline=LINE, fnt=F_SM)
    arrow(d, (610, 965), (710, 965), color=BLUE, width=5)
    arrow(d, (1140, 965), (1250, 965), color=GREEN, width=5)
    arrow(d, (1680, 965), (1790, 965), color=ORANGE, width=5)
    return save(img, "fig5_cry_alert.png")


def fig5():
    img, d = canvas("ͼ5 �����澯����ͼ", "չʾ����ʶ��ģ�����Ρ���ֵ�жϡ���ȴ�����ƺ��澯ִ�л���")

    top_y1, top_y2 = 220, 340
    step_w, step_gap = 225, 32
    start_x = 60
    top_boxes = [
        ("PDM����ɼ�\n�����ɼ���Ƶ", BLUE, SOFT_BLUE),
        ("��ƵƬ�λ���\n������ / ����֡", GREEN, SOFT_TEAL),
        ("Edge Impulse\n������Ƶ����", ORANGE, SOFT_ORANGE),
        ("�������\ncrying / noise", BLUE, "white"),
    ]
    top_rects = []
    x = start_x
    for text, outline, fill in top_boxes:
        rect = (x, top_y1, x + step_w, top_y2)
        top_rects.append((rect, outline))
        box(d, rect, text, fill=fill, outline=outline, fnt=font(25))
        x += step_w + step_gap

    for i in range(len(top_rects) - 1):
        arrow(d, (top_rects[i][0][2], 280), (top_rects[i + 1][0][0], 280), color=top_rects[i][1], width=5)

    cry_diamond = (1070, 210, 1270, 350)
    diamond(d, cry_diamond, "crying?", fill="white", outline=BLUE, fnt=font(29), width=4)
    arrow(d, (top_rects[-1][0][2], 280), (cry_diamond[0], 280), color=BLUE, width=5)

    start_box = (1360, 220, 1655, 340)
    box(d, start_box, "��ʼ��ֵ�ж�\nconfidence >= 0.80", fill=SOFT_TEAL, outline=GREEN, fnt=font(24))
    arrow(d, (cry_diamond[2], 280), (start_box[0], 280), color=GREEN, width=5)

    stop_box = (1745, 220, 2040, 340)
    box(d, stop_box, "ֹͣ��ֵ�ж�\nconfidence < 0.55", fill="white", outline=BLUE, fnt=font(24))
    arrow(d, (start_box[2], 280), (stop_box[0], 280), color=BLUE, width=5)

    window_box = (1360, 395, 1655, 515)
    box(d, window_box, "����ȷ��\n����4������", fill=SOFT_BLUE, outline=BLUE, fnt=font(24))
    arrow(d, ((stop_box[0] + stop_box[2]) / 2, stop_box[3]), ((window_box[0] + window_box[2]) / 2, window_box[1]), color=BLUE, width=5)

    cooldown_diamond = (1740, 385, 2050, 525)
    diamond(d, cooldown_diamond, "��ȴ����\n60 s �����ظ��澯", fill=SOFT_ORANGE, outline=ORANGE, fnt=font(24), width=4)
    arrow(d, (window_box[2], 455), (cooldown_diamond[0], 455), color=ORANGE, width=5)

    tag_font = font(20)
    d.text((1302, 248), "��", font=tag_font, fill=GREEN)
    d.text((1690, 248), "��", font=tag_font, fill=BLUE)
    d.text((1198, 184), "��", font=tag_font, fill=BLUE)
    d.text((1492, 360), "��", font=tag_font, fill=BLUE)

    start_chain = [
        ((95, 620, 455, 735), "app_alert_baby_cry_start()", ORANGE, SOFT_ORANGE),
        ((505, 620, 865, 735), "״̬����\nmutex ����", BLUE, "white"),
        ((915, 620, 1275, 735), "���� LED �߳�\nsemaphore �ͷ�", GREEN, SOFT_TEAL),
        ((1325, 620, 1685, 735), "LED ��˸�澯", ORANGE, SOFT_ORANGE),
        ((1735, 620, 2095, 735), "MQTT �澯�ϱ�\nbaby_cry / level / time", RED, SOFT_RED),
    ]
    for i, (rect, text, outline, fill) in enumerate(start_chain):
        box(d, rect, text, fill=fill, outline=outline, fnt=font(24))
        if i < len(start_chain) - 1:
            arrow(d, (rect[2], 678), (start_chain[i + 1][0][0], 678), color=outline, width=5)

    stop_chain = [
        ((360, 900, 725, 1015), "app_alert_baby_cry_stop()", BLUE, SOFT_BLUE),
        ((805, 900, 1170, 1015), "����澯״̬", BLUE, "white"),
        ((1250, 900, 1615, 1015), "ֹͣ LED ��˸", GREEN, SOFT_TEAL),
        ((1695, 900, 2060, 1015), "�ָ���Ĭ���", ORANGE, SOFT_GRAY),
    ]
    for i, (rect, text, outline, fill) in enumerate(stop_chain):
        box(d, rect, text, fill=fill, outline=outline, fnt=font(24))
        if i < len(stop_chain) - 1:
            arrow(d, (rect[2], 958), (stop_chain[i + 1][0][0], 958), color=outline, width=5)

    arrow(d, ((start_box[0] + start_box[2]) / 2, start_box[3]), ((start_chain[0][0][0] + start_chain[0][0][2]) / 2, start_chain[0][0][1]), color=ORANGE, width=6)
    arrow(d, ((cooldown_diamond[0] + cooldown_diamond[2]) / 2, cooldown_diamond[3]), ((start_chain[-1][0][0] + start_chain[-1][0][2]) / 2, start_chain[-1][0][1]), color=RED, width=6)
    arrow(d, ((window_box[0] + window_box[2]) / 2, window_box[3]), ((stop_chain[0][0][0] + stop_chain[0][0][2]) / 2, stop_chain[0][0][1]), color=BLUE, width=6)

    box(d, (95, 785, 770, 860), "��ʼ�澯֧·����ģ������ﵽ 0.80 �������䣬������״̬�����ͱ��ؼ�Զ�̸���", fill=SOFT_ORANGE, outline=ORANGE, fnt=font(20))
    box(d, (815, 785, 1510, 860), "ֹͣ�澯֧·��������Ŷȵ��� 0.55 ���£��������� 4 �����ڣ���ִ��ֹͣ����", fill=SOFT_BLUE, outline=BLUE, fnt=font(20))
    box(d, (1555, 785, 2095, 860), "��ȴ���ƣ�APP_ALERT_CRY_COOLDOWN_MS = 60000�������ظ��澯", fill=SOFT_RED, outline=RED, fnt=font(20))
    return save(img, "fig5_cry_alert.png")


def fig5():
    img, d = canvas(
        "\u56fe5 \u54ed\u58f0\u544a\u8b66\u6d41\u7a0b\u56fe",
        "\u5c55\u793a\u97f3\u9891\u91c7\u6837\u3001\u6a21\u578b\u63a8\u7406\u3001\u9608\u503c\u5224\u5b9a\u3001\u51b7\u5374\u6291\u5236\u4e0e\u544a\u8b66\u6267\u884c\u95ed\u73af",
    )

    top_y1, top_y2 = 220, 340
    step_w, step_gap = 225, 32
    start_x = 60
    top_boxes = [
        ("PDM\u9ea6\u514b\u98ce\u91c7\u6837\n\u8fde\u7eed\u91c7\u96c6\u97f3\u9891", BLUE, SOFT_BLUE),
        ("\u97f3\u9891\u7247\u6bb5\u7f13\u5b58\n\u6ed1\u52a8\u7a97\u53e3 / \u7279\u5f81\u5e27", GREEN, SOFT_TEAL),
        ("Edge Impulse\u6a21\u578b\u63a8\u7406\n\u7aef\u4fa7\u97f3\u9891\u5206\u7c7b", ORANGE, SOFT_ORANGE),
        ("\u5206\u7c7b\u7ed3\u679c\u89e3\u6790\ncrying / noise", BLUE, "white"),
    ]
    top_rects = []
    x = start_x
    for text, outline, fill in top_boxes:
        rect = (x, top_y1, x + step_w, top_y2)
        top_rects.append((rect, outline))
        box(d, rect, text, fill=fill, outline=outline, fnt=font(24))
        x += step_w + step_gap

    for i in range(len(top_rects) - 1):
        arrow(d, (top_rects[i][0][2], 280), (top_rects[i + 1][0][0], 280), color=top_rects[i][1], width=5)

    cry_diamond = (1070, 210, 1270, 350)
    diamond(d, cry_diamond, "crying?", fill="white", outline=BLUE, fnt=font(28), width=4)
    arrow(d, (top_rects[-1][0][2], 280), (cry_diamond[0], 280), color=BLUE, width=5)

    start_box = (1360, 220, 1655, 340)
    box(d, start_box, "\u5f00\u59cb\u9608\u503c\u5224\u65ad\nconfidence >= 0.80", fill=SOFT_TEAL, outline=GREEN, fnt=font(24))
    arrow(d, (cry_diamond[2], 280), (start_box[0], 280), color=GREEN, width=5)

    stop_box = (1745, 220, 2040, 340)
    box(d, stop_box, "\u505c\u6b62\u9608\u503c\u5224\u65ad\nconfidence < 0.55", fill="white", outline=BLUE, fnt=font(24))
    arrow(d, (start_box[2], 280), (stop_box[0], 280), color=BLUE, width=5)

    window_box = (1360, 395, 1655, 515)
    box(d, window_box, "\u8fde\u7eed\u7a97\u53e3\u786e\u8ba4\n\u8fde\u7eed4\u4e2a\u7a97\u53e3", fill=SOFT_BLUE, outline=BLUE, fnt=font(24))
    arrow(d, ((stop_box[0] + stop_box[2]) / 2, stop_box[3]), ((window_box[0] + window_box[2]) / 2, window_box[1]), color=BLUE, width=5)

    cooldown_diamond = (1740, 385, 2050, 525)
    diamond(d, cooldown_diamond, "\u51b7\u5374\u673a\u5236\u5224\u65ad\n60 s\u6291\u5236\u91cd\u590d\u544a\u8b66", fill=SOFT_ORANGE, outline=ORANGE, fnt=font(24), width=4)
    arrow(d, (window_box[2], 455), (cooldown_diamond[0], 455), color=ORANGE, width=5)

    tag_font = font(20)
    d.text((1302, 248), "\u662f", font=tag_font, fill=GREEN)
    d.text((1690, 248), "\u5426", font=tag_font, fill=BLUE)
    d.text((1198, 184), "\u662f", font=tag_font, fill=BLUE)
    d.text((1492, 360), "\u662f", font=tag_font, fill=BLUE)

    start_chain = [
        ((95, 620, 455, 735), "app_alert_baby_cry_start()", ORANGE, SOFT_ORANGE),
        ((505, 620, 865, 735), "\u72b6\u6001\u9501\u5b58\nmutex\u4fdd\u62a4", BLUE, "white"),
        ((915, 620, 1275, 735), "\u5524\u9192LED\u7ebf\u7a0b\nsemaphore\u91ca\u653e", GREEN, SOFT_TEAL),
        ((1325, 620, 1685, 735), "LED\u95ea\u70c1\u544a\u8b66", ORANGE, SOFT_ORANGE),
        ((1735, 620, 2095, 735), "MQTT\u544a\u8b66\u4e0a\u62a5\nbaby_cry / level / time", RED, SOFT_RED),
    ]
    for i, (rect, text, outline, fill) in enumerate(start_chain):
        box(d, rect, text, fill=fill, outline=outline, fnt=font(24))
        if i < len(start_chain) - 1:
            arrow(d, (rect[2], 678), (start_chain[i + 1][0][0], 678), color=outline, width=5)

    stop_chain = [
        ((360, 900, 725, 1015), "app_alert_baby_cry_stop()", BLUE, SOFT_BLUE),
        ((805, 900, 1170, 1015), "\u6e05\u9664\u544a\u8b66\u72b6\u6001", BLUE, "white"),
        ((1250, 900, 1615, 1015), "\u505c\u6b62LED\u95ea\u70c1", GREEN, SOFT_TEAL),
        ((1695, 900, 2060, 1015), "\u6062\u590d\u9759\u9ed8\u76d1\u6d4b", ORANGE, SOFT_GRAY),
    ]
    for i, (rect, text, outline, fill) in enumerate(stop_chain):
        box(d, rect, text, fill=fill, outline=outline, fnt=font(24))
        if i < len(stop_chain) - 1:
            arrow(d, (rect[2], 958), (stop_chain[i + 1][0][0], 958), color=outline, width=5)

    arrow(d, ((start_box[0] + start_box[2]) / 2, start_box[3]), ((start_chain[0][0][0] + start_chain[0][0][2]) / 2, start_chain[0][0][1]), color=ORANGE, width=6)
    arrow(d, ((cooldown_diamond[0] + cooldown_diamond[2]) / 2, cooldown_diamond[3]), ((start_chain[-1][0][0] + start_chain[-1][0][2]) / 2, start_chain[-1][0][1]), color=RED, width=6)
    arrow(d, ((window_box[0] + window_box[2]) / 2, window_box[3]), ((stop_chain[0][0][0] + stop_chain[0][0][2]) / 2, stop_chain[0][0][1]), color=BLUE, width=6)

    box(d, (95, 785, 770, 860), "\u5f00\u59cb\u544a\u8b66\u652f\u8def\uff1a\u6a21\u578b\u7f6e\u4fe1\u5ea6\u8fbe\u52300.80\u540e\u89e6\u53d1\u544a\u8b66\uff0c\u6267\u884c\u72b6\u6001\u9501\u5b58\u548c\u672c\u5730\u53cd\u9988", fill=SOFT_ORANGE, outline=ORANGE, fnt=font(20))
    box(d, (815, 785, 1510, 860), "\u505c\u6b62\u544a\u8b66\u652f\u8def\uff1a\u7f6e\u4fe1\u5ea6\u4f4e\u4e8e0.55\u4e14\u8fde\u7eed4\u4e2a\u7a97\u53e3\u6ee1\u8db3\u6761\u4ef6\u540e\uff0c\u6267\u884c\u505c\u6b62\u544a\u8b66", fill=SOFT_BLUE, outline=BLUE, fnt=font(20))
    box(d, (1555, 785, 2095, 860), "\u51b7\u5374\u6291\u5236\uff1aAPP_ALERT_CRY_COOLDOWN_MS = 60000\uff0c\u7528\u4e8e\u907f\u514d\u77ed\u65f6\u95f4\u5185\u91cd\u590d\u544a\u8b66", fill=SOFT_RED, outline=RED, fnt=font(20))
    return save(img, "fig5_cry_alert.png")


def fig5():
    img, d = canvas(
        "\u56fe5 \u54ed\u58f0\u544a\u8b66\u6d41\u7a0b\u56fe",
        "\u5c55\u793a\u97f3\u9891\u91c7\u6837\u3001\u6a21\u578b\u63a8\u7406\u3001\u9608\u503c\u5224\u5b9a\u3001\u51b7\u5374\u6291\u5236\u4e0e\u544a\u8b66\u6267\u884c\u95ed\u73af",
    )

    top_y1, top_y2 = 220, 340
    step_w, step_gap = 225, 32
    start_x = 60
    top_boxes = [
        ("PDM\u9ea6\u514b\u98ce\u91c7\u6837\n\u8fde\u7eed\u91c7\u96c6\u97f3\u9891", BLUE, SOFT_BLUE),
        ("\u97f3\u9891\u7247\u6bb5\u7f13\u5b58\n\u6ed1\u52a8\u7a97\u53e3 / \u7279\u5f81\u5e27", GREEN, SOFT_TEAL),
        ("Edge Impulse\u6a21\u578b\u63a8\u7406\n\u7aef\u4fa7\u97f3\u9891\u5206\u7c7b", ORANGE, SOFT_ORANGE),
        ("\u5206\u7c7b\u7ed3\u679c\u89e3\u6790\ncrying / noise", BLUE, "white"),
    ]
    top_rects = []
    x = start_x
    for text, outline, fill in top_boxes:
        rect = (x, top_y1, x + step_w, top_y2)
        top_rects.append((rect, outline))
        box(d, rect, text, fill=fill, outline=outline, fnt=font(25))
        x += step_w + step_gap

    for i in range(len(top_rects) - 1):
        arrow(d, (top_rects[i][0][2], 280), (top_rects[i + 1][0][0], 280), color=top_rects[i][1], width=5)

    cry_diamond = (1070, 210, 1270, 350)
    diamond(d, cry_diamond, "crying?", fill="white", outline=BLUE, fnt=font(29), width=4)
    arrow(d, (top_rects[-1][0][2], 280), (cry_diamond[0], 280), color=BLUE, width=5)

    start_box = (1360, 220, 1655, 340)
    box(d, start_box, "\u5f00\u59cb\u9608\u503c\u5224\u65ad\nconfidence >= 0.80", fill=SOFT_TEAL, outline=GREEN, fnt=font(25))
    arrow(d, (cry_diamond[2], 280), (start_box[0], 280), color=GREEN, width=5)

    stop_box = (1745, 220, 2040, 340)
    box(d, stop_box, "\u505c\u6b62\u9608\u503c\u5224\u65ad\nconfidence < 0.55", fill="white", outline=BLUE, fnt=font(25))
    arrow(d, (start_box[2], 280), (stop_box[0], 280), color=BLUE, width=5)

    window_box = (1360, 395, 1655, 515)
    box(d, window_box, "\u8fde\u7eed\u7a97\u53e3\u786e\u8ba4\n\u8fde\u7eed4\u4e2a\u7a97\u53e3", fill=SOFT_BLUE, outline=BLUE, fnt=font(25))
    arrow(d, ((stop_box[0] + stop_box[2]) / 2, stop_box[3]), ((window_box[0] + window_box[2]) / 2, window_box[1]), color=BLUE, width=5)

    cooldown_diamond = (1740, 385, 2050, 525)
    diamond(d, cooldown_diamond, "\u51b7\u5374\u673a\u5236\u5224\u65ad\n60 s\u6291\u5236\u91cd\u590d\u544a\u8b66", fill=SOFT_ORANGE, outline=ORANGE, fnt=font(25), width=4)
    arrow(d, (window_box[2], 455), (cooldown_diamond[0], 455), color=ORANGE, width=5)

    tag_font = font(29)
    d.text((1298, 238), "\u662f", font=tag_font, fill=GREEN)
    d.text((1686, 238), "\u5426", font=tag_font, fill=BLUE)
    d.text((1194, 172), "\u662f", font=tag_font, fill=BLUE)
    d.text((1488, 348), "\u662f", font=tag_font, fill=BLUE)

    start_chain = [
        ((95, 620, 455, 735), "app_alert_baby_cry_start()", ORANGE, SOFT_ORANGE),
        ((505, 620, 865, 735), "\u72b6\u6001\u9501\u5b58\nmutex\u4fdd\u62a4", BLUE, "white"),
        ((915, 620, 1275, 735), "\u5524\u9192LED\u7ebf\u7a0b\nsemaphore\u91ca\u653e", GREEN, SOFT_TEAL),
        ((1325, 620, 1685, 735), "LED\u95ea\u70c1\u544a\u8b66", ORANGE, SOFT_ORANGE),
        ((1735, 620, 2095, 735), "MQTT\u544a\u8b66\u4e0a\u62a5\nbaby_cry / level / time", RED, SOFT_RED),
    ]
    for i, (rect, text, outline, fill) in enumerate(start_chain):
        box(d, rect, text, fill=fill, outline=outline, fnt=font(25))
        if i < len(start_chain) - 1:
            arrow(d, (rect[2], 678), (start_chain[i + 1][0][0], 678), color=outline, width=5)

    stop_chain = [
        ((360, 870, 725, 985), "app_alert_baby_cry_stop()", BLUE, SOFT_BLUE),
        ((805, 870, 1170, 985), "\u6e05\u9664\u544a\u8b66\u72b6\u6001", BLUE, "white"),
        ((1250, 870, 1615, 985), "\u505c\u6b62LED\u95ea\u70c1", GREEN, SOFT_TEAL),
        ((1695, 870, 2060, 985), "\u6062\u590d\u9759\u9ed8\u76d1\u6d4b", ORANGE, SOFT_GRAY),
    ]
    for i, (rect, text, outline, fill) in enumerate(stop_chain):
        box(d, rect, text, fill=fill, outline=outline, fnt=font(25))
        if i < len(stop_chain) - 1:
            arrow(d, (rect[2], 928), (stop_chain[i + 1][0][0], 928), color=outline, width=5)

    arrow(d, ((start_box[0] + start_box[2]) / 2, start_box[3]), ((start_chain[0][0][0] + start_chain[0][0][2]) / 2, start_chain[0][0][1]), color=ORANGE, width=6)
    arrow(d, ((cooldown_diamond[0] + cooldown_diamond[2]) / 2, cooldown_diamond[3]), ((start_chain[-1][0][0] + start_chain[-1][0][2]) / 2, start_chain[-1][0][1]), color=RED, width=6)
    d.line([((window_box[0] + window_box[2]) / 2, window_box[3]), (1085, 760)], fill=BLUE, width=6)
    d.line([(1085, 760), (1085, 805)], fill=BLUE, width=6)
    d.line([(1085, 805), (540, 805)], fill=BLUE, width=6)
    arrow(d, (540, 805), (540, 870), color=BLUE, width=6)
    return save(img, "fig5_cry_alert.png")
def fig6():
    img, d = canvas("图6 阈值命令下发流程图", "用户侧到设备侧的完整闭环，命令格式统一为 key=value")

    chain = [
        (120, 220, 390, 310, "页面输入阈值\n温度 / 湿度", BLUE, SOFT_BLUE),
        (460, 220, 760, 310, "FastAPI 接口\n/api/command/threshold", GREEN, SOFT_TEAL),
        (830, 220, 1130, 310, "命令封装\nSET_THRESH key=value", BLUE, "white"),
        (1200, 220, 1480, 310, "MQTT command\n发布到设备端", GREEN, SOFT_TEAL),
        (1550, 220, 1830, 310, "mqtt_command_cb()\n解析并分发", BLUE, SOFT_BLUE),
        (1890, 220, 2100, 310, "结果确认\nACK / 状态更新", ORANGE, SOFT_ORANGE),
    ]
    for i, (x1, y1, x2, y2, text, out, fill) in enumerate(chain):
        box(d, (x1, y1, x2, y2), text, fill=fill, outline=out, fnt=F_SM if i < 4 else F_XS)
        if i < len(chain) - 1:
            arrow(d, (x2, 265), (chain[i + 1][0], 265), color=out, width=5)

    box(d, (230, 430, 690, 560), "阈值校验\n范围 / 类型 / 空值", fill=SOFT_ORANGE, outline=ORANGE, fnt=F_SM)
    box(d, (760, 430, 1240, 560), "环境监测更新\napp_env_monitor_set_threshold()", fill=SOFT_TEAL, outline=GREEN, fnt=F_SM)
    box(d, (1310, 430, 1760, 560), "共享内存同步\napp_ipc_put_command()", fill="white", outline=BLUE, fnt=F_SM)
    box(d, (1830, 430, 2080, 560), "设备端生效\n阈值立即刷新", fill=SOFT_BLUE, outline=BLUE, fnt=F_SM)
    arrow(d, (1050, 310), (1050, 430), color=GREEN, width=5)
    arrow(d, (1450, 310), (1450, 430), color=BLUE, width=5)

    label_chip(d, (120, 650, 550, 710), "命令格式示例：SET_THRESH temp_max=37.50", fill=SOFT_GRAY, outline=LINE, text_fill=DARK)
    label_chip(d, (610, 650, 1040, 710), "命令格式示例：SET_THRESH humi_min=40.00", fill=SOFT_GRAY, outline=LINE, text_fill=DARK)
    label_chip(d, (1100, 650, 1530, 710), "PC 端负责展示与提交，设备端负责执行", fill=SOFT_TEAL, outline="#9adfd2", text_fill=GREEN)
    label_chip(d, (1590, 650, 2080, 710), "闭环目标：阈值更新后立即影响风险判断", fill=SOFT_ORANGE, outline="#f0cfa4", text_fill=ORANGE)

    box(d, (170, 820, 610, 950), "设备端逻辑\n解析 key=value", fill="white", outline=BLUE, fnt=F_SM)
    box(d, (700, 820, 1140, 950), "环境阈值\n上限 / 下限同步更新", fill="white", outline=GREEN, fnt=F_SM)
    box(d, (1230, 820, 1670, 950), "IPC 共享区\n状态同步给其他模块", fill="white", outline=BLUE, fnt=F_SM)
    box(d, (1760, 820, 2080, 950), "结果反馈\nUI 可见 / 日志可追踪", fill="white", outline=ORANGE, fnt=F_SM)
    arrow(d, (610, 885), (700, 885), color=BLUE, width=5)
    arrow(d, (1140, 885), (1230, 885), color=GREEN, width=5)
    arrow(d, (1670, 885), (1760, 885), color=ORANGE, width=5)
    return save(img, "fig6_threshold.png")


if __name__ == "__main__":
    paths = [fig1(), fig2(), fig3(), fig4(), fig5(), fig6()]
    for p in paths:
        print(p)
