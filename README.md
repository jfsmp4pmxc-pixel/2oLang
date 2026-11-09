# 2oLang

Trình biên dịch cho ngôn ngữ lập trình **2oLang**: đọc file `.2o` và **sinh thẳng mã máy** (không qua C, assembler hay linker ngoài, không cần libc). Trình biên dịch viết bằng C++17.

```
./2oLang <file đầu vào> '-(<mục tiêu>)->' <file đầu ra>
```

> Trong shell hãy đặt `'-(...)->'` trong dấu nháy, vì `( )` và `>` có nghĩa đặc biệt với shell.
> Cách viết tương đương: `./2oLang main.2o -t Alpine64bits -o main`

| Mục tiêu | Kết quả |
|---|---|
| `Alpine32bits` | ELF32 i386, chạy trên Alpine, Ubuntu và mọi Linux |
| `Alpine64bits` | ELF64 x86_64, chạy trên Alpine, Ubuntu và mọi Linux |
| `MacOS` | Mach-O x86_64 (xem mục *Giới hạn*) |
| `i386` | mã máy thô i386 (flat binary, syscall Linux), nạp tại `0x08048000` |
| `x86_64` | mã máy thô x86_64 (flat binary, syscall Linux), nạp tại `0x400000` |

Mã máy thô nằm liên tục: code, rồi data, rồi 256 MB heap (vùng nhớ này phải ghi được và đã được xoá về 0). Đổi địa chỉ nạp bằng `--org <địa chỉ>`.

## Build

```sh
make            # ra ./2oLang
make static     # build tĩnh (dùng cho Alpine/musl)
make test       # chạy bộ test
```

Cần `g++` hỗ trợ C++17 và `make`. CI (`.github/workflows/build.yml`) build và test trên Ubuntu, Alpine 64 bit và Alpine 32 bit (có thêm một job macOS không chặn pipeline).

## Ngôn ngữ

Mỗi chương trình **bắt buộc** có ba khối theo thứ tự tuỳ ý: `global { }`, `var { }`, `entry { }`.

```
global {          // biến toàn cục: KHÔNG dùng db
    int gcount = 0;
}

var {             // biến nội bộ: BẮT BUỘC dùng db
    db int a = 6;                       // số nguyên
    db float b = 7.3667;                // số thực
    db string name = "Jack";            // chuỗi
    db drive number = "1", "2", name, a; // ổ dữ liệu (mảng)
}

entry {           // điểm bắt đầu; trong hàm/entry bỏ cả db và kiểu dữ liệu
    x = a * 2;
    msg "x = " + x + "\n", green;
}
```

- `nil` là không có giá trị. `true`/`false` là giá trị logic.
- Dấu `;` có thể bỏ khi câu lệnh kết thúc bằng xuống dòng.
- Gán vào tên chưa khai báo trong hàm/entry sẽ tạo biến cục bộ; tên trùng `var`/`global` thì gán vào biến đó.
- Chú thích: `// ...` và `/* ... */`.

### Toán tử

| | |
|---|---|
| `+ - * /` `%` | Chia hết thì ra số nguyên, không thì ra số thực (lấy 15 chữ số sau dấu `.`, có làm tròn). Chia cho 0 báo lỗi lúc chạy. |
| `a^b` | Luỹ thừa (kết hợp phải). Số nguyên tràn tự chuyển sang số thực. |
| `== != < > <= >=` | So sánh. |
| `and` `or` `not` (`!`) | Logic, tính tắt (short-circuit). |
| `random(a to b)` | Số nguyên ngẫu nhiên trong đoạn `[a, b]` (gồm cả hai đầu). `random()` ra số thực trong `[0, 1)`. |

### Chuỗi

```
msg len ten;                  // độ dài (tính theo ký tự UTF-8)
msg "tên: " + ten ho + "\n";  // nối bằng + hoặc đặt cạnh nhau: ten ho, i"\n"
msg element foods;            // số phần tử của drive
```

`msg` in giá trị, có thể thêm màu: `msg "x", red;` (`black red green yellow blue magenta cyan white gray`). Màu chỉ được in ra khi đầu ra là terminal.

Escape trong chuỗi: `\n \t \r \0 \e \\ \" \xHH`.

### Điều kiện và vòng lặp

```
if (hp > 75) {msg "ổn\n", green;}
else if (hp > 1 and hp < 75) {msg "cẩn thận\n", yellow;}
else {msg "thua\n";}

while (i < 10) { i = i + 1; }   // sai điều kiện thì thoát
while () { ... break; ... continue; }   // không điều kiện, thoát bằng break
while (true) { ... }            // vô điều kiện, KHÔNG dùng break (báo lỗi); dùng exit/return
```

### Hàm, đối tượng, drive

```
define binh_phuong(x) { return x * x; }

obj Mike {
    public  { tuoi = 25; chieu_cao = 181; }
    private { tai_san = 15000; }
}
// Mike.tuoi, Mike.tai_san = 99;  -> chỉ entry mới được SỬA trường và truy xuất trường private.
// Hàm khác (ví dụ test()) đụng vào private hoặc sửa trường đều bị báo lỗi lúc biên dịch.

drive(a).add("chanh", 4);   // thêm vào vị trí 4 (bỏ vị trí thì thêm vào cuối)
drive(a).rm("táo");         // xoá theo giá trị
drive(a).rm(1);             // xoá phần tử số 1
a[2]                        // đọc phần tử; a[2] = "x" để gán
```

Chỉ số của drive và chuỗi **tính từ 1**, cho thống nhất với `add`/`rm`. `exit;` hoặc `exit 3;` thoát chương trình.

### Nhập

```
if (input "help") { ... }     // đọc một dòng, ra true nếu đúng là "help"
ten = input;                  // đọc một dòng, trả về chuỗi
if (input alt + f4) { ... }   // chờ một phím/tổ hợp phím, ra true nếu khớp
```

Phím hỗ trợ: chữ cái, số, `enter esc tab space backspace delete home end up down left right`, `f1`..`f12`, kết hợp với `ctrl`, `alt`, `shift` bằng dấu `+`.

### Liên kết

2oLang không có thư viện chuẩn: tự viết hoặc tải về rồi nạp bằng `#import`. Đường dẫn tính từ thư mục của file đang import (rồi đến thư mục hiện tại).

```
#import "scr/shell.2o"
```

File được import chỉ cần chứa `define`, `obj`, `global`, `var`; `entry` của nó bị bỏ qua. `var` của mỗi file là riêng của file đó, `global`, hàm và `obj` dùng chung.

## Báo lỗi

Trình biên dịch báo **toàn bộ lỗi trong một lần chạy** thay vì dừng ở lỗi đầu tiên. Mỗi lỗi có `file:dòng:cột`, dòng mã và dấu `^`, cuối cùng là tổng số lỗi; khi có lỗi thì không tạo file đầu ra.

- Lỗi cú pháp (ký tự lạ, chuỗi chưa đóng, thiếu `;`, `)`, `}` ...): parser bỏ câu lệnh hỏng rồi đọc tiếp từ ranh giới câu lệnh kế tiếp. Nếu có lỗi cú pháp thì dừng sau bước này.
- Lỗi ngữ nghĩa (biến/hàm chưa khai báo, sai số tham số, trùng tên, truy xuất `private`, sai kiểu ở giá trị viết thẳng ...): được gom cho cả chương trình, mỗi câu lệnh báo tối đa một lỗi.
- Một lỗi có thể kéo theo lỗi phụ (ví dụ khai báo hỏng thì chỗ dùng biến đó báo "chưa khai báo"); hãy sửa từ trên xuống. Tối đa 100 lỗi được in.

## Cách hoạt động

```
src/lexer.cpp, parser.cpp   chữ -> token -> AST
src/gen.cpp, genexpr.cpp    AST -> mã máy x86 (i386 / x86_64)
src/asm.hpp                 bộ mã hoá lệnh x86 dùng chung cho hai kiến trúc
src/image.cpp               ghi ELF32, ELF64, Mach-O, mã máy thô
src/rt_core.inc, rt_io.inc  runtime viết bằng chính 2oLang (phương ngữ raw), biên dịch kèm mọi chương trình
```

Giá trị lúc chạy là một từ máy: bit thấp bằng 1 là số nguyên, `0` là `nil`, còn lại là con trỏ tới đối tượng trên heap (số thực, chuỗi, drive). Runtime gọi thẳng syscall của hệ điều hành.

## Giới hạn cần biết

- **Số nguyên:** 63 bit trên x86_64, **31 bit trên i386** (vượt quá thì tự chuyển sang số thực). Ví dụ `20!` ra số nguyên ở 64 bit nhưng ra `2.432902008e+18` ở 32 bit. Số thực từ `1e15` trở lên in dạng khoa học.
- **Bộ nhớ:** heap 256 MB cấp phát kiểu bump, **không có bộ thu gom rác**. Vòng lặp tạo hàng chục triệu chuỗi sẽ báo `lỗi: hết bộ nhớ`.
- **macOS:** Mach-O được sinh theo đúng định dạng (`llvm-objdump` đọc được) nhưng **chưa chạy thử trên máy Mac thật**; job `macos` trong CI sẽ chạy thử. Chỉ x86_64 (Mac Apple Silicon chạy được qua Rosetta nếu hệ điều hành chấp nhận).
- **Mã máy thô** dùng syscall Linux và cần tự nạp vào bộ nhớ.
- Chưa có số thực kiểu `float` ngoài dạng `double`, chưa có ép kiểu tự động khi gán (kiểu khai báo chỉ được kiểm tra với giá trị viết thẳng).

## Test

`make test` biên dịch và chạy mọi chương trình trong `tests/cases` (cả 32 và 64 bit) rồi so với `.out`, kiểm tra các thông báo lỗi trong `tests/errors`, và kiểm tra các mục tiêu `i386`, `x86_64`, `MacOS` biên dịch được.

## Giấy phép

2oLang là phần mềm tự do, phát hành theo **Giấy phép Công cộng GNU phiên bản 3** (GPL-3.0-or-later). Toàn văn nằm trong file [LICENSE](LICENSE).

Giấy phép này áp dụng cho trình biên dịch. Chương trình `.2o` bạn viết và biên dịch ra vẫn thuộc về bạn; tuy vậy mỗi file thực thi đều chứa kèm runtime của 2oLang (`src/rt_core.inc`, `src/rt_io.inc`), nên hãy tự xem xét điều khoản GPL khi phân phối nó.
