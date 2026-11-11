#!/usr/bin/env python3
# Chạy chương trình trong một terminal giả (pty) rồi kiểm tra mã điều khiển nó in ra.
# Terminal giả đặt cửa sổ 30 dòng x 100 cột và trả lời truy vấn vị trí con trỏ bằng "dòng 5, cột 7".
# Dùng: python3 tests/pty/run_pty.py <trình_biên_dịch>
import fcntl, os, pty, select, struct, subprocess, sys, termios, time

compiler = sys.argv[1] if len(sys.argv) > 1 else "./2oLang"
src = os.path.join(os.path.dirname(os.path.abspath(__file__)), "system_tty.2o")
out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "out")
os.makedirs(out_dir, exist_ok=True)

def run(target):
    exe = os.path.join(out_dir, "system_tty." + target)
    r = subprocess.run([compiler, src, "-(%s)->" % target, exe], capture_output=True, text=True)
    if r.returncode != 0:
        return None, "biên dịch lỗi: " + r.stderr.strip().splitlines()[0]
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 100, 0, 0))
    p = subprocess.Popen([exe], stdin=slave, stdout=slave, stderr=slave, close_fds=True)
    os.close(slave)
    data, answered, deadline = b"", False, time.time() + 10
    while time.time() < deadline:
        ready, _, _ = select.select([master], [], [], 0.2)
        if ready:
            try:
                chunk = os.read(master, 4096)
            except OSError:
                break
            if not chunk:
                break
            data += chunk
            if not answered and b"\x1b[6n" in data:
                os.write(master, b"\x1b[5;7R")
                answered = True
        elif p.poll() is not None:
            break
    try:
        p.wait(timeout=5)
    except subprocess.TimeoutExpired:
        p.kill()
        return None, "chương trình không kết thúc"
    return data, None

fails = 0
for target in ("Alpine64bits", "Alpine32bits"):
    data, err = run(target)
    if err:
        print("  FAIL pty [%s] %s" % (target, err)); fails += 1; continue
    data = data.replace(b"\r\n", b"\n")  # terminal đổi \n thành \r\n khi hiển thị
    clear = b"\x1b[H\x1b[2J\x1b[3J"
    after_clear = data.split(clear, 1)[1] if clear in data else b""
    checks = [
        ("màu xanh khi là terminal", b"\x1b[32mdong 1\n\x1b[0m" in data),
        ("vị trí con trỏ do terminal trả lời", b"pos=[5, 7]" in data),
        ("kích thước cửa sổ", b"size=[30, 100]" in data),
        ("cursor.move", b"\x1b[2;5H" in data),
        ("cursor.hide", b"\x1b[?25l" in data),
        ("cursor.show", b"\x1b[?25h" in data),
        ("console.clear", clear in data),
        ("console.load in lại bản đã lưu sau khi clear", b"dong 1" in after_clear and b"pos=" not in after_clear),
    ]
    for name, ok in checks:
        if ok:
            print("  ok   pty [%s] %s" % (target, name))
        else:
            print("  FAIL pty [%s] %s" % (target, name)); fails += 1
    if fails:
        print("    đầu ra thô:", data)
sys.exit(1 if fails else 0)
