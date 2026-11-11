#!/usr/bin/env bash
# Bộ test của 2oLang. Dùng: ./tests/run.sh [đường_dẫn_trình_biên_dịch]
#  - tests/cases/*.2o   : biên dịch rồi chạy trên Alpine64bits và Alpine32bits, so đầu ra với <tên>.out
#                         (nếu có <tên>.in thì dùng làm stdin); kiểm tra MacOS / i386 / x86_64 biên dịch được
#  - tests/errors/*.2o  : phải biên dịch lỗi. Các dòng đầu file có dạng
#                           // ERROR: <đoạn thông báo mong đợi>   (có thể nhiều dòng, mỗi thông báo phải xuất hiện)
#                           // COUNT: <N>                        (tuỳ chọn: tổng số lỗi phải đúng bằng N)
set -u
cd "$(dirname "$0")/.."
C="${1:-./2oLang}"
OUT=tests/out
mkdir -p "$OUT"
pass=0; fail=0
ok()  { pass=$((pass+1)); echo "  ok   $1"; }
bad() { fail=$((fail+1)); echo "  FAIL $1"; }

echo "== chương trình chạy được"
for src in tests/cases/*.2o; do
  name=$(basename "$src" .2o)
  input=/dev/null; [ -f "tests/cases/$name.in" ] && input="tests/cases/$name.in"
  for tgt in Alpine64bits Alpine32bits; do
    bin="$OUT/$name.$tgt"
    if ! "$C" "$src" "-($tgt)->" "$bin" >/dev/null 2>"$OUT/$name.cerr"; then
      bad "$name [$tgt] biên dịch lỗi: $(head -1 "$OUT/$name.cerr")"; continue
    fi
    "$bin" < "$input" > "$OUT/$name.$tgt.out" 2>&1
    if diff -q "tests/cases/$name.out" "$OUT/$name.$tgt.out" >/dev/null; then ok "$name [$tgt]"
    else bad "$name [$tgt] đầu ra khác mong đợi"; diff "tests/cases/$name.out" "$OUT/$name.$tgt.out" | head -6; fi
  done
  for tgt in MacOS i386 x86_64; do
    if "$C" "$src" "-($tgt)->" "$OUT/$name.$tgt" >/dev/null 2>&1; then ok "$name [$tgt] biên dịch"
    else bad "$name [$tgt] biên dịch lỗi"; fi
  done
done

echo "== lỗi biên dịch phải được báo đúng"
for src in tests/errors/*.2o; do
  name=$(basename "$src" .2o)
  wants=$(grep -E '^// ERROR:' "$src" | sed 's|^// ERROR: *||')
  count=$(grep -E '^// COUNT:' "$src" | head -1 | sed 's|^// COUNT: *||')
  rm -f "$OUT/$name.bin"
  msg=$("$C" "$src" "-(Alpine64bits)->" "$OUT/$name.bin" 2>&1); code=$?
  if [ -z "$wants" ]; then bad "$name: thiếu dòng '// ERROR: ...'"; continue; fi
  if [ "$code" = 0 ]; then bad "$name: lẽ ra phải báo lỗi"; continue; fi
  if [ -e "$OUT/$name.bin" ]; then bad "$name: có lỗi mà vẫn tạo file đầu ra"; continue; fi
  missing=""
  while IFS= read -r want; do
    echo "$msg" | grep -qF -- "$want" || missing="$missing [$want]"
  done <<< "$wants"
  if [ -n "$missing" ]; then bad "$name: thiếu thông báo:$missing"; echo "$msg" | head -3
  elif [ -n "$count" ] && ! echo "$msg" | grep -qF "Tìm thấy $count lỗi"; then
    bad "$name: số lỗi không phải $count: $(echo "$msg" | grep 'Tìm thấy')"
  else ok "$name"; fi
done

echo "== terminal giả (pty): system.*, màu"
if command -v python3 >/dev/null 2>&1; then
  ptyout=$(python3 tests/pty/run_pty.py "$C" 2>&1); ptycode=$?
  echo "$ptyout" | grep -E "^  (ok|FAIL)"
  pass=$((pass + $(echo "$ptyout" | grep -c "^  ok")))
  fail=$((fail + $(echo "$ptyout" | grep -c "^  FAIL")))
  if [ $ptycode != 0 ] && ! echo "$ptyout" | grep -q "^  FAIL"; then bad "pty: $(echo "$ptyout" | tail -1)"; fi
else
  echo "  (bỏ qua: máy không có python3)"
fi

echo; echo "đạt: $pass, hỏng: $fail"
[ "$fail" = 0 ]
