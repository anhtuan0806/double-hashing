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

Sau khi chèn dữ liệu, chương trình sẽ thống kê độ dài cluster cho từng phương pháp dò.

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
Ngoài ra kết quả tổng hợp cũng được ghi vào file `results.csv` để tiện mở bằng Excel.
Thông tin chi tiết về độ dài cluster được lưu tại file `clusters.csv`.

## Tác giả

- Nhóm 9
- Lớp 24CTT2A
- Trường Đại học Khoa học Tự nhiên, ĐHQG-HCM
