R"2o(
// ===== Runtime 2oLang (phương ngữ raw: số nguyên thô, không gắn thẻ) =====
// Bố cục đối tượng heap (w = kích thước từ máy):
//   float : [1][pad][double ở +8]      string: [2][số byte][byte...][0]      drive: [3][count][cap][items]
// Giá trị: nil=0, int=(n*2+1), false=2, true=4, còn lại là con trỏ heap.
// Ô runtime: 0 con trỏ heap, 1 giới hạn heap, 2 trạng thái random, 3 isatty(stdout)
//   8..24: ô double tạm (11 luôn bằng 0.0)

define rt_write(fd, p, n) {
    while (n > 0) {
        r = rt_sys(rt_sysno(1), fd, p, n)
        if (r <= 0) { return r }
        p = p + r
        n = n - r
    }
    return 0
}

define rt_cstrlen(p) {
    n = 0
    while (rt_load8(p + n) != 0) { n = n + 1 }
    return n
}

define rt_die(m) {
    rt_write(2, m, rt_cstrlen(m))
    rt_write(2, "\n", 1)
    rt_sys(rt_sysno(2), 1, 0, 0)
    return 0
}

define rt_alloc(n) {
    n = (n + 7) / 8 * 8
    p = rt_load(rt_cell(0))
    e = p + n
    if (e > rt_load(rt_cell(1))) { rt_die("lỗi: hết bộ nhớ") }
    rt_store(rt_cell(0), e)
    return p
}

define rt_memcpy(d, s, n) {
    i = 0
    while (i < n) {
        rt_store8(d + i, rt_load8(s + i))
        i = i + 1
    }
    return d
}

define rt_copy8(d, s) {
    rt_store32(d, rt_load32(s))
    rt_store32(d + 4, rt_load32(s + 4))
    return 0
}

define rt_exit(v) {
    c = 0
    if (rt_and(v, 1) == 1) { c = (v - 1) / 2 }
    rt_sys(rt_sysno(2), c, 0, 0)
    return 0
}

// 0 nil, 1 int, 2 float, 3 string, 4 drive, 5 bool
define rt_type(v) {
    if (v == 0) { return 0 }
    if (rt_and(v, 1) == 1) { return 1 }
    if (v == 2) { return 5 }
    if (v == 4) { return 5 }
    return rt_load(v) + 1
}

define rt_bool(x) {
    if (x != 0) { return 4 }
    return 2
}

// ----- tạo đối tượng -----
define rt_newf() {
    p = rt_alloc(16)
    rt_store(p, 1)
    return p
}

define rt_mkf(src) {
    p = rt_newf()
    rt_copy8(p + 8, src)
    return p
}

define rt_mkint(x) {
    if (x > rt_intmax()) {
        p = rt_newf()
        rt_i2f(p + 8, x)
        return p
    }
    if (x < 0 - rt_intmax() - 1) {
        p = rt_newf()
        rt_i2f(p + 8, x)
        return p
    }
    return x * 2 + 1
}

define rt_strnew(n) {
    w = rt_wordsize()
    p = rt_alloc(2 * w + n + 1)
    rt_store(p, 2)
    rt_store(p + w, n)
    rt_store8(p + 2 * w + n, 0)
    return p
}

define rt_sdata(s) { return s + 2 * rt_wordsize() }
define rt_slen(s) { return rt_load(s + rt_wordsize()) }

define rt_cs(c) {
    n = rt_cstrlen(c)
    s = rt_strnew(n)
    rt_memcpy(rt_sdata(s), c, n)
    return s
}

define rt_scat(a, b) {
    la = rt_slen(a)
    lb = rt_slen(b)
    r = rt_strnew(la + lb)
    rt_memcpy(rt_sdata(r), rt_sdata(a), la)
    rt_memcpy(rt_sdata(r) + la, rt_sdata(b), lb)
    return r
}

// ----- chuyển sang số thực trong ô dst; trả 0 nếu không phải số -----
define rt_tonum(v, dst) {
    t = rt_type(v)
    if (t == 1) {
        rt_i2f(dst, (v - 1) / 2)
        return 1
    }
    if (t == 2) {
        rt_copy8(dst, v + 8)
        return 1
    }
    return 0
}

define rt_mulint(x, y) {
    if (x == 0 or y == 0) { return 1 }
    r = x * y
    if (r / x != y) {
        rt_i2f(rt_cell(8), x)
        rt_i2f(rt_cell(9), y)
        rt_fop(2, rt_cell(10), rt_cell(8), rt_cell(9))
        return rt_mkf(rt_cell(10))
    }
    return rt_mkint(r)
}

define rt_numerr() {
    rt_die("lỗi: phép toán không hợp lệ với kiểu dữ liệu này")
    return 0
}

// op: 0 cộng, 1 trừ, 2 nhân, 3 chia
define rt_arith(op, a, b) {
    if (rt_type(a) == 1 and rt_type(b) == 1) {
        x = (a - 1) / 2
        y = (b - 1) / 2
        if (op == 0) { return rt_mkint(x + y) }
        if (op == 1) { return rt_mkint(x - y) }
        if (op == 2) { return rt_mulint(x, y) }
        if (y == 0) { rt_die("lỗi: chia cho 0") }
        if (x % y == 0) { return rt_mkint(x / y) }
    }
    if (rt_tonum(a, rt_cell(8)) == 0) { rt_numerr() }
    if (rt_tonum(b, rt_cell(9)) == 0) { rt_numerr() }
    if (op == 3) {
        if (rt_fcmp(rt_cell(9), rt_cell(11)) == 0) { rt_die("lỗi: chia cho 0") }
    }
    if (op == 0) { rt_fop(0, rt_cell(10), rt_cell(8), rt_cell(9)) }
    if (op == 1) { rt_fop(1, rt_cell(10), rt_cell(8), rt_cell(9)) }
    if (op == 2) { rt_fop(2, rt_cell(10), rt_cell(8), rt_cell(9)) }
    if (op == 3) { rt_fop(3, rt_cell(10), rt_cell(8), rt_cell(9)) }
    return rt_mkf(rt_cell(10))
}

define rt_cat(a, b) { return rt_scat(rt_tostr(a), rt_tostr(b)) }

define rt_add(a, b) {
    if (rt_type(a) == 3 or rt_type(b) == 3) { return rt_cat(a, b) }
    return rt_arith(0, a, b)
}
define rt_sub(a, b) { return rt_arith(1, a, b) }
define rt_mul(a, b) { return rt_arith(2, a, b) }
define rt_div(a, b) { return rt_arith(3, a, b) }

define rt_mod(a, b) {
    if (rt_type(a) == 1 and rt_type(b) == 1) {
        y = (b - 1) / 2
        if (y == 0) { rt_die("lỗi: chia cho 0 (phép %)") }
        return rt_mkint(((a - 1) / 2) % y)
    }
    if (rt_tonum(a, rt_cell(8)) == 0) { rt_numerr() }
    if (rt_tonum(b, rt_cell(9)) == 0) { rt_numerr() }
    if (rt_fcmp(rt_cell(9), rt_cell(11)) == 0) { rt_die("lỗi: chia cho 0 (phép %)") }
    rt_fop(3, rt_cell(10), rt_cell(8), rt_cell(9))
    rt_fround(rt_cell(10), rt_cell(10), 1)
    rt_fop(2, rt_cell(10), rt_cell(10), rt_cell(9))
    rt_fop(1, rt_cell(10), rt_cell(8), rt_cell(10))
    return rt_mkf(rt_cell(10))
}

define rt_neg(v) {
    t = rt_type(v)
    if (t == 1) { return rt_mkint(0 - (v - 1) / 2) }
    if (t == 2) {
        rt_copy8(rt_cell(8), v + 8)
        rt_fop(1, rt_cell(10), rt_cell(11), rt_cell(8))
        return rt_mkf(rt_cell(10))
    }
    rt_numerr()
    return 0
}

define rt_pow(a, b) {
    if (rt_type(a) == 1 and rt_type(b) == 1) {
        y = (b - 1) / 2
        if (y >= 0 and y <= 1100) {
            r = 3
            i = 0
            while (i < y) {
                r = rt_arith(2, r, a)
                i = i + 1
            }
            return r
        }
    }
    if (rt_tonum(a, rt_cell(8)) == 0) { rt_numerr() }
    if (rt_tonum(b, rt_cell(9)) == 0) { rt_numerr() }
    odd = 0
    neg = 0
    c = rt_fcmp(rt_cell(8), rt_cell(11))
    if (c == 0) {
        if (rt_fcmp(rt_cell(9), rt_cell(11)) == 0) { return 3 }
        if (rt_fcmp(rt_cell(9), rt_cell(11)) == 1) { rt_die("lỗi: chia cho 0 (luỹ thừa âm của 0)") }
        return rt_mkf(rt_cell(11))
    }
    if (c == 1) {
        rt_fround(rt_cell(12), rt_cell(9), 3)
        if (rt_fcmp(rt_cell(12), rt_cell(9)) != 0) { rt_die("lỗi: luỹ thừa của số âm với số mũ không nguyên") }
        rt_i2f(rt_cell(13), 2)
        rt_fop(3, rt_cell(14), rt_cell(9), rt_cell(13))
        rt_fround(rt_cell(14), rt_cell(14), 3)
        rt_fop(2, rt_cell(14), rt_cell(14), rt_cell(13))
        if (rt_fcmp(rt_cell(14), rt_cell(9)) != 0) { odd = 1 }
        neg = 1
        rt_fop(1, rt_cell(8), rt_cell(11), rt_cell(8))
    }
    rt_fpow(rt_cell(10), rt_cell(8), rt_cell(9))
    if (neg == 1 and odd == 1) { rt_fop(1, rt_cell(10), rt_cell(11), rt_cell(10)) }
    return rt_mkf(rt_cell(10))
}

// ----- so sánh: -1 bé, 0 bằng, 1 lớn, 2 không so sánh được -----
define rt_compare(a, b) {
    ta = rt_type(a)
    tb = rt_type(b)
    if (ta == 1 and tb == 1) {
        if (a < b) { return 0 - 1 }
        if (a > b) { return 1 }
        return 0
    }
    if ((ta == 1 or ta == 2) and (tb == 1 or tb == 2)) {
        rt_tonum(a, rt_cell(8))
        rt_tonum(b, rt_cell(9))
        c = rt_fcmp(rt_cell(8), rt_cell(9))
        if (c == 0) { return 0 }
        if (c == 1) { return 0 - 1 }
        if (c == 2) { return 1 }
        return 2
    }
    if (ta == 0 and tb == 0) { return 0 }
    if (ta == 5 and tb == 5) {
        if (a < b) { return 0 - 1 }
        if (a > b) { return 1 }
        return 0
    }
    if (ta == 3 and tb == 3) {
        la = rt_slen(a)
        lb = rt_slen(b)
        n = la
        if (lb < n) { n = lb }
        da = rt_sdata(a)
        db2 = rt_sdata(b)
        i = 0
        while (i < n) {
            x = rt_load8(da + i)
            y = rt_load8(db2 + i)
            if (x < y) { return 0 - 1 }
            if (x > y) { return 1 }
            i = i + 1
        }
        if (la < lb) { return 0 - 1 }
        if (la > lb) { return 1 }
        return 0
    }
    if (ta == 4 and tb == 4) {
        w = rt_wordsize()
        n = rt_load(a + w)
        if (n != rt_load(b + w)) { return 2 }
        ia = rt_load(a + 3 * w)
        ib = rt_load(b + 3 * w)
        i = 0
        while (i < n) {
            c = rt_compare(rt_load(ia + i * w), rt_load(ib + i * w))
            if (c != 0) { return c }
            i = i + 1
        }
        return 0
    }
    return 2
}

define rt_eq(a, b) { return rt_bool(rt_compare(a, b) == 0) }
define rt_ne(a, b) { return rt_bool(rt_compare(a, b) != 0) }
define rt_lt(a, b) { return rt_bool(rt_compare(a, b) == 0 - 1) }
define rt_gt(a, b) { return rt_bool(rt_compare(a, b) == 1) }
define rt_le(a, b) {
    c = rt_compare(a, b)
    return rt_bool(c == 0 - 1 or c == 0)
}
define rt_ge(a, b) {
    c = rt_compare(a, b)
    return rt_bool(c == 1 or c == 0)
}

define rt_truthy(v) {
    t = rt_type(v)
    if (t == 0) { return 0 }
    if (t == 1) { return v != 1 }
    if (t == 2) {
        rt_copy8(rt_cell(8), v + 8)
        return rt_fcmp(rt_cell(8), rt_cell(11)) != 0
    }
    if (t == 3) { return rt_slen(v) != 0 }
    if (t == 4) { return rt_load(v + rt_wordsize()) != 0 }
    return v == 4
}

define rt_not(v) { return rt_bool(rt_truthy(v) == 0) }
)2o"
