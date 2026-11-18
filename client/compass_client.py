import argparse
import math
import os
import queue
import sys
import threading
import time
import tkinter as tk
from datetime import datetime
from tkinter import messagebox, ttk

import serial

BAUDRATE = 38400
PING_TIMEOUT_S = 2.0
DATA_TIMEOUT_S = 2.0
POLL_MS = 50
SCALE_MIN = 0
SCALE_MAX = 200
AXES = ("X", "Y", "Z")


class TrafficLog:
    def __init__(self, directory):
        os.makedirs(directory, exist_ok=True)
        name = datetime.now().strftime("compass_%Y%m%d_%H%M%S.log")
        self.path = os.path.join(directory, name)
        self._file = open(self.path, "a", encoding="utf-8")
        self._lock = threading.Lock()

    def write(self, direction, text):
        stamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
        line = f"{stamp} {direction} {text}"
        with self._lock:
            self._file.write(line + "\n")
            self._file.flush()
        return line

    def close(self):
        with self._lock:
            self._file.close()


class BoardLink:
    def __init__(self, port_name, log, serial_factory=serial.Serial):
        self.port_name = port_name
        self.log = log
        self.lines = queue.Queue()
        self._serial_factory = serial_factory
        self._port = None
        self._reader = None
        self._running = False

    @property
    def connected(self):
        return self._port is not None

    def open(self):
        self._port = self._serial_factory(self.port_name, BAUDRATE, timeout=0.1)
        self._running = True
        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()

    def close(self):
        self._running = False
        if self._reader is not None:
            self._reader.join(timeout=1.0)
        if self._port is not None:
            self._port.close()
            self._port = None

    def send(self, text):
        if self._port is None:
            return None
        self._port.write((text + "\n").encode("ascii"))
        return self.log.write("TX", text)

    def _read_loop(self):
        buffer = b""
        while self._running:
            try:
                chunk = self._port.read(256)
            except (serial.SerialException, OSError) as error:
                self.lines.put(("error", str(error)))
                return
            if not chunk:
                continue
            buffer += chunk
            while b"\n" in buffer:
                raw, buffer = buffer.split(b"\n", 1)
                text = raw.decode("ascii", errors="replace").strip()
                if text:
                    self.lines.put(("line", text, self.log.write("RX", text)))


def parse_message(text):
    parts = text.split()
    if not parts:
        return None
    kind = parts[0]
    if kind == "DATA" and len(parts) == 8:
        try:
            values = [int(p) for p in parts[1:]]
        except ValueError:
            return None
        return {"type": "DATA", "x": values[0], "y": values[1], "z": values[2],
                "dir": values[3], "scales": values[4:7]}
    if kind == "PONG":
        return {"type": "PONG", "info": " ".join(parts[1:])}
    if kind == "HELLO":
        return {"type": "HELLO", "info": " ".join(parts[1:])}
    if kind == "OK" and len(parts) == 5 and parts[1] == "SCALE":
        try:
            return {"type": "OK_SCALE", "scales": [int(p) for p in parts[2:5]]}
        except ValueError:
            return None
    if kind == "ERR":
        return {"type": "ERR", "reason": " ".join(parts[1:])}
    return None


class CompassWindow:
    def __init__(self, root, link):
        self.root = root
        self.link = link
        self.ping_sent_at = None
        self.board_alive = False
        self.last_data_at = None
        self.sensor_error = False

        root.title(f"Компас SDK-1.1M — {link.port_name}")
        root.minsize(820, 560)
        root.protocol("WM_DELETE_WINDOW", self.on_close)

        self._build_status()
        self._build_main()
        self._build_log()
        self.draw_compass(None)

    def _build_status(self):
        bar = ttk.Frame(self.root, padding=(10, 8))
        bar.pack(fill="x")
        self.status_dot = tk.Canvas(bar, width=14, height=14, highlightthickness=0)
        self.status_dot.pack(side="left")
        self.status_text = ttk.Label(bar, text="Подключение…", font=("Segoe UI", 11, "bold"))
        self.status_text.pack(side="left", padx=(6, 0))
        ttk.Button(bar, text="Пинг", command=self.send_ping).pack(side="right")
        ttk.Label(bar, text=f"Журнал: logs/{os.path.basename(self.link.log.path)}").pack(side="right", padx=10)
        self.set_status("Подключение…", "#d0a000")

    def _build_main(self):
        main = ttk.Frame(self.root, padding=(10, 0))
        main.pack(fill="both", expand=True)

        values = ttk.LabelFrame(main, text="Магнитное поле (на стенде)", padding=10)
        values.grid(row=0, column=0, sticky="nsew", padx=(0, 10))
        self.value_labels = {}
        for row, axis in enumerate(AXES):
            ttk.Label(values, text=f"{axis}:", font=("Consolas", 16)).grid(row=row, column=0, sticky="w")
            label = ttk.Label(values, text="—", font=("Consolas", 16, "bold"), width=6, anchor="e")
            label.grid(row=row, column=1, sticky="e")
            ttk.Label(values, text="мГс", font=("Consolas", 12)).grid(row=row, column=2, sticky="w", padx=(4, 0))
            self.value_labels[axis] = label
        ttk.Label(values, text="Направление:", font=("Segoe UI", 12)).grid(row=3, column=0, columnspan=2, sticky="w", pady=(12, 0))
        self.dir_label = ttk.Label(values, text="—", font=("Consolas", 13, "bold"))
        self.dir_label.grid(row=4, column=0, columnspan=3, sticky="w")
        ttk.Label(values, text="Коэффициенты на стенде:", font=("Segoe UI", 12)).grid(row=5, column=0, columnspan=3, sticky="w", pady=(12, 0))
        self.board_scales_label = ttk.Label(values, text="—", font=("Consolas", 11))
        self.board_scales_label.grid(row=6, column=0, columnspan=3, sticky="w")

        self.canvas = tk.Canvas(main, width=280, height=280, bg="#101418", highlightthickness=0)
        self.canvas.grid(row=0, column=1, padx=(0, 10))

        scales = ttk.LabelFrame(main, text="Масштабные коэффициенты, %", padding=10)
        scales.grid(row=0, column=2, sticky="nsew")
        self.scale_vars = {}
        validate = (self.root.register(self._validate_scale), "%P")
        for row, axis in enumerate(AXES):
            var = tk.IntVar(value=100)
            self.scale_vars[axis] = var
            ttk.Label(scales, text=f"K{axis.lower()}").grid(row=row, column=0, sticky="w")
            ttk.Scale(scales, from_=SCALE_MIN, to=SCALE_MAX, orient="horizontal", length=160,
                      command=lambda value, v=var: v.set(int(float(value))),
                      variable=var).grid(row=row, column=1, padx=6, pady=4)
            ttk.Spinbox(scales, from_=SCALE_MIN, to=SCALE_MAX, textvariable=var, width=5,
                        validate="key", validatecommand=validate).grid(row=row, column=2)
        ttk.Button(scales, text="Отправить на стенд", command=self.send_scales).grid(row=3, column=0, columnspan=3, sticky="ew", pady=(12, 4))
        ttk.Button(scales, text="Сбросить (100 %)", command=self.reset_scales).grid(row=4, column=0, columnspan=3, sticky="ew")
        self.scale_result = ttk.Label(scales, text="", wraplength=230)
        self.scale_result.grid(row=5, column=0, columnspan=3, sticky="w", pady=(8, 0))

        main.columnconfigure(0, weight=1)
        main.columnconfigure(2, weight=1)

    def _build_log(self):
        frame = ttk.LabelFrame(self.root, text="Обмен со стендом", padding=6)
        frame.pack(fill="both", expand=True, padx=10, pady=10)
        self.log_view = tk.Text(frame, height=9, font=("Consolas", 9), state="disabled")
        scroll = ttk.Scrollbar(frame, command=self.log_view.yview)
        self.log_view.configure(yscrollcommand=scroll.set)
        self.log_view.pack(side="left", fill="both", expand=True)
        scroll.pack(side="right", fill="y")

    @staticmethod
    def _validate_scale(text):
        if text == "":
            return True
        return text.isdigit() and SCALE_MIN <= int(text) <= SCALE_MAX

    def set_status(self, text, color):
        self.status_text.configure(text=text)
        self.status_dot.delete("all")
        self.status_dot.create_oval(1, 1, 13, 13, fill=color, outline=color)

    def append_log(self, line):
        if line is None:
            return
        self.log_view.configure(state="normal")
        self.log_view.insert("end", line + "\n")
        if int(self.log_view.index("end-1c").split(".")[0]) > 500:
            self.log_view.delete("1.0", "100.0")
        self.log_view.see("end")
        self.log_view.configure(state="disabled")

    def draw_compass(self, direction):
        c = self.canvas
        c.delete("all")
        cx, cy, r = 140, 140, 120
        c.create_oval(cx - r, cy - r, cx + r, cy + r, outline="#5a6470", width=2)
        c.create_text(cx, cy - r - 8, text="верх стенда", fill="#8a96a3", font=("Segoe UI", 8))
        for deg in range(0, 360, 30):
            a = math.radians(deg)
            x0, y0 = cx + (r - 8) * math.sin(a), cy - (r - 8) * math.cos(a)
            x1, y1 = cx + r * math.sin(a), cy - r * math.cos(a)
            c.create_line(x0, y0, x1, y1, fill="#5a6470", width=2)
        if direction is None or direction < 0:
            c.create_oval(cx - 5, cy - 5, cx + 5, cy + 5, outline="#8a96a3", width=2)
            return
        a = math.radians(direction)
        ux, uy = math.sin(a), -math.cos(a)
        tip = (cx + ux * (r - 14), cy + uy * (r - 14))
        tail = (cx - ux * 45, cy - uy * 45)
        c.create_line(*tail, *tip, fill="#e8ecef", width=4, arrow="last", arrowshape=(22, 26, 9))
        c.create_text(tip[0] + ux * 12, tip[1] + uy * 12, text="N", fill="#ff6b5b", font=("Segoe UI", 12, "bold"))
        c.create_oval(cx - 4, cy - 4, cx + 4, cy + 4, fill="#e8ecef", outline="")

    def send_ping(self):
        if not self.link.connected:
            return
        self.ping_sent_at = time.monotonic()
        self.board_alive = False
        self.set_status("Проверка связи (PING)…", "#d0a000")
        self.append_log(self.link.send("PING"))

    def read_scales(self):
        values = []
        for axis in AXES:
            try:
                value = int(self.scale_vars[axis].get())
            except (tk.TclError, ValueError):
                raise ValueError(f"K{axis.lower()}: введите число от {SCALE_MIN} до {SCALE_MAX}")
            if not SCALE_MIN <= value <= SCALE_MAX:
                raise ValueError(f"K{axis.lower()}: допустимо от {SCALE_MIN} до {SCALE_MAX}")
            values.append(value)
        return values

    def send_scales(self):
        try:
            values = self.read_scales()
        except ValueError as error:
            self.scale_result.configure(text=str(error), foreground="#c0392b")
            return
        self.scale_result.configure(text="Отправлено, ждём подтверждения…", foreground="")
        self.append_log(self.link.send("SCALE {} {} {}".format(*values)))

    def reset_scales(self):
        for axis in AXES:
            self.scale_vars[axis].set(100)
        self.send_scales()

    def handle_message(self, text):
        msg = parse_message(text)
        if msg is None:
            return
        kind = msg["type"]
        if kind in ("PONG", "HELLO"):
            if kind == "PONG":
                self.ping_sent_at = None
            self.board_alive = True
            self.set_status(f"Стенд отвечает: {msg['info']}", "#2e9d4f")
        elif kind == "DATA":
            self.last_data_at = time.monotonic()
            self.sensor_error = False
            for axis, key in zip(AXES, ("x", "y", "z")):
                self.value_labels[axis].configure(text=f"{msg[key]:d}")
            direction = msg["dir"]
            self.dir_label.configure(text=f"{direction}° от верха" if direction >= 0 else "не определено")
            self.board_scales_label.configure(text="  ".join(f"K{a.lower()}={v}%" for a, v in zip(AXES, msg["scales"])))
            self.draw_compass(direction)
            if self.board_alive:
                self.set_status("Стенд отвечает, данные поступают", "#2e9d4f")
        elif kind == "OK_SCALE":
            self.scale_result.configure(text="Стенд принял: " + ", ".join(f"{v}%" for v in msg["scales"]), foreground="#2e9d4f")
        elif kind == "ERR":
            if msg["reason"] == "SENSOR":
                self.sensor_error = True
                self.set_status("Стенд отвечает, но ошибка датчика LSM9DS", "#c0392b")
                for label in self.value_labels.values():
                    label.configure(text="—")
                self.dir_label.configure(text="ошибка датчика")
                self.draw_compass(None)
            else:
                self.scale_result.configure(text=f"Стенд ответил ошибкой: {msg['reason']}", foreground="#c0392b")

    def poll(self):
        while True:
            try:
                item = self.link.lines.get_nowait()
            except queue.Empty:
                break
            if item[0] == "error":
                self.set_status(f"Связь потеряна: {item[1]}", "#c0392b")
                self.append_log(self.link.log.write("ERR", item[1]))
                continue
            _, text, logged = item
            self.append_log(logged)
            self.handle_message(text)
        now = time.monotonic()
        if self.ping_sent_at is not None and now - self.ping_sent_at > PING_TIMEOUT_S:
            self.ping_sent_at = None
            self.set_status("Стенд не ответил на PING", "#c0392b")
        if self.board_alive and not self.sensor_error and self.last_data_at is not None and now - self.last_data_at > DATA_TIMEOUT_S:
            self.set_status("Нет данных от стенда", "#d0a000")
        self.root.after(POLL_MS, self.poll)

    def start(self):
        self.send_ping()
        self.root.after(POLL_MS, self.poll)

    def on_close(self):
        self.link.close()
        self.link.log.write("INFO", "closed")
        self.link.log.close()
        self.root.destroy()


def enable_dpi_awareness():
    if sys.platform != "win32":
        return
    try:
        import ctypes
        ctypes.windll.shcore.SetProcessDpiAwareness(1)
    except (AttributeError, OSError):
        pass


def main(argv=None, serial_factory=serial.Serial):
    parser = argparse.ArgumentParser(description="Клиент «Компас» для стенда SDK-1.1M")
    parser.add_argument("port", help="COM-порт стенда, например COM5")
    parser.add_argument("--log-dir", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "logs"))
    args = parser.parse_args(argv)

    enable_dpi_awareness()
    log = TrafficLog(args.log_dir)
    log.write("INFO", f"open {args.port} {BAUDRATE} 8N1")
    link = BoardLink(args.port, log, serial_factory)
    root = tk.Tk()
    try:
        link.open()
    except (serial.SerialException, OSError) as error:
        log.write("ERR", str(error))
        root.withdraw()
        messagebox.showerror("Компас", f"Не удалось открыть {args.port}:\n{error}")
        log.close()
        root.destroy()
        return 1
    window = CompassWindow(root, link)
    window.start()
    root.mainloop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
