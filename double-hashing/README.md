# Double Hashing Experiment

Đây là project minh họa và thử nghiệm kỹ thuật **Double Hashing** trong cấu trúc dữ liệu Hash Table.

## Mục tiêu

- Hiểu và thực hành kỹ thuật Double Hashing để giải quyết vấn đề va chạm (collision) trong Hash Table.
- So sánh hiệu quả của Double Hashing với các phương pháp khác.

## Cấu trúc thư mục

```
double-hashing/
├── src/
│   ├── CMakeLists.txt       # Cấu hình CMake cho thư mục src 
│   └── main.cpp             # File mã nguồn chính
├── .gitignore               # Danh sách file/thư mục bỏ qua khi commit
├── CMakeLists.txt           # File cấu hình CMake cho toàn project
├── LICENSE                  # Thông tin bản quyền
└── README.md                # Tài liệu mô tả, hướng dẫn dự án
```

## Hướng dẫn build & chạy

**Yêu cầu:**  
- Đã cài đặt [CMake](https://cmake.org/)  
- Trình biên dịch C++ (g++ hoặc MSVC...)

**Các bước:**

```sh
 #Bước 1: Tạo thư mục build
mkdir build
cd build

 #Bước 2: Cấu hình project bằng CMake
cmake ..  hoặc cmake .. -G "Visual Studio 17 2022" nếu cần chỉ định cụ thể

 #Bước 3: Biên dịch với cấu hình Debug
cmake --build . --config Debug

 #Bước 4: Chạy chương trình (file .exe nằm trong thư mục src\Debug)
cd src\Debug
double-hashing.exe
```
## Tác giả

- Nhóm 9
- Lớp 24CTT2A
- Trường Đại học Khoa học Tự nhiên, ĐHQG-HCM
