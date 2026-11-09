R"2o(
// ===== chuyển giá trị thành chuỗi =====

// ghi v với ít nhất wd chữ số thập phân vào buf+pos, trả vị trí mới
define rt_putn(buf, pos, v, wd) {
    n = 1
    t = v
    while (t >= 10) {
        t = t / 10
        n = n + 1
    }
    z = 0
    if (wd > n) { z = wd - n }
    i = 0
    while (i < z) {
        rt_store8(buf + pos + i, 48)
        i = i + 1
    }
    i = n - 1
    while (i >= 0) {
        rt_store8(buf + pos + z + i, 48 + v % 10)
        v = v / 10
        i = i - 1
    }
    return pos + z + n
}

define rt_itoa(x) {
    neg = 0
    if (x < 0) {
        neg = 1
        x = 0 - x
    }
    n = 1
    t = x
    while (t >= 10) {
        t = t / 10
        n = n + 1
    }
    s = rt_strnew(n + neg)
    d = rt_sdata(s)
    if (neg == 1) { rt_store8(d, 45) }
    rt_putn(d, neg, x, 0)
    return s
}

// Định dạng số thực dương < 1e15 nằm ở ô 8 với nfrac chữ số thập phân (làm tròn, bỏ số 0 thừa)
define rt_ffmt(neg, nfrac) {
    rt_copy8(rt_cell(14), rt_cell(8))
    rt_fround(rt_cell(15), rt_cell(14), 1)
    rt_i2f(rt_cell(12), 10)
    rt_i2f(rt_cell(13), 1000000000)
    rt_fop(3, rt_cell(17), rt_cell(15), rt_cell(13))
    rt_fround(rt_cell(17), rt_cell(17), 1)
    hi = rt_f2i(rt_cell(17))
    rt_fop(2, rt_cell(18), rt_cell(17), rt_cell(13))
    rt_fop(1, rt_cell(18), rt_cell(15), rt_cell(18))
    lo = rt_f2i(rt_cell(18))
    buf = rt_alloc(64)
    rt_store8(buf, 48)
    pos = 1
    if (hi > 0) {
        pos = rt_putn(buf, pos, hi, 0)
        pos = rt_putn(buf, pos, lo, 9)
    } else {
        pos = rt_putn(buf, pos, lo, 0)
    }
    ni = pos - 1
    rt_fop(1, rt_cell(19), rt_cell(14), rt_cell(15))
    k = 0
    while (k <= nfrac) {
        rt_fop(2, rt_cell(19), rt_cell(19), rt_cell(12))
        rt_fround(rt_cell(20), rt_cell(19), 1)
        dg = rt_f2i(rt_cell(20))
        if (dg > 9) { dg = 9 }
        if (dg < 0) { dg = 0 }
        rt_store8(buf + pos + k, 48 + dg)
        rt_fop(1, rt_cell(19), rt_cell(19), rt_cell(20))
        k = k + 1
    }
    look = 1 + ni + nfrac
    if (rt_load8(buf + look) >= 53) {
        j = look - 1
        going = 1
        while (j >= 0 and going == 1) {
            c = rt_load8(buf + j)
            if (c == 57) {
                rt_store8(buf + j, 48)
                j = j - 1
            } else {
                rt_store8(buf + j, c + 1)
                going = 0
            }
        }
    }
    s0 = 1
    if (rt_load8(buf) == 49) { s0 = 0 }
    nf = nfrac
    while (nf > 0 and rt_load8(buf + ni + nf) == 48) { nf = nf - 1 }
    fc = nf
    if (fc == 0) { fc = 1 }
    total = neg + (ni + 1 - s0) + 1 + fc
    s = rt_strnew(total)
    d = rt_sdata(s)
    q = 0
    if (neg == 1) {
        rt_store8(d, 45)
        q = 1
    }
    rt_memcpy(d + q, buf + s0, ni + 1 - s0)
    q = q + ni + 1 - s0
    rt_store8(d + q, 46)
    q = q + 1
    if (nf == 0) {
        rt_store8(d + q, 48)
    } else {
        rt_memcpy(d + q, buf + 1 + ni, nf)
    }
    return s
}

define rt_ftoa(v) {
    rt_copy8(rt_cell(8), v + 8)
    if (rt_fcmp(rt_cell(8), rt_cell(8)) == 3) { return rt_cs("nan") }
    neg = 0
    if (rt_fcmp(rt_cell(8), rt_cell(11)) == 1) {
        neg = 1
        rt_fop(1, rt_cell(8), rt_cell(11), rt_cell(8))
    }
    rt_i2f(rt_cell(21), 100000)
    rt_fop(2, rt_cell(21), rt_cell(21), rt_cell(21))
    rt_i2f(rt_cell(22), 100000)
    rt_fop(2, rt_cell(21), rt_cell(21), rt_cell(22))
    rt_i2f(rt_cell(23), 1000000)
    rt_i2f(rt_cell(24), 1)
    rt_fop(3, rt_cell(23), rt_cell(24), rt_cell(23))
    big = 0
    if (rt_fcmp(rt_cell(8), rt_cell(21)) == 2 or rt_fcmp(rt_cell(8), rt_cell(21)) == 0) { big = 1 }
    small = 0
    if (rt_fcmp(rt_cell(8), rt_cell(11)) == 2 and rt_fcmp(rt_cell(8), rt_cell(23)) == 1) { small = 1 }
    if (big == 0 and small == 0) { return rt_ffmt(neg, 15) }
    rt_i2f(rt_cell(12), 10)
    e = 0
    if (big == 1) {
        while (rt_fcmp(rt_cell(8), rt_cell(12)) != 1 and e < 400) {
            rt_fop(3, rt_cell(8), rt_cell(8), rt_cell(12))
            e = e + 1
        }
        if (e >= 400) {
            if (neg == 1) { return rt_cs("-inf") }
            return rt_cs("inf")
        }
    } else {
        while (rt_fcmp(rt_cell(8), rt_cell(24)) == 1 and e < 400) {
            rt_fop(2, rt_cell(8), rt_cell(8), rt_cell(12))
            e = e + 1
        }
    }
    m = rt_ffmt(neg, 9)
    if (big == 1) { m = rt_scat(m, rt_cs("e+")) }
    else { m = rt_scat(m, rt_cs("e-")) }
    return rt_scat(m, rt_itoa(e))
}

define rt_tostr(v) {
    t = rt_type(v)
    if (t == 0) { return rt_cs("nil") }
    if (t == 1) { return rt_itoa((v - 1) / 2) }
    if (t == 2) { return rt_ftoa(v) }
    if (t == 3) { return v }
    if (t == 5) {
        if (v == 4) { return rt_cs("true") }
        return rt_cs("false")
    }
    w = rt_wordsize()
    n = rt_load(v + w)
    items = rt_load(v + 3 * w)
    r = rt_cs("[")
    i = 0
    while (i < n) {
        if (i > 0) { r = rt_scat(r, rt_cs(", ")) }
        r = rt_scat(r, rt_tostr(rt_load(items + i * w)))
        i = i + 1
    }
    return rt_scat(r, rt_cs("]"))
}

// ===== độ dài, chỉ số, drive =====
define rt_clen(b) {
    if (b < 128) { return 1 }
    if (b < 224) { return 2 }
    if (b < 240) { return 3 }
    return 4
}

define rt_nchars(s) {
    d = rt_sdata(s)
    n = rt_slen(s)
    i = 0
    c = 0
    while (i < n) {
        i = i + rt_clen(rt_load8(d + i))
        c = c + 1
    }
    return c
}

define rt_len(v) {
    t = rt_type(v)
    if (t == 3) { return rt_nchars(v) * 2 + 1 }
    if (t == 4) { return rt_load(v + rt_wordsize()) * 2 + 1 }
    if (t == 0) { return 1 }
    return rt_nchars(rt_tostr(v)) * 2 + 1
}

define rt_element(v) {
    if (rt_type(v) == 4) { return rt_load(v + rt_wordsize()) * 2 + 1 }
    return rt_len(v)
}

define rt_drive_new() {
    w = rt_wordsize()
    p = rt_alloc(4 * w)
    rt_store(p, 3)
    rt_store(p + w, 0)
    rt_store(p + 2 * w, 4)
    rt_store(p + 3 * w, rt_alloc(4 * w))
    return p
}

define rt_check_drive(d) {
    if (rt_type(d) != 4) { rt_die("lỗi: giá trị này không phải là drive") }
    return 0
}

define rt_drive_push(d, v) {
    rt_check_drive(d)
    w = rt_wordsize()
    n = rt_load(d + w)
    cap = rt_load(d + 2 * w)
    items = rt_load(d + 3 * w)
    if (n == cap) {
        cap = cap * 2
        ni = rt_alloc(cap * w)
        rt_memcpy(ni, items, n * w)
        items = ni
        rt_store(d + 2 * w, cap)
        rt_store(d + 3 * w, items)
    }
    rt_store(items + n * w, v)
    rt_store(d + w, n + 1)
    return d
}

// chỉ số tính từ 1
define rt_drive_add(d, v, pos) {
    rt_check_drive(d)
    w = rt_wordsize()
    n = rt_load(d + w)
    p = n + 1
    if (pos != 0) {
        if (rt_type(pos) != 1) { rt_die("lỗi: vị trí chèn vào drive phải là số nguyên") }
        p = (pos - 1) / 2
    }
    if (p < 1 or p > n + 1) { rt_die("lỗi: vị trí chèn vào drive nằm ngoài phạm vi") }
    rt_drive_push(d, v)
    items = rt_load(d + 3 * w)
    i = n
    while (i > p - 1) {
        rt_store(items + i * w, rt_load(items + (i - 1) * w))
        i = i - 1
    }
    rt_store(items + (p - 1) * w, v)
    return d
}

define rt_drive_rm(d, x) {
    rt_check_drive(d)
    w = rt_wordsize()
    n = rt_load(d + w)
    items = rt_load(d + 3 * w)
    idx = 0 - 1
    if (rt_type(x) == 1) {
        idx = (x - 1) / 2 - 1
        if (idx < 0 or idx >= n) { rt_die("lỗi: số thứ tự phần tử cần xoá nằm ngoài drive") }
    } else {
        i = 0
        while (i < n and idx < 0) {
            if (rt_compare(rt_load(items + i * w), x) == 0) { idx = i }
            i = i + 1
        }
    }
    if (idx >= 0) {
        i = idx
        while (i < n - 1) {
            rt_store(items + i * w, rt_load(items + (i + 1) * w))
            i = i + 1
        }
        rt_store(d + w, n - 1)
    }
    return d
}

define rt_index(d, k) {
    t = rt_type(d)
    if (rt_type(k) != 1) { rt_die("lỗi: chỉ số phải là số nguyên") }
    i = (k - 1) / 2
    w = rt_wordsize()
    if (t == 4) {
        if (i < 1 or i > rt_load(d + w)) { rt_die("lỗi: chỉ số nằm ngoài phạm vi drive") }
        return rt_load(rt_load(d + 3 * w) + (i - 1) * w)
    }
    if (t == 3) {
        if (i < 1) { rt_die("lỗi: chỉ số nằm ngoài phạm vi chuỗi") }
        da = rt_sdata(d)
        n = rt_slen(d)
        p = 0
        j = i - 1
        while (j > 0 and p < n) {
            p = p + rt_clen(rt_load8(da + p))
            j = j - 1
        }
        if (p >= n) { rt_die("lỗi: chỉ số nằm ngoài phạm vi chuỗi") }
        cl = rt_clen(rt_load8(da + p))
        if (p + cl > n) { cl = n - p }
        r = rt_strnew(cl)
        rt_memcpy(rt_sdata(r), da + p, cl)
        return r
    }
    rt_die("lỗi: chỉ truy xuất phần tử [ ] cho drive hoặc chuỗi")
    return 0
}

define rt_setindex(d, k, v) {
    rt_check_drive(d)
    if (rt_type(k) != 1) { rt_die("lỗi: chỉ số phải là số nguyên") }
    i = (k - 1) / 2
    w = rt_wordsize()
    n = rt_load(d + w)
    if (i == n + 1) {
        rt_drive_push(d, v)
        return v
    }
    if (i < 1 or i > n) { rt_die("lỗi: chỉ số nằm ngoài phạm vi drive") }
    rt_store(rt_load(d + 3 * w) + (i - 1) * w, v)
    return v
}

// ===== xuất / nhập =====
define rt_isatty() {
    c = rt_load(rt_cell(3))
    if (c == 0) {
        buf = rt_alloc(128)
        r = rt_sys(rt_sysno(3), 1, rt_const(0), buf)
        c = 2
        if (r == 0) { c = 1 }
        rt_store(rt_cell(3), c)
    }
    return c == 1
}

define rt_msg(v, c) {
    s = rt_tostr(v)
    col = 0
    if (c != 1) { col = rt_isatty() }
    if (col == 1) {
        rt_write(1, "\x1b[", 2)
        n = rt_itoa((c - 1) / 2)
        rt_write(1, rt_sdata(n), rt_slen(n))
        rt_write(1, "m", 1)
    }
    rt_write(1, rt_sdata(s), rt_slen(s))
    if (col == 1) { rt_write(1, "\x1b[0m", 4) }
    return 0
}

define rt_input_line() {
    buf = rt_alloc(4096)
    n = 0
    going = 1
    while (going == 1 and n < 4095) {
        r = rt_sys(rt_sysno(0), 0, buf + n, 1)
        if (r <= 0) { going = 0 }
        else {
            if (rt_load8(buf + n) == 10) { going = 0 }
            else { n = n + 1 }
        }
    }
    if (n > 0) {
        if (rt_load8(buf + n - 1) == 13) { n = n - 1 }
    }
    s = rt_strnew(n)
    rt_memcpy(rt_sdata(s), buf, n)
    return s
}

define rt_input_match(v) {
    return rt_bool(rt_compare(rt_input_line(), rt_tostr(v)) == 0)
}

// đọc một phím (hoặc tổ hợp phím) và so với dãy byte mong đợi
define rt_input_key(seq) {
    old = rt_alloc(128)
    cur = rt_alloc(128)
    got = rt_alloc(16)
    tty = rt_sys(rt_sysno(3), 0, rt_const(0), old)
    if (tty == 0) {
        rt_memcpy(cur, old, 128)
        lf = rt_load32(cur + rt_const(3))
        rt_store32(cur + rt_const(3), rt_and(lf, rt_xor(rt_const(2), 4294967295)))
        rt_store32(cur, rt_and(rt_load32(cur), rt_xor(rt_const(6), 4294967295)))
        rt_store8(cur + rt_const(4), 1)
        rt_store8(cur + rt_const(5), 0)
        rt_sys(rt_sysno(3), 0, rt_const(1), cur)
    }
    n = rt_sys(rt_sysno(0), 0, got, 1)
    if (n == 1) {
        m = 0
        if (tty == 0) {
            rt_store8(cur + rt_const(4), 0)
            rt_store8(cur + rt_const(5), 1)
            rt_sys(rt_sysno(3), 0, rt_const(1), cur)
            m = rt_sys(rt_sysno(0), 0, got + 1, 7)
        } else {
            if (rt_slen(seq) > 1) { m = rt_sys(rt_sysno(0), 0, got + 1, rt_slen(seq) - 1) }
        }
        if (m > 0) { n = n + m }
    }
    if (tty == 0) { rt_sys(rt_sysno(3), 0, rt_const(1), old) }
    ok = 0
    if (n == rt_slen(seq)) {
        ok = 1
        i = 0
        while (i < n) {
            if (rt_load8(got + i) != rt_load8(rt_sdata(seq) + i)) { ok = 0 }
            i = i + 1
        }
    }
    return rt_bool(ok)
}

// ===== số ngẫu nhiên =====
define rt_rand31() {
    s = rt_load(rt_cell(2))
    if (s == 0) {
        s = rt_rdtsc()
        if (s == 0) { s = 88172645 }
    }
    s = rt_xor(s, rt_and(rt_shl(s, 13), 4294967295))
    s = rt_xor(s, rt_shr(s, 17))
    s = rt_xor(s, rt_and(rt_shl(s, 5), 4294967295))
    rt_store(rt_cell(2), s)
    return rt_shr(s, 1)
}

define rt_random(a, b) {
    if (rt_type(a) != 1 or rt_type(b) != 1) { rt_die("lỗi: random(a to b) cần hai số nguyên") }
    x = (a - 1) / 2
    y = (b - 1) / 2
    if (x > y) {
        t = x
        x = y
        y = t
    }
    return rt_mkint(x + rt_rand31() % (y - x + 1))
}

define rt_random0() {
    rt_i2f(rt_cell(8), rt_rand31())
    rt_i2f(rt_cell(9), 1073741824)
    rt_i2f(rt_cell(12), 2)
    rt_fop(2, rt_cell(9), rt_cell(9), rt_cell(12))
    rt_fop(3, rt_cell(10), rt_cell(8), rt_cell(9))
    return rt_mkf(rt_cell(10))
}
)2o"
