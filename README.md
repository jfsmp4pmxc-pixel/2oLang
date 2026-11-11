# 2oLang

Trình biên dịch cho ngôn ngữ lập trình **2oLang**: đọc file `.2o` và **sinh thẳng mã máy** (không qua C, assembler hay linker ngoài, không cần libc). Trình biên dịch viết bằng C++17.

```Bash
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

**File được biên dịch** (file chính) **bắt buộc** có ba khối theo thứ tự tuỳ ý: `global { }`, `var { }`, `entry { }`. File được `#import` thì không bắt buộc khối nào, thường chỉ cần `var { }` kèm các `define`.

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
- Gán vào tên chưa khai báo trong hàm/entry sẽ tạo biến cục bộ; nếu tên đã có (xem thứ tự tìm biến ở mục *Liên kết*) thì gán vào biến đó. Vì vậy `a = a + 1;` trong entry cộng thêm 1 vào biến `a` đã khai báo.
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
input ten;                    // đọc một dòng vào biến ten (kết thúc khi gặp enter); chạy lại thì đọc dòng kế tiếp
input = ten;                  // cách viết khác của dòng trên
if (input alt + f4) { ... }   // chờ một phím/tổ hợp phím, ra true nếu khớp
```

Phím hỗ trợ: chữ cái, số, `enter esc tab space backspace delete home end up down left right`, `f1`..`f12`, kết hợp với `ctrl`, `alt`, `shift` bằng dấu `+`.

`input ten;` (và `ten = input;`) dùng được trong vòng lặp để nhập nhiều dòng liên tiếp. Khi hết dữ liệu (stdin đóng, ví dụ nhập từ file hoặc Ctrl+D) thì `ten` nhận `nil`, so sánh `if (ten == nil)` để thoát vòng lặp. Lưu ý: `input ten;` với `ten` là biến luôn có nghĩa là đọc vào `ten`; muốn so sánh với một chuỗi thì viết `input "help"`.

### Lệnh hệ thống

Nhóm lệnh `system.*` làm việc với console. Lệnh không có tham số viết liền, không cần ngoặc.

```
system.console.clear         // dọn màn hình console (và xoá nhật ký console)
system.console.save          // lưu console hiện tại
system.console.load          // in toàn bộ console đã lưu
system.console.size          // [số dòng, số cột] của cửa sổ; .y là số dòng, .x là số cột
system.cursor.pos            // vị trí con trỏ nhấp nháy: [y, x] (dòng, cột, tính từ 1); .y và .x lấy từng phần
system.cursor.move(y, x)     // đặt con trỏ tới dòng y, cột x (tính từ 1)
system.cursor.hide           // ẩn con trỏ
system.cursor.show           // hiện lại con trỏ
```

```
system.console.save
msg "tạm thời\n";
system.console.clear
system.console.load          // in lại đúng những gì đã in trước khi save
msg "dòng " + system.cursor.pos.y + ", cột " + system.cursor.pos.x + "\n";
```

- **`save` / `load` làm việc trên nhật ký đầu ra:** terminal không cho chương trình đọc lại nội dung màn hình, nên 2oLang ghi lại mọi thứ `msg` đã in (kể cả màu), giữ tối đa 1 MB gần nhất. `save` chụp nhật ký đó, `load` in lại bản chụp, `clear` xoá nhật ký. Chữ do chương trình khác hay do người dùng gõ vào không nằm trong nhật ký.
- **Cần terminal thật:** `clear`, `move`, `hide`, `show` chỉ gửi mã điều khiển khi đầu ra là terminal (không in mã vào file hay ống). `cursor.pos` hỏi terminal bằng mã ANSI và chờ tối đa khoảng 0,2 giây; `size` đọc kích thước cửa sổ. Khi không phải terminal, `pos` và `size` trả `[0, 0]`.
- `system` là từ khoá, không đặt tên biến hay hàm là `system`.

### Liên kết

2oLang không có thư viện chuẩn: tự viết hoặc tải về rồi nạp bằng `#import`. Đường dẫn tính từ thư mục của file đang import (rồi đến thư mục hiện tại).

```
#import "scr/shell.2o"
```

Chỉ file được biên dịch mới cần `entry`; file được import có thể chỉ có `var` và `define` (khối `entry` trong file import, nếu có, bị bỏ qua). Hàm, `obj` và `global` dùng chung cho mọi file.

**Thứ tự tìm biến** khi gặp một tên: biến cục bộ/tham số, rồi `var` của chính file đang chạy, rồi `global`, cuối cùng là `var` của các file mà nó `#import` (trực tiếp hoặc gián tiếp). Ví dụ entry gọi hàm `tang()` nằm trong file import, rồi dùng biến `dem` không có ở file chính: tìm thấy `dem` trong `var` của file import. Nếu hai file import cùng khai báo một tên thì báo lỗi "mơ hồ" (đổi tên một trong hai, hoặc khai báo tên đó ở file chính). Hàm trong file import chỉ thấy biến của chính nó và của các file nó import, không thấy biến của file chính.

## Thư viện chuẩn (`Lib/`)

Giống `<iostream>` / `<stdio.h>` của C/C++: thư mục [`Lib/`](Lib) chứa các thư viện `.2o` (lấy từ
[Twoo-Studio/2oLang_Objs](https://github.com/Twoo-Studio/2oLang_Objs)), được **nhúng thẳng vào trình biên dịch lúc build**
nên dùng được ở mọi nơi mà không cần đi kèm file nào. Gọi bằng dấu `< >` thay vì đường dẫn:

```
#import <CioStyle>

global {}
var {}
entry {
    printf("xin chao\n");
    exit;
}
```

| Thư viện | Nội dung |
|---|---|
| `CioStyle` | `printf(s)` - in chuỗi `s` ra màn hình |

- `./2oLang lib` liệt kê thư viện (nguồn: thư mục cài đặt hay nhúng sẵn).
- `./2oLang lib update` tải/cập nhật thư viện mới nhất từ [2oLang_Objs](https://github.com/Twoo-Studio/2oLang_Objs)
  vào `~/.2oLang/lib` (hoặc `$TWOO_HOME/lib`) - **không cần build lại** 2oLang. Cần `git`, hoặc `curl`/`wget` + `tar`.
- `./2oLang --list-libs` chỉ in tên các thư viện nhúng sẵn.
- Thứ tự tìm `#import <Tên>`: `-L <thư mục>` (lặp được) -> biến môi trường `TWOO_LIB` (cách nhau bằng `:`) -> `~/.2oLang/lib` (do `lib update`) -> thư viện nhúng sẵn.
  Nhờ vậy bạn có thể ghi đè hoặc thêm thư viện riêng mà không cần build lại.
- Thêm thư viện chuẩn mới: bỏ file `Tên.2o` vào `Lib/` rồi `make` (tự nhúng lại).
- `#import "đường/dẫn.2o"` vẫn dùng như cũ cho file của riêng bạn.

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
