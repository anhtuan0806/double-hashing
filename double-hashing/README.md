# Double Hashing Experiment

Đây là project minh họa và thử nghiệm kỹ thuật **Double Hashing** trong cấu trúc dữ liệu Hash Table.

## Mục tiêu

- Hiểu và thực hành kỹ thuật Double Hashing để giải quyết vấn đề va chạm (collision) trong Hash Table.
- So sánh hiệu quả của Double Hashing với các phương pháp khác.

## Cấu trúc thư mục

```text
double-hashing/
├── LICENSE           # Thông tin bản quyền
├── README.md         # Tài liệu mô tả, hướng dẫn dự án
└── main.cpp          # File mã nguồn chính
```

## Mô tả chương trình

Chương trình `main.cpp` thực hiện benchmark ba phương pháp dò
(Double Hashing, Linear Probing và Quadratic Probing) trên các bộ dữ
liệu sinh theo ba mẫu: ngẫu nhiên, tuần tự và có cụm. Mỗi bộ dữ liệu
được kiểm thử với hai hệ số tải 0.5 và 0.9, đồng thời tùy chọn bật/tắt
rehash. Kết quả bao gồm thời gian thực thi các thao tác (insert,
search, delete) và thống kê độ dài cluster của từng phương pháp.

## Hướng dẫn build & chạy

**Yêu cầu:**
- Trình biên dịch C++ hỗ trợ chuẩn C++17 (g++, clang++, ...)

**Các bước:**

```sh
# Biên dịch chương trình
g++ -std=c++17 -O2 main.cpp -o double-hashing

# Chạy chương trình
./double-hashing
```

Sau khi chèn dữ liệu, chương trình sẽ thống kê độ dài cluster cho từng phương pháp dò.
Ngoài ra kết quả tổng hợp cũng được ghi vào file `time.csv` để tiện mở bằng Excel.
Thông tin chi tiết về độ dài cluster được lưu tại file `clusters.csv`.

## Input

Khi chạy, chương trình yêu cầu nhập một dòng gồm các số nguyên dương cách
nhau bởi dấu cách, đại diện cho số lượng phần tử muốn kiểm thử. Ví dụ:

```text
1000 2000 5000
```

## Output

Kết quả thống kê được in ra màn hình và đồng thời lưu vào hai file CSV:

- `time.csv`: thời gian thực thi của các thao tác insert, search và delete.
- `clusters.csv`: độ dài cụm khóa sau khi chèn.

## Tác giả

- Nhóm 9
- Lớp 24CTT2A
- Trường Đại học Khoa học Tự nhiên, ĐHQG-HCM
