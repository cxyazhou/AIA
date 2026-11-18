# -*- coding: utf-8 -*-
"""
电视风格媒体中心
================
启动后读取脚本所在目录下的 4 个清单文件（每行一个文件夹路径）：
  tv.cfg     -> 只读取每个路径下的【子目录】（电视剧，目录名作为剧名）
  movie.cfg  -> 递归读取每个路径下的视频文件
  music.cfg  -> 递归读取每个路径下的音乐文件
  pic.cfg    -> 递归读取每个路径下的图片文件

操作方式（键盘，类似电视盒子）：
  左侧分类：↑ / ↓ 切换 电视剧 / 电影 / 图片 / 音乐
  →        进入右侧内容区
  方向键    在海报/列表间移动，自动翻页
  Enter    播放；电视剧先弹出选集窗口
  Esc      返回左侧 / 关闭播放
"""

import os
import re
import sys
import ctypes
import subprocess
import tkinter as tk
import platform

IS_WINDOWS = platform.system() == "Windows"
IS_LINUX   = platform.system() == "Linux"
IS_MAC     = platform.system() == "Darwin"

try:
    import vlc  # python-vlc，调用系统安装的 VLC 进行内嵌播放
except Exception:
    vlc = None

from PIL import Image, ImageDraw, ImageFont, ImageTk

# ----------------------------------------------------------------------
# 基础配置
# ----------------------------------------------------------------------
BASE_DIR = os.path.dirname(os.path.abspath(__file__))

# 分类 -> 清单文件名
CATEGORIES = [
    ("tv",    "电视剧", "tv.cfg",    "剧"),
    ("movie", "电影",   "movie.cfg", "影"),
    ("pic",   "图片",   "pic.cfg",   "图"),
    ("music", "音乐",   "music.cfg", "♪"),
]
# 左侧操作项（排在媒体分类下方，不属于内容分类）
# key, 显示名, 字形占位
ACTIONS = [
    ("quit",     "退出", "出"),
    ("shutdown", "关机", "关"),
]
# 左侧菜单总数 / 分类数
N_CAT = len(CATEGORIES)
N_MENU = N_CAT + len(ACTIONS)
COUNTDOWN_SECONDS = 5

VIDEO_EXTS = {
    ".mp4", ".mkv", ".avi", ".rmvb", ".rm", ".ts", ".m2ts", ".mov",
    ".wmv", ".flv", ".f4v", ".m4v", ".mpg", ".mpeg", ".3gp", ".webm",
    ".iso", ".vob",
}
AUDIO_EXTS = {
    ".mp3", ".flac", ".wav", ".ape", ".m4a", ".aac", ".ogg",
    ".wma", ".opus", ".aiff", ".cue",
}
IMAGE_EXTS = {
    ".jpg", ".jpeg", ".png", ".bmp", ".gif", ".webp", ".tif", ".tiff",
}
POSTER_EXTS = (".jpg", ".jpeg", ".png")

# 颜色（深色 + 蓝色主题）
C_BG       = "#000000"
C_PANEL    = "#0d0d0d"
C_CARD     = "#1b1b1b"
C_CARD_SEL = "#3498db"
C_TEXT     = "#f2f2f2"
C_SUB      = "#8a8a8a"
C_ACCENT   = "#3498db"
C_SEL_BG   = "#14304a"
C_DANGER   = "#e74c3c"
C_DANGER_BG = "#3a1715"

FONT_FAMILY = "Microsoft YaHei UI"

# 海报网格参数
POSTER_COLS, POSTER_ROWS = 5, 2
POSTER_W, POSTER_H = 172, 244
PIC_COLS, PIC_ROWS = 5, 2
PIC_W, PIC_H = 230, 140
MUSIC_ROWS = 10


# ----------------------------------------------------------------------
# 工具函数
# ----------------------------------------------------------------------
def natural_key(text):
    """让 第1集 / 第2集 / 第10集 按数字大小排序。"""
    return [int(p) if p.isdigit() else p.lower()
            for p in re.split(r"(\d+)", text)]


def read_list_file(path):
    """读取清单文件，每行一个路径，忽略空行和 # 开头注释。"""
    lines = None
    for enc in ("utf-8-sig", "gbk"):
        try:
            with open(path, "r", encoding=enc) as f:
                lines = f.readlines()
            break
        except UnicodeDecodeError:
            continue
        except OSError:
            return []
    if lines is None:
        return []
    paths = []
    for line in lines:
        p = line.strip()
        if p and not p.startswith("#"):
            paths.append(os.path.abspath(os.path.expandvars(p)))
    return paths


def list_file_candidates():
    """返回 [(category_key, 实际清单路径 or None), ...]。"""
    result = []
    for key, _name, fname, _glyph in CATEGORIES:
        p = os.path.join(BASE_DIR, fname)
        path = p if os.path.isfile(p) else None
        result.append((key, path))
    return result


def scan_tv(paths):
    """tv.cfg：只取每个根路径下的一级子目录。"""
    items = []
    for root in paths:
        if not os.path.isdir(root):
            print("跳过不存在的路径: %s" % root)
            continue
        try:
            entries = [e for e in os.scandir(root) if e.is_dir()]
        except OSError as exc:
            print("无法读取目录 %s: %s" % (root, exc))
            continue
        for e in entries:
            full = e.path
            items.append({
                "type": "tv",
                "name": e.name,
                "disp": e.name,
                "path": full,
                "poster": find_tv_poster(full),
            })
    items.sort(key=lambda it: natural_key(it["name"]))
    return items


def scan_files(paths, exts, item_type):
    """movie/music/pic：递归收集指定扩展名的文件。"""
    items = []
    for root in paths:
        if not os.path.isdir(root):
            print("跳过不存在的路径: %s" % root)
            continue
        for dirpath, _dirs, files in os.walk(root):
            for fn in files:
                if os.path.splitext(fn)[1].lower() in exts:
                    full = os.path.join(dirpath, fn)
                    item = {
                        "type": item_type,
                        "name": fn,
                        "disp": os.path.splitext(fn)[0],
                        "path": full,
                    }
                    if item_type == "movie":
                        item["poster"] = find_movie_poster(full)
                    items.append(item)
    items.sort(key=lambda it: natural_key(it["path"]))
    return items


def find_tv_poster(dir_path):
    """电视剧海报：目录下与目录同名的 .jpg（兼容 jpeg/png）。"""
    base = os.path.join(dir_path, os.path.basename(dir_path))
    for ext in POSTER_EXTS:
        p = base + ext
        if os.path.isfile(p):
            return p
    return None


def find_movie_poster(file_path):
    """电影海报：与视频同目录、同主名的 .jpg（兼容 jpeg/png）。"""
    stem = os.path.splitext(file_path)[0]
    for ext in POSTER_EXTS:
        p = stem + ext
        if os.path.isfile(p):
            return p
    return None


def list_tv_episodes(dir_path):
    """电视剧选集：目录下的每个可播放文件（音视频，排除海报图片），自然排序。"""
    playable = VIDEO_EXTS | AUDIO_EXTS
    try:
        names = [n for n in os.listdir(dir_path)
                 if os.path.isfile(os.path.join(dir_path, n))
                 and os.path.splitext(n)[1].lower() in playable]
    except OSError:
        return []
    names.sort(key=natural_key)
    return names


# ----------------------------------------------------------------------
# 图片缩略图 / 占位图
# ----------------------------------------------------------------------
_FONT_CACHE = {}


def _load_font(size):
    if size in _FONT_CACHE:
        return _FONT_CACHE[size]
    font = None
    for fp in (r"C:\Windows\Fonts\msyh.ttc",
               r"C:\Windows\Fonts\msyhbd.ttc",
               r"C:\Windows\Fonts\segoeui.ttf"):
        try:
            font = ImageFont.truetype(fp, size)
            break
        except OSError:
            continue
    if font is None:
        font = ImageFont.load_default()
    _FONT_CACHE[size] = font
    return font


def cover_resize(im, w, h):
    src_w, src_h = im.size
    scale = max(w / src_w, h / src_h)
    new_w, new_h = max(1, int(src_w * scale)), max(1, int(src_h * scale))
    im = im.resize((new_w, new_h), Image.LANCZOS)
    left = (new_w - w) // 2
    top = (new_h - h) // 2
    return im.crop((left, top, left + w, top + h))


def fit_resize(im, w, h):
    src_w, src_h = im.size
    scale = min(w / src_w, h / src_h)
    new_w = max(1, int(src_w * scale))
    new_h = max(1, int(src_h * scale))
    return im.resize((new_w, new_h), Image.LANCZOS)


def make_placeholder(w, h, glyph):
    img = Image.new("RGB", (w, h), (26, 26, 30))
    draw = ImageDraw.Draw(img)
    draw.rectangle([0, 0, w - 1, h - 1], outline=(70, 70, 78))
    font = _load_font(int(min(w, h) * 0.36))
    try:
        bb = draw.textbbox((0, 0), glyph, font=font)
        tw, th = bb[2] - bb[0], bb[3] - bb[1]
        draw.text(((w - tw) / 2 - bb[0], (h - th) / 2 - bb[1]),
                  glyph, fill=(110, 110, 120), font=font)
    except Exception:
        pass
    return img


def workarea_geometry(root):
    if IS_WINDOWS:
        try:
            class RECT(ctypes.Structure):
                _fields_ = [("left", ctypes.c_long), ("top", ctypes.c_long),
                            ("right", ctypes.c_long), ("bottom", ctypes.c_long)]
            rc = RECT()
            ctypes.windll.user32.SystemParametersInfoW(0x0030, 0, ctypes.byref(rc), 0)
            return (rc.right - rc.left, rc.bottom - rc.top, rc.left, rc.top)
        except Exception:
            pass
    root.update_idletasks()
    return (root.winfo_screenwidth(), root.winfo_screenheight(), 0, 0)


def bring_to_front(top):
    """让新弹出的 Toplevel 在 Windows 上可靠显示到最前并拿到焦点。

    模态弹窗(grab_set)销毁的同一回调里创建新窗口时，主窗口会在事件
    处理结束后被重新激活，把新窗口压到下面；用“临时置顶再取消”规避。
    """
    try:
        top.deiconify()
        top.lift()
        top.focus_force()
        top.attributes("-topmost", True)

        def _release():
            try:
                if top.winfo_exists():
                    top.attributes("-topmost", False)
                    top.lift()
            except tk.TclError:
                pass

        top.after(300, _release)
    except tk.TclError:
        pass


class ThumbCache:
    def __init__(self, limit=300):
        self.limit = limit
        self._data = {}
        self._order = []

    def get(self, path, w, h, glyph=""):
        key = (path, w, h)
        if key in self._data:
            return self._data[key]
        if path and os.path.isfile(path):
            try:
                with Image.open(path) as im:
                    im = im.convert("RGB")
                    img = cover_resize(im, w, h)
            except Exception:
                img = make_placeholder(w, h, glyph)
        else:
            img = make_placeholder(w, h, glyph)
        photo = ImageTk.PhotoImage(img)
        self._data[key] = photo
        self._order.append(key)
        if len(self._order) > self.limit:
            for old in self._order[:len(self._order) - self.limit]:
                self._data.pop(old, None)
            del self._order[:len(self._order) - self.limit]
        return photo


# ----------------------------------------------------------------------
# 主界面
# ----------------------------------------------------------------------
class MediaApp:
    def __init__(self, root):
        self.root = root
        self.items = {key: [] for key, *_ in CATEGORIES}
        self.menu_index = 0                     # 左侧菜单索引：4 分类 + 退出/关机
        self.zone = "left"                       # left / right
        self.sel = {key: 0 for key, *_ in CATEGORIES}
        self.cache = ThumbCache()
        self.vlc_inst = None
        self._player_win = None
        self._viewer = None
        self._dialog = None
        self._confirm = None
        self._exiting = False

        root.title("媒体中心")
        root.configure(bg=C_BG)
        # 去掉标题栏，窗口铺满工作区（不遮挡任务栏）；
        # 退出统一从左侧“退出”入口
        w, h, x, y = workarea_geometry(root)
        root.geometry("%dx%d+%d+%d" % (w, h, x, y))
        if IS_WINDOWS:
            root.overrideredirect(True)
        else:
            # Linux / macOS：交给窗口管理器做全屏，更稳
            root.attributes("-fullscreen", True)
        self._build_ui()
        self.bind_main_keys()
        self.root.after(80, self.bootstrap)
        # 无边框窗口不会自动抢前台；启动后保持置顶约 2 秒，
        # 等启动它的终端/资源管理器 settle，再恢复普通窗口层级，
        # 之后用户可正常切到其它程序
        self.root.after(120, self._startup_to_front)

    def _startup_to_front(self):
        try:
            self.root.deiconify()
            self.root.lift()
            self.root.focus_force()
            self.root.attributes("-topmost", True)
            self.root.after(2000, self._release_startup_topmost)
        except tk.TclError:
            pass

    def _release_startup_topmost(self):
        try:
            if self.root.winfo_exists():
                self.root.attributes("-topmost", False)
                self.root.lift()
        except tk.TclError:
            pass

    # ---------------- 菜单辅助 ----------------
    def _menu_label(self, idx):
        if idx < N_CAT:
            return CATEGORIES[idx][1]
        return ACTIONS[idx - N_CAT][1]

    def _menu_glyph(self, idx):
        if idx < N_CAT:
            return CATEGORIES[idx][3]
        return ACTIONS[idx - N_CAT][2]

    def _current_cat(self):
        """当前菜单是媒体分类时返回分类元组，否则 None。"""
        if self.menu_index < N_CAT:
            return CATEGORIES[self.menu_index]
        return None

    # ---------------- 界面搭建 ----------------
    def _build_ui(self):
        body = tk.Frame(self.root, bg=C_BG)
        body.pack(fill="both", expand=True)

        # 左侧分类
        self.side = tk.Frame(body, bg=C_PANEL, width=230)
        self.side.pack(side="left", fill="y")
        self.side.pack_propagate(False)

        tk.Label(self.side, text="媒体中心", bg=C_PANEL, fg=C_ACCENT,
                 font=(FONT_FAMILY, 22, "bold"),
                 pady=26).pack(fill="x")

        self.side_buttons = []
        for i in range(N_MENU):
            if i == N_CAT:
                tk.Frame(self.side, bg="#262626", height=1).pack(
                    fill="x", padx=24, pady=14)
            is_action = i >= N_CAT
            name = self._menu_label(i)
            btn = tk.Label(self.side, text="  " + name, bg=C_PANEL,
                           fg=(C_DANGER if is_action else C_SUB),
                           font=(FONT_FAMILY, 17),
                           anchor="w", padx=26, pady=14, cursor="hand2")
            btn.pack(fill="x")
            btn.bind("<Button-1>",
                     lambda e, idx=i: self._mouse_pick_menu(idx))
            self.side_buttons.append(btn)

        tk.Label(self.side, text="↑↓ 切换   → 浏览   Enter 播放/执行",
                 bg=C_PANEL, fg="#555555", font=(FONT_FAMILY, 10),
                 pady=10).pack(side="bottom", fill="x")

        # 右侧内容
        right = tk.Frame(body, bg=C_BG)
        right.pack(side="left", fill="both", expand=True)

        topbar = tk.Frame(right, bg=C_BG, height=74)
        topbar.pack(fill="x", padx=36)
        topbar.pack_propagate(False)
        self.title_var = tk.StringVar(value="正在扫描媒体库…")
        tk.Label(topbar, textvariable=self.title_var, bg=C_BG, fg=C_TEXT,
                 font=(FONT_FAMILY, 24, "bold")).pack(side="left", pady=18)
        self.page_var = tk.StringVar(value="")
        tk.Label(topbar, textvariable=self.page_var, bg=C_BG, fg=C_SUB,
                 font=(FONT_FAMILY, 12)).pack(side="right", pady=18)

        self.hint_var = tk.StringVar(
            value="方向键移动选择   Enter 播放 / 选集   Esc 返回")
        tk.Label(right, textvariable=self.hint_var, bg=C_BG, fg="#666666",
                 font=(FONT_FAMILY, 11)).pack(anchor="w", padx=36)

        self.grid = tk.Frame(right, bg=C_BG)
        self.grid.pack(fill="both", expand=True, padx=26, pady=(8, 20))

        self.status_var = tk.StringVar(value="")
        tk.Label(right, textvariable=self.status_var, bg=C_BG, fg="#555555",
                 font=(FONT_FAMILY, 10), anchor="w").pack(
            fill="x", padx=36, pady=(0, 10))

        self.render_sidebar()

    # ---------------- 启动扫描 ----------------
    def bootstrap(self):
        self.root.update_idletasks()
        counts = {}
        for key, list_path in list_file_candidates():
            if list_path is None:
                self.items[key] = []
                counts[key] = 0
                continue
            paths = read_list_file(list_path)
            if key == "tv":
                self.items[key] = scan_tv(paths)
            elif key == "movie":
                self.items[key] = scan_files(paths, VIDEO_EXTS, "movie")
            elif key == "pic":
                self.items[key] = scan_files(paths, IMAGE_EXTS, "pic")
            else:
                self.items[key] = scan_files(paths, AUDIO_EXTS, "music")
            counts[key] = len(self.items[key])
        self.status_var.set(
            "   ".join("%s %d 项" % (name, counts[key])
                       for key, name, *_ in CATEGORIES))
        self.render_sidebar()
        self.render_content()

    # ---------------- 渲染：左侧 ----------------
    def render_sidebar(self):
        for i in range(N_MENU):
            btn = self.side_buttons[i]
            is_action = i >= N_CAT
            base_fg = C_DANGER if is_action else C_SUB
            if i == self.menu_index and self.zone == "left":
                btn.configure(bg=(C_DANGER_BG if is_action else C_SEL_BG),
                              fg=(C_DANGER if is_action else C_ACCENT),
                              font=(FONT_FAMILY, 17, "bold"))
            elif i == self.menu_index:
                btn.configure(bg="#101418", fg=C_TEXT,
                              font=(FONT_FAMILY, 17, "bold"))
            else:
                btn.configure(bg=C_PANEL, fg=base_fg,
                              font=(FONT_FAMILY, 17))

    # ---------------- 渲染：右侧内容 ----------------
    def _grid_spec(self, key):
        if key == "pic":
            return PIC_COLS, PIC_ROWS, PIC_W, PIC_H
        if key == "music":
            return 1, MUSIC_ROWS, 0, 0
        return POSTER_COLS, POSTER_ROWS, POSTER_W, POSTER_H

    def render_content(self):
        cat = self._current_cat()
        if cat is None:
            self._render_action_page()
            return
        key, name, _f, glyph = cat
        items = self.items[key]
        cols, rows, cw, ch = self._grid_spec(key)
        page_size = cols * rows
        total = len(items)
        total_pages = max(1, (total + page_size - 1) // page_size)
        idx = self.sel[key] if total else 0
        page = idx // page_size if total else 0
        start = page * page_size

        for w in self.grid.winfo_children():
            w.destroy()

        self.title_var.set(name)
        self.page_var.set("第 %d/%d 页" % (page + 1, total_pages)
                          if total else "")

        if not total:
            tk.Label(self.grid, text="（暂无内容，请检查对应清单文件中的路径）",
                     bg=C_BG, fg=C_SUB, font=(FONT_FAMILY, 16)).pack(
                expand=True)
            return

        if key == "music":
            self._render_music(items, start, page_size, idx)
        else:
            self._render_cards(key, glyph, items, start, page_size,
                               cols, cw, ch, idx)

    def _render_action_page(self):
        """右侧：退出 / 关机 的操作提示页。"""
        action_key, name, _glyph = ACTIONS[self.menu_index - N_CAT]
        for w in self.grid.winfo_children():
            w.destroy()
        self.title_var.set(name)
        self.page_var.set("")
        box = tk.Frame(self.grid, bg=C_PANEL)
        box.pack(expand=True)
        tk.Label(box, text=name, bg=C_PANEL, fg=C_DANGER,
                 font=(FONT_FAMILY, 44, "bold"),
                 pady=30).pack(fill="x", padx=120)
        if action_key == "quit":
            desc = "退出媒体中心并关闭程序"
        else:
            desc = "关闭这台电脑"
        tk.Label(box, text=desc, bg=C_PANEL, fg=C_TEXT,
                 font=(FONT_FAMILY, 17), pady=8).pack(fill="x")
        tk.Label(box, text="按 Enter 执行（将有 %d 秒倒计时确认）   Esc 取消"
                           % COUNTDOWN_SECONDS,
                 bg=C_PANEL, fg=C_SUB,
                 font=(FONT_FAMILY, 12), pady=8).pack(fill="x")

    def _render_cards(self, key, glyph, items, start, page_size,
                      cols, cw, ch, idx):
        for slot in range(page_size):
            pos = start + slot
            if pos >= len(items):
                break
            item = items[pos]
            selected = self.zone == "right" and pos == idx
            r, c = divmod(slot, cols)

            outer = tk.Frame(self.grid, bg=(C_CARD_SEL if selected else "#0c0c0c"),
                             bd=0)
            outer.grid(row=r, column=c, padx=12, pady=12, sticky="n")
            card = tk.Frame(outer, bg=C_CARD, bd=0)
            card.pack(padx=3, pady=3)

            poster_path = item.get("poster") or (
                item["path"] if key == "pic" else None)
            photo = self.cache.get(poster_path, cw, ch, glyph)
            img_lb = tk.Label(card, image=photo, bg=C_CARD, bd=0)
            img_lb.image = photo
            img_lb.pack(padx=6, pady=(6, 4))
            disp = item.get("disp", item["name"])
            title = disp if len(disp) <= 14 else disp[:13] + "…"
            title_lb = tk.Label(
                card, text=title, bg=C_CARD,
                fg=C_TEXT if selected else "#d8d8d8",
                font=(FONT_FAMILY, 11, "bold" if selected else "normal"),
                wraplength=cw + 10, justify="center", width=12)
            title_lb.pack(padx=6, pady=(0, 8))

            for w in (outer, card, img_lb, title_lb):
                w.bind("<Button-1>",
                       lambda e, p=pos: self._mouse_pick_item(p))
        # 居中列
        for c in range(cols):
            self.grid.grid_columnconfigure(c, weight=1)

    def _render_music(self, items, start, page_size, idx):
        list_frame = tk.Frame(self.grid, bg=C_BG)
        list_frame.pack(fill="both", expand=True, padx=10)
        for slot in range(page_size):
            pos = start + slot
            if pos >= len(items):
                break
            item = items[pos]
            selected = self.zone == "right" and pos == idx
            row = tk.Frame(list_frame,
                           bg=C_ACCENT if selected else "#101010", bd=0)
            row.pack(fill="x", pady=3)
            inner = tk.Frame(row, bg="#14304a" if selected else C_CARD)
            inner.pack(fill="x", padx=2, pady=2)
            mark = "▶" if selected else "·"
            tk.Label(inner, text=" %s %02d" % (mark, pos + 1),
                     bg=inner["bg"], fg="#ffffff" if selected else C_SUB,
                     font=(FONT_FAMILY, 12, "bold"), width=7,
                     anchor="w").pack(side="left", padx=(10, 4), pady=8)
            name = item.get("disp", item["name"])
            tk.Label(inner, text=name, bg=inner["bg"],
                     fg="#ffffff" if selected else C_TEXT,
                     font=(FONT_FAMILY, 13, "bold" if selected else "normal"),
                     anchor="w").pack(side="left", pady=8)
            folder = os.path.basename(os.path.dirname(item["path"]))
            tk.Label(inner, text=folder, bg=inner["bg"],
                     fg="#bfe0f7" if selected else C_SUB,
                     font=(FONT_FAMILY, 10), anchor="e").pack(
                side="right", padx=16, pady=8)
            for w in (row, inner) + tuple(inner.winfo_children()):
                w.bind("<Button-1>",
                       lambda e, p=pos: self._mouse_pick_item(p))

    # ---------------- 键盘导航 ----------------
    def bind_main_keys(self):
        m = {
            "<Up>":      self.on_up,
            "<Down>":    self.on_down,
            "<Left>":    self.on_left,
            "<Right>":   self.on_right,
            "<Return>":  self.on_enter,
            "<KP_Up>":   self.on_up,
            "<KP_Down>": self.on_down,
            "<KP_Left>": self.on_left,
            "<KP_Right>": self.on_right,
            "<KP_Enter>": self.on_enter,
            "<Escape>":  self.on_escape,
        }
        for seq, fn in m.items():
            self.root.bind_all(seq, fn)

    def on_up(self, _e=None):
        if self.zone == "left":
            self.menu_index = (self.menu_index - 1) % N_MENU
        else:
            key = self._current_cat()[0]
            self._move_selection(-self._step_vertical(key))
        self._refresh()

    def on_down(self, _e=None):
        if self.zone == "left":
            self.menu_index = (self.menu_index + 1) % N_MENU
        else:
            key = self._current_cat()[0]
            self._move_selection(self._step_vertical(key))
        self._refresh()

    def on_left(self, _e=None):
        if self.zone == "right":
            key = self._current_cat()[0]
            cols = self._grid_spec(key)[0]
            if cols == 1 or self.sel[key] % cols == 0:
                self.zone = "left"
            else:
                self._move_selection(-1)
        self._refresh()

    def on_right(self, _e=None):
        if self.zone == "left":
            cat = self._current_cat()
            # 操作项不进入右侧；分类有内容才进入
            if cat is not None and self.items[cat[0]]:
                self.zone = "right"
        else:
            self._move_selection(1)
        self._refresh()

    def on_enter(self, _e=None):
        if self.zone == "left":
            cat = self._current_cat()
            if cat is None:
                action_key = ACTIONS[self.menu_index - N_CAT][0]
                self.request_action(action_key)
            elif self.items[cat[0]]:
                self.zone = "right"
                self._refresh()
            return
        key = self._current_cat()[0]
        items = self.items[key]
        if not items:
            return
        item = items[self.sel[key]]
        self.activate(item)

    def on_escape(self, _e=None):
        if self.zone == "right":
            self.zone = "left"
            self._refresh()

    def _step_vertical(self, key):
        cols, _rows, _w, _h = self._grid_spec(key)
        return cols

    def _move_selection(self, delta):
        key = self._current_cat()[0]
        total = len(self.items[key])
        if not total:
            return
        self.sel[key] = max(0, min(total - 1, self.sel[key] + delta))

    def _refresh(self):
        self.render_sidebar()
        self.render_content()

    # ---------------- 鼠标（辅助） ----------------
    def _mouse_pick_menu(self, idx):
        self.menu_index = idx
        self.zone = "left"
        self._refresh()
        if idx >= N_CAT:
            # 点击退出/关机：直接给出 5 秒倒计时提示
            action_key = ACTIONS[idx - N_CAT][0]
            self.request_action(action_key)

    def _mouse_pick_item(self, pos):
        key = self._current_cat()[0]
        self.sel[key] = pos
        self.zone = "right"
        self._refresh()
        self.activate(self.items[key][pos])

    # ---------------- 退出 / 关机 ----------------
    def request_action(self, action_key):
        if self._confirm is not None or self._exiting:
            return
        self._confirm = CountdownDialog(self, action_key)

    def execute_action(self, action_key):
        if self._exiting:
            return
        self._exiting = True
        # 先释放播放相关资源
        for win in (self._player_win, self._viewer):
            if win is not None:
                try:
                    win.close()
                except Exception:
                    pass
        if action_key == "shutdown":
            try:
                if IS_WINDOWS:
                    # /t 0 立即关机；CREATE_NO_WINDOW 避免弹出控制台
                    subprocess.Popen(["shutdown", "/s", "/t", "0"],
                                     creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
                else:
                    subprocess.Popen(["systemctl", "poweroff"])
            except Exception as exc:
                self._exiting = False
                self.status_var.set("关机命令执行失败: %s" % exc)
                return
        try:
            self.root.destroy()
        except tk.TclError:
            pass
        sys.exit(0)

    # ---------------- 激活 / 播放 ----------------
    def activate(self, item):
        if item["type"] == "tv":
            self.open_episode_dialog(item)
        elif item["type"] == "pic":
            self.open_picture_viewer(item)
        else:
            self.play_media(item["path"], item.get("disp", item["name"]))

    def _get_vlc(self):
        if self.vlc_inst is not None:
            return self.vlc_inst
        if vlc is None:
            return None
        try:
            self.vlc_inst = vlc.Instance("--no-video-title-show",
                                         "--quiet")
        except Exception as exc:
            print("VLC 初始化失败: %s" % exc)
            self.vlc_inst = None
        return self.vlc_inst

    def play_media(self, path, title=""):
        inst = self._get_vlc()
        if inst is None:
            self._open_external(path)
            return
        if self._player_win is not None:
            try:
                self._player_win.close()
            except Exception:
                pass
        self._player_win = PlayerWindow(self, inst, path, title)

    def _open_external(self, path):
        try:
            if IS_WINDOWS:
                os.startfile(path)
            elif IS_MAC:
                subprocess.Popen(["open", path])
            else:
                subprocess.Popen(["xdg-open", path])
        except Exception as exc:
            self.status_var.set("无法播放 %s (%s)" % (path, exc))
        
    # ---------------- 电视剧选集 ----------------
    def open_episode_dialog(self, item):
        episodes = list_tv_episodes(item["path"])
        if not episodes:
            self.status_var.set("《%s》目录下没有可播放的文件" % item["name"])
            return
        self._dialog = EpisodeDialog(self, item, episodes)

    # ---------------- 图片查看 ----------------
    def open_picture_viewer(self, item):
        pics = self.items["pic"]
        try:
            idx = pics.index(item)
        except ValueError:
            idx = 0
        if self._viewer is not None:
            try:
                self._viewer.close()
            except Exception:
                pass
        self._viewer = PictureViewer(self, pics, idx)


# ----------------------------------------------------------------------
# 电视剧选集弹窗
# ----------------------------------------------------------------------
class EpisodeDialog:
    def __init__(self, app, item, episodes):
        self.app = app
        self.item = item
        self.episodes = episodes
        top = tk.Toplevel(app.root)
        self.top = top
        top.title("选择剧集 - %s" % item["name"])
        top.configure(bg=C_PANEL)
        w, h = 720, 560
        x = app.root.winfo_rootx() + (app.root.winfo_width() - w) // 2
        y = app.root.winfo_rooty() + (app.root.winfo_height() - h) // 2
        top.geometry("%dx%d+%d+%d" % (w, h, max(0, x), max(0, y)))
        top.transient(app.root)
        top.resizable(True, True)

        tk.Label(top, text="《%s》 选择剧集" % item["name"],
                 bg=C_PANEL, fg=C_TEXT, font=(FONT_FAMILY, 16, "bold"),
                 anchor="w", padx=20, pady=14).pack(fill="x")

        body = tk.Frame(top, bg=C_PANEL)
        body.pack(fill="both", expand=True, padx=16)
        sb = tk.Scrollbar(body)
        sb.pack(side="right", fill="y")
        self.lb = tk.Listbox(
            body, bg="#151515", fg=C_TEXT, selectbackground=C_ACCENT,
            selectforeground="#ffffff", highlightthickness=0,
            activestyle="none", font=(FONT_FAMILY, 13),
            selectmode="single", relief="flat", bd=0,
            yscrollcommand=sb.set)
        self.lb.pack(side="left", fill="both", expand=True)
        sb.configure(command=self.lb.yview)
        for name in episodes:
            self.lb.insert(tk.END, "  " + name)

        tk.Label(top, text="↑↓ 选择   Enter / 双击 播放   Esc 返回",
                 bg=C_PANEL, fg=C_SUB, font=(FONT_FAMILY, 10),
                 anchor="w", padx=20, pady=10).pack(fill="x")

        self.lb.selection_set(0)
        self.lb.activate(0)
        self.lb.see(0)
        self.lb.focus_set()
        # 直接在 Listbox 上绑定（widget 标签最先触发），回调返回
        # "break" 可同时阻断 Listbox 内置方向键行为与主窗口的
        # bind_all 同名绑定，避免一集跳两项或主界面跟着移动。
        self.lb.bind("<Double-Button-1>", self._on_ok)
        self.lb.bind("<Up>", self._on_up)
        self.lb.bind("<Down>", self._on_down)
        self.lb.bind("<KP_Up>", self._on_up)
        self.lb.bind("<KP_Down>", self._on_down)
        self.lb.bind("<Left>", self._break)
        self.lb.bind("<Right>", self._break)
        self.lb.bind("<KP_Left>", self._break)
        self.lb.bind("<KP_Right>", self._break)
        self.lb.bind("<Return>", self._on_ok)
        self.lb.bind("<KP_Enter>", self._on_ok)
        self.lb.bind("<Escape>", self._on_cancel)
        # 焦点不在列表上时（弹窗其它区域）仍由全局层接管
        self.top.bind("<Return>", self._on_ok)
        self.top.bind("<KP_Enter>", self._on_ok)
        self.top.bind("<Escape>", self._on_cancel)
        top.protocol("WM_DELETE_WINDOW", self._on_cancel)
        top.grab_set()

    @staticmethod
    def _break(_e=None):
        return "break"

    def _on_up(self, _e=None):
        self._move(-1)
        return "break"

    def _on_down(self, _e=None):
        self._move(1)
        return "break"

    def _move(self, delta):
        n = self.lb.size()
        if not n:
            return
        cur = self.lb.curselection()
        i = (cur[0] if cur else 0) + delta
        i = max(0, min(n - 1, i))
        self.lb.selection_clear(0, tk.END)
        self.lb.selection_set(i)
        self.lb.activate(i)
        self.lb.see(i)

    def _on_ok(self, _e=None):
        if not self.top.winfo_exists():
            return
        sel = self.lb.curselection()
        if not sel:
            return
        path = os.path.join(self.item["path"], self.episodes[sel[0]])
        title = "%s - %s" % (self.item["name"], self.episodes[sel[0]])
        app = self.app
        self._destroy()
        # 等弹窗（含 grab）彻底销毁后再创建播放窗口，避免与主窗口
        # 抢激活态导致播放窗口被压在主界面后面
        app.root.after(80, lambda: app.root.winfo_exists()
                       and app.play_media(path, title))
        return "break"

    def _on_cancel(self, _e=None):
        self._destroy()
        return "break"

    def _destroy(self):
        if not self.top.winfo_exists():
            return
        try:
            self.top.grab_release()
        except tk.TclError:
            pass
        self.app.bind_main_keys()
        self.top.destroy()
        self.app._dialog = None


# ----------------------------------------------------------------------
# 退出 / 关机 倒计时确认弹窗
# ----------------------------------------------------------------------
class CountdownDialog:
    def __init__(self, app, action_key, seconds=COUNTDOWN_SECONDS):
        self.app = app
        self.action_key = action_key
        self.total = seconds
        self.remaining = seconds
        self.finished = False
        self._job = None

        is_shutdown = action_key == "shutdown"
        self.title_text = "关闭电脑" if is_shutdown else "退出程序"
        self.detail = ("电脑将在倒计时结束后关闭，请确认没有未保存的工作。"
                       if is_shutdown else "媒体中心将在倒计时结束后关闭。")

        top = tk.Toplevel(app.root)
        self.top = top
        top.title("确认" + self.title_text)
        top.configure(bg=C_PANEL)
        w, h = 520, 300
        x = app.root.winfo_rootx() + (app.root.winfo_width() - w) // 2
        y = app.root.winfo_rooty() + (app.root.winfo_height() - h) // 2
        top.geometry("%dx%d+%d+%d" % (w, h, max(0, x), max(0, y)))
        top.resizable(False, False)
        top.protocol("WM_DELETE_WINDOW", self.cancel)

        tk.Label(top, text=self.title_text, bg=C_PANEL, fg=C_DANGER,
                 font=(FONT_FAMILY, 24, "bold"),
                 pady=22).pack(fill="x")

        self.num_var = tk.StringVar()
        tk.Label(top, textvariable=self.num_var, bg=C_PANEL, fg=C_TEXT,
                 font=(FONT_FAMILY, 46, "bold")).pack(fill="x")
        tk.Label(top, text=self.detail, bg=C_PANEL, fg=C_SUB,
                 font=(FONT_FAMILY, 12), pady=10,
                 wraplength=w - 60, justify="center").pack(fill="x", padx=30)

        btns = tk.Frame(top, bg=C_PANEL)
        btns.pack(pady=12)
        ok = tk.Label(btns, text="  立即执行  ", bg=C_DANGER, fg="#ffffff",
                      font=(FONT_FAMILY, 13, "bold"), padx=18, pady=10,
                      cursor="hand2")
        ok.pack(side="left", padx=14)
        cancel = tk.Label(btns, text="  取消  ", bg="#2a2a2a", fg=C_TEXT,
                          font=(FONT_FAMILY, 13), padx=18, pady=10,
                          cursor="hand2")
        cancel.pack(side="left", padx=14)
        ok.bind("<Button-1>", lambda e: self.execute_now())
        cancel.bind("<Button-1>", lambda e: self.cancel())

        tk.Label(top, text="Enter 立即执行    Esc 取消",
                 bg=C_PANEL, fg="#666666", font=(FONT_FAMILY, 10)).pack(
            side="bottom", pady=(0, 10))

        # 弹窗期间替换主窗口全局按键：确认/取消之外的按键一律吞掉，
        # 关闭时由 bind_main_keys() 恢复
        noop = lambda _e: "break"
        for seq in ("<Up>", "<Down>", "<Left>", "<Right>",
                    "<KP_Up>", "<KP_Down>", "<KP_Left>", "<KP_Right>",
                    "<space>"):
            top.bind_all(seq, noop)
        for seq in ("<Return>", "<KP_Enter>"):
            top.bind_all(seq, self._on_ok_key)
        top.bind_all("<Escape>", self._on_cancel_key)
        top.focus_force()
        top.grab_set()
        bring_to_front(top)
        self._render()
        self._schedule()

    # ---- 键盘包装 ----
    def _on_ok_key(self, _e=None):
        self.execute_now()
        return "break"

    def _on_cancel_key(self, _e=None):
        self.cancel()
        return "break"

    def _render(self):
        self.num_var.set("%d 秒后自动%s" % (
            self.remaining,
            "关机" if self.action_key == "shutdown" else "退出"))

    def _schedule(self):
        self._job = self.top.after(1000, self._tick)

    def _tick(self):
        if self.finished:
            return
        self.remaining -= 1
        if self.remaining <= 0:
            self.execute_now()
            return
        self._render()
        self._schedule()

    def execute_now(self, _e=None):
        if self.finished:
            return
        self.finished = True
        self._cancel_job()
        app = self.app
        action_key = self.action_key
        self._close()
        app.execute_action(action_key)

    def cancel(self, _e=None):
        if self.finished:
            return
        self.finished = True
        self._cancel_job()
        self._close()

    def _cancel_job(self):
        if self._job is not None:
            try:
                self.top.after_cancel(self._job)
            except tk.TclError:
                pass
            self._job = None

    def _close(self):
        try:
            self.top.grab_release()
        except tk.TclError:
            pass
        try:
            self.top.destroy()
        except tk.TclError:
            pass
        self.app._confirm = None
        self.app.bind_main_keys()


# ----------------------------------------------------------------------
# VLC 内嵌播放窗口（视频 / 音乐）
# ----------------------------------------------------------------------
class PlayerWindow:
    def __init__(self, app, inst, path, title):
        self.app = app
        self.path = path
        self.title = title
        self.closed = False

        top = tk.Toplevel(app.root)
        self.top = top
        top.title(title or os.path.basename(path))
        top.configure(bg="#000000")
        top.geometry("1100x660+120+80")
        top.protocol("WM_DELETE_WINDOW", self.close)
        # 启动即全屏（电视剧 / 电影 / 音乐一致），F 可切回窗口模式
        self.is_fullscreen = True
        top.attributes("-fullscreen", True)
        self._overlay_job = None

        self.name_var = tk.StringVar(value=title or os.path.basename(path))
        self.name_lb = tk.Label(top, textvariable=self.name_var,
                                bg="#000000", fg="#dddddd",
                                font=(FONT_FAMILY, 13), pady=8)
        self.name_lb.pack(fill="x")

        self.video = tk.Frame(top, bg="#000000")
        self.video.pack(fill="both", expand=True)

        self.hint_var = tk.StringVar(
            value="空格 暂停/继续   ←/→ 快退快进 10s   ↑/↓ 音量   "
                  "F 全屏/窗口   M 静音   Esc 退出")
        self.hint = tk.Label(top, textvariable=self.hint_var, bg="#000000",
                             fg="#777777", font=(FONT_FAMILY, 10), pady=6)
        self.hint.pack(fill="x")

        self.top.update_idletasks()
        self.player = inst.media_player_new()
        self.player.set_hwnd(self.video.winfo_id())
        self.media = inst.media_new(os.path.abspath(path))
        self.player.set_media(self.media)

        # 不同 python-vlc 版本结束事件命名不同：
        # 新版 MediaPlayerEnded / 旧版 MediaPlayerEndReached
        evt_ended = getattr(vlc.EventType, "MediaPlayerEnded", None) \
            or getattr(vlc.EventType, "MediaPlayerEndReached")
        ev = self.player.event_manager()
        ev.event_attach(evt_ended,
                        lambda _e: self._ui_call(self._on_ended))
        ev.event_attach(vlc.EventType.MediaPlayerEncounteredError,
                        lambda _e: self._ui_call(self._on_error))

        self._bind_keys()
        self.player.play()
        bring_to_front(self.top)
        # 全屏下标题/提示短暂显示后自动隐藏，移动鼠标唤出
        self._overlay_job = self.top.after(2500, self._hide_overlay)

    def _ui_call(self, fn):
        """VLC 回调线程 -> Tk 主线程；窗口已销毁时静默丢弃。"""
        try:
            self.app.root.after(0, fn)
        except (tk.TclError, RuntimeError):
            pass

    def _bind_keys(self):
        m = {
            "<space>":    self.toggle_pause,
            "<Left>":     lambda e: self.seek(-10_000),
            "<Right>":    lambda e: self.seek(10_000),
            "<Up>":       lambda e: self.volume(5),
            "<Down>":     lambda e: self.volume(-5),
            "f":          lambda e: self.toggle_fullscreen(),
            "F":          lambda e: self.toggle_fullscreen(),
            "m":          lambda e: self.toggle_mute(),
            "M":          lambda e: self.toggle_mute(),
            "<Escape>":   lambda e: self.close(),
            "<Motion>":   self._show_overlay,
        }
        for seq, fn in m.items():
            self.top.bind_all(seq, fn)

    # ---------------- 全屏叠加信息 ----------------
    def _cancel_overlay_job(self):
        if self._overlay_job is not None:
            try:
                self.top.after_cancel(self._overlay_job)
            except tk.TclError:
                pass
            self._overlay_job = None

    def _show_overlay(self, _e=None):
        if self.closed or not self.is_fullscreen:
            return
        try:
            if not self.name_lb.winfo_ismapped():
                self.name_lb.pack(fill="x", before=self.video)
            if not self.hint.winfo_ismapped():
                self.hint.pack(fill="x")
        except tk.TclError:
            return
        self._cancel_overlay_job()
        self._overlay_job = self.top.after(2500, self._hide_overlay)

    def _hide_overlay(self):
        self._overlay_job = None
        if self.closed or not self.is_fullscreen:
            return
        try:
            self.name_lb.pack_forget()
            self.hint.pack_forget()
        except tk.TclError:
            pass

    def toggle_pause(self, _e=None):
        try:
            self.player.pause()
        except Exception:
            pass

    def seek(self, ms):
        try:
            self.player.set_time(max(0, self.player.get_time() + ms))
        except Exception:
            pass

    def volume(self, delta):
        try:
            self.player.audio_set_volume(
                max(0, min(150, self.player.audio_get_volume() + delta)))
        except Exception:
            pass

    def toggle_mute(self, _e=None):
        try:
            self.player.audio_toggle_mute()
        except Exception:
            pass

    def toggle_fullscreen(self, _e=None):
        self.is_fullscreen = not self.is_fullscreen
        try:
            self.top.attributes("-fullscreen", self.is_fullscreen)
        except tk.TclError:
            self.is_fullscreen = not self.is_fullscreen
            return
        self._cancel_overlay_job()
        if self.is_fullscreen:
            self._overlay_job = self.top.after(2500, self._hide_overlay)
            return
        # 窗口模式：标题与操作提示常驻
        try:
            if not self.name_lb.winfo_ismapped():
                self.name_lb.pack(fill="x", before=self.video)
            if not self.hint.winfo_ismapped():
                self.hint.pack(fill="x")
        except tk.TclError:
            pass

    def _on_ended(self):
        if not self.closed:
            self.close()

    def _on_error(self):
        self.app.status_var.set("播放出错: %s" % self.title)
        if not self.closed:
            self.close()

    def close(self):
        if self.closed:
            return
        self.closed = True
        self._cancel_overlay_job()
        try:
            self.top.unbind_all("<Motion>")
        except tk.TclError:
            pass
        try:
            self.player.stop()
        except Exception:
            pass
        try:
            self.player.release()
        except Exception:
            pass
        try:
            self.top.destroy()
        except tk.TclError:
            pass
        self.app._player_win = None
        self.app.bind_main_keys()
        self.app.root.after(60, lambda: bring_to_front(self.app.root))


# ----------------------------------------------------------------------
# 图片全屏查看
# ----------------------------------------------------------------------
class PictureViewer:
    def __init__(self, app, pics, index):
        self.app = app
        self.pics = pics
        self.index = index
        self.photo = None

        top = tk.Toplevel(app.root)
        self.top = top
        top.title("图片查看")
        top.configure(bg="#000000")
        top.protocol("WM_DELETE_WINDOW", self.close)
        try:
            top.attributes("-fullscreen", True)
        except tk.TclError:
            top.state("zoomed")

        self.label = tk.Label(top, bg="#000000")
        self.label.pack(fill="both", expand=True)
        self.info_var = tk.StringVar()
        tk.Label(top, textvariable=self.info_var, bg="#000000", fg="#999999",
                 font=(FONT_FAMILY, 12), pady=8).pack(fill="x")

        for seq, fn in (("<Left>", self.prev), ("<Up>", self.prev),
                        ("<KP_Left>", self.prev), ("<KP_Up>", self.prev),
                        ("<Right>", self.next), ("<Down>", self.next),
                        ("<KP_Right>", self.next), ("<KP_Down>", self.next),
                        ("<Return>", self.next), ("<KP_Enter>", self.next),
                        ("<space>", self.next),
                        ("<Escape>", lambda e: self.close())):
            top.bind_all(seq, fn)
        bring_to_front(top)
        self.show()

    def prev(self, _e=None):
        self.index = (self.index - 1) % len(self.pics)
        self.show()

    def next(self, _e=None):
        self.index = (self.index + 1) % len(self.pics)
        self.show()

    def show(self):
        item = self.pics[self.index]
        try:
            with Image.open(item["path"]) as im:
                im = im.convert("RGB")
                self.top.update_idletasks()
                w = max(100, self.top.winfo_width() - 40)
                h = max(100, self.top.winfo_height() - 90)
                img = fit_resize(im, w, h)
            self.photo = ImageTk.PhotoImage(img)
            self.label.configure(image=self.photo)
        except Exception as exc:
            self.label.configure(image="", text="无法打开图片: %s" % exc,
                                 fg="#ff8888",
                                 font=(FONT_FAMILY, 16))
        self.info_var.set("%d / %d    %s    ← → 切换   Esc 返回"
                          % (self.index + 1, len(self.pics),
                             item.get("disp", item["name"])))

    def close(self):
        try:
            self.top.destroy()
        except tk.TclError:
            pass
        self.app._viewer = None
        self.app.bind_main_keys()
        self.app.root.after(60, lambda: bring_to_front(self.app.root))


# ----------------------------------------------------------------------
def main():
    if IS_WINDOWS:
        # Windows 高 DPI 清晰显示（必须在创建 Tk 之前设置）
        try:
            from ctypes import windll
            windll.shcore.SetProcessDpiAwareness(1)
        except Exception:
            pass
    root = tk.Tk()
    MediaApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
