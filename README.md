# 🦖 STM32-ChromeDino: Embedded Bare-Metal Project

Dự án phát triển và mô phỏng lại trò chơi giải trí nổi tiếng **Chrome Dinosaur (T-Rex Run)** hoạt động trong môi trường hệ thống nhúng Bare-metal (không sử dụng hệ điều hành). Chương trình được tối ưu hóa riêng cho vi điều khiển lõi ARM Cortex-M4 kết hợp hệ sinh thái phần cứng Lumi.

**Nhóm phát triển (Team 1):** Tống Tâm Ngọc &  Trần Huy Sơn.

---

## 🛠️ Thông Số Hệ Thống & Sơ Đồ Chân (Hardware Layout)

Hệ thống cấu thành từ board mạch nền tảng kết hợp bo mở rộng thông qua các đường bus ngoại vi tiêu chuẩn:
* **MCU:** STMicroelectronics NUCLEO-F401RE.
* **Shield mở rộng:** LUMI IoT STM Board Kit (Phiên bản LM-ISBK V1.0).

### Bảng tra cứu ánh xạ ngoại vi (Pin Mapping)

| Thiết bị ngoại vi | Mã hiệu linh kiện | Chân kết nối (STM32) | Giao tiếp / Vai trò kỹ thuật |
| :--- | :--- | :--- | :--- |
| **Màn hình Đồ họa** | ST7735 LCD (128x128) | SPI Pins | Hiển thị ma trận điểm, màu sắc |
| **Còi phản hồi** | Active Buzzer | `PC9` (via Transistor) | Phát tín hiệu âm thanh tần số khi nhảy |
| **Hệ thống Đèn nền** | LED RGB | Transistor Driver | Nhấp nháy cảnh báo trạng thái Game Over |
| **Nút bấm 1 (SW1)** | Tactile Button | `PB5` | Di chuyển/Điều hướng Menu lên phía trên |
| **Nút bấm 3 (SW3)** | Tactile Button | `PA4` | Xác nhận Menu / Ra lệnh cho Khủng long nhảy |
| **Nút bấm 5 (SW5)** | Tactile Button | `PB4` | Di chuyển/Điều hướng Menu xuống phía dưới |

---

## 🧠 Giải Thuật Hệ Thống & Tư Duy Thiết Kế (System Architecture)

Dự án áp dụng nhiều kỹ thuật lập trình tối ưu hóa bộ nhớ và tăng tốc xử lý cho vi điều khiển:

### 1. Kiến trúc Máy trạng thái (Finite State Machine - FSM)
Vòng đời vận hành của chương trình được quản lý tập trung qua biến trạng thái toàn cục với 3 phân vùng độc lập:
* `ST_MENU`: Giao diện tương tác ban đầu. Cho phép xem điểm kỷ lục (`Best Score`) và cấu hình vận tốc trò chơi.
* `ST_PLAY`: Vòng lặp gameplay cốt lõi. Tính toán vị trí, bắt sự kiện và cập nhật đồ họa hiển thị.
* `ST_OVER`: Trạng thái dừng hệ thống khi xảy ra va chạm. Kích hoạt chuỗi cảnh báo LED/Buzzer trong 2 giây trước khi tự động khởi động lại Menu.

### 2. Quản lý Đồ họa & Khung hình
* **Đồng bộ khung thời gian (Non-blocking FPS Control):** Thay vì dùng hàm khóa luồng (`delay`), hệ thống áp dụng cơ chế kiểm tra sai lệch thời gian thực qua hàm đếm Tick với chu kỳ cố định `60ms/frame` (~16.6 FPS).
* **Vẽ cục bộ chống nhấp nháy (Partial Refresh):** Để giải quyết giới hạn tốc độ truyền của bus SPI khi kéo màn hình màu, thuật toán chỉ thực hiện xóa và vẽ lại các pixel có sự thay đổi tọa độ (giữa vị trí cũ và mới). Phương pháp này giúp triệt tiêu hoàn toàn hiện tượng sụt khung hình hoặc chớp nháy LCD.

### 3. Vật lý & Thuật toán va chạm
* **Cơ chế nhảy đơn giản:** Quỹ đạo bay được mô phỏng tuyến tính gói gọn trong 14 frame (7 frame gia tốc đi lên, 7 frame giảm tốc đi xuống) với biên độ trần đạt 35 pixel.
* **Phát hiện va chạm hình học (AABB):** Áp dụng mô hình toán học *Axis-Aligned Bounding Box*. Hệ thống liên tục thực hiện so sánh 4 đường biên tọa độ của hai hình chữ nhật bao quanh Khủng long và Xương rồng, giúp phát hiện va chạm tức thì với chi phí tính toán cực thấp.
* **Xoay vòng chướng ngại vật:** Tích hợp 3 hình dáng xương rồng khác nhau, được thay đổi luân phiên ngẫu nhiên dựa trên giá trị thời gian thực tại thời điểm chướng ngại vật cũ đi hết màn hình.

---

## 🔄 Lưu Đồ Thuật Toán Hệ Thống (System Flowcharts)

Hệ thống được thiết kế theo kiến trúc **Máy trạng thái hữu hạn (FSM)**. Dưới đây là sơ đồ luân chuyển tổng quan và luồng xử lý chi tiết của từng trạng thái.

### 1. Sơ đồ trạng thái tổng quan (Main State Machine)
```mermaid
stateDiagram-v2
    direction LR
    [*] --> Init: Cấp nguồn
    
    Init --> ST_MENU : Khởi tạo xong
    
    ST_MENU --> ST_PLAY : Bấm SW3 (Start)
    
    ST_PLAY --> ST_OVER : Xảy ra va chạm
    
    ST_OVER --> ST_MENU : Đợi đủ 2 giây
```
### 2. Giai đoạn Khởi tạo (Initialization)
```mermaid
graph LR
    A((Start)) --> B[Cấu hình Clock 84MHz]
    B --> C[Khởi tạo LCD & Timer]
    C --> D[Cấu hình ngoại vi: LED, Buzzer, Button]
    D --> E(((ST_MENU)))
    
    style A fill:#4CAF50,stroke:#333,stroke-width:2px,color:#fff
    style E fill:#2196F3,stroke:#333,stroke-width:2px,color:#fff
```
### 3. Trạng thái ST_MENU (Giao diện chờ)
```mermaid
graph TD
    A(((ST_MENU))) --> B{Quét tín hiệu nút bấm?}
    
    B -->|SW1 / PB5| C[Dịch chuyển con trỏ LÊN]
    B -->|SW5 / PB4| D[Dịch chuyển con trỏ XUỐNG]
    B -->|SW3 / PA4| E{Vị trí hiện tại của con trỏ?}
    
    C -.-> A
    D -.-> A
    
    E -->|Mục Speed| F[Đảo cấu hình tốc độ: Chậm 🔄 Nhanh]
    F -.-> A
    
    E -->|Mục Start Game| G[Xóa màn hình, vẽ lại mặt đất & nhân vật]
    G --> H(((ST_PLAY)))
    
    style A fill:#2196F3,stroke:#333,stroke-width:2px,color:#fff
    style H fill:#FF9800,stroke:#333,stroke-width:2px,color:#fff
```
### 4. Trạng thái ST_PLAY (Vòng lặp Game chính)
```mermaid
graph TD
    A(((ST_PLAY))) --> B{Thời gian chờ đã đủ 60ms?}
    
    B -->|Chưa đủ| C{Có bấm SW3 không?}
    C -->|Có| D[Kích hoạt cờ Nhảy + Bật Buzzer kêu]
    C -->|Không| B
    D --> B
    
    B -->|Đã đủ 60ms| E[Cập nhật Tọa độ Khủng long & Xương rồng]
    E --> F{Hộp viền có giao nhau không? \n Thuật toán AABB}
    
    F -->|KHÔNG va chạm| G[Xóa phần thừa, Vẽ lại khung hình mới]
    G --> H[Cập nhật Điểm số]
    H -.-> A
    
    F -->|CÓ va chạm| I(((ST_OVER)))
    
    style A fill:#FF9800,stroke:#333,stroke-width:2px,color:#fff
    style I fill:#f44336,stroke:#333,stroke-width:2px,color:#fff
```
### 5. Trạng thái ST_OVER (Kết thúc lượt chơi)
```mermaid
graph LR
    A(((ST_OVER))) --> B[Bật LED Đỏ + Còi Buzzer]
    B --> C[Hiển thị chữ GAME OVER, Điểm số & Kỷ lục]
    C --> D{Thời gian chờ đủ 2 giây?}
    
    D -->|Chưa| D
    D -->|Đã đủ 2s| E[Tắt tự động LED và Buzzer]
    E --> F(((ST_MENU)))
    
    style A fill:#f44336,stroke:#333,stroke-width:2px,color:#fff
    style F fill:#2196F3,stroke:#333,stroke-width:2px,color:#fff
```

## 📂 Cấu Trúc Mã Nguồn (Repository Directory)

Dự án được phân tách cấu trúc rõ ràng theo chuẩn STM32CubeIDE nhằm đảm bảo tính module hóa:

```text
├── Core/
│   ├── Inc/                    # Thư mục chứa các file tiêu đề (.h)
│   └── Src/
│       ├── main.c              # Xử lý logic vòng lặp game và máy trạng thái
│       ├── syscalls.c          # Các hàm stub hệ thống mức thấp của GCC
│       └── sysmem.c            # Quản lý phân vùng bộ nhớ động Heap cho Newlib
├── Startup/                    # Mã nguồn khởi động và Vector ngắt (.s)
├── LinkerScripts/              # File phân bổ vùng nhớ Flash/RAM (.ld)
├── SDK_1.0.3_NUCLEO-F401RE/    # Bộ thư viện chuẩn của ST và mở rộng của Lumi
└── README.md                   # Tài liệu hướng dẫn hệ thống
```
## Tổng quan

Người chơi có thể chọn độ khó trên màn hình menu khi mở game. Sau khi bắt đầu, người chơi điều khiển khủng long với mục tiêu nhảy qua càng nhiều xương rồng càng tốt. Game sẽ theo dõi điểm số và hiển thị trên menu. Khi thua, người chơi sẽ được hiển thị điểm hiện tại và điểm cao nhất, sau đó quay về menu.

## Yêu cầu phần mềm

- [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html)
- [LUMI SDK](https://github.com/HD-Nam/ThuVien_SDK_1.0.3_NUCLEO-F401RE) đã clone về máy

## Chi tiết kỹ thuật

- **Tốc độ khung hình:** Cố định 60ms/frame (khoảng 16 FPS) cho mọi mức tốc độ
- **Tốc độ xương rồng:** Chậm = 6 pixel/frame, Nhanh = 9 pixel/frame
- **Nhảy:** 14 frame (7 frame bay lên + 7 frame rơi xuống), độ cao 35 pixel
- **Va chạm:** Sử dụng thuật toán AABB (Axis-Aligned Bounding Box)
- **3 loại xương rồng:** Xoay vòng ngẫu nhiên mỗi lần xuất hiện lại
