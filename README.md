# Focal-RUN

# Focal-RUN — เครื่องมือบิลด์โปรเจกต์ CMake อย่างรวดเร็ว

## ภาพรวม

**Focal-RUN** คือเครื่องมือที่ช่วยให้คุณบิลด์โปรเจกต์ CMake และ Rust ได้โดยไม่ต้องเขียนสคริปต์บิลด์เอง รองรับการบิลด์หลายโปรเจกต์พร้อมกัน มีระบบลำดับความสำคัญให้โปรเจกต์ระดับรากถูกบิลด์ก่อน และสรุปผลอย่างละเอียดว่าโปรเจกต์ใดสำเร็จหรือผิดพลาด

## การอัปเดตล่าสุด (v1.1.0)

- ✨ เพิ่มการรองรับโปรเจกต์ Rust ด้วย Cargo
- 🦀 เพิ่มคำสั่ง `--rust` เพื่อกรองเฉพาะโปรเจกต์ Rust
- 🧪 เพิ่มคำสั่ง Cargo ต่างๆ: `--test`, `--doc`, `--run`, `--check`, `--bench`
- 🔍 สแกนไฟล์ `Cargo.toml` แบบอัตโนมัติ
- 📊 อัปเดตการตรวจสอบเครื่องมือให้รวม Rust Compiler และ Cargo

## คุณสมบัติเด่น

- 🔍 สแกนอัตโนมัติ — ค้นหาไฟล์ `CMakeLists.txt`, `Makefile`, และ `Cargo.toml` แบบ recursive ทุกไดเรกทอรีย่อย
- 🏗️ บิลด์หลายโปรเจกต์ — ใช้แฟล็ก `--all` เพื่อบิลด์พร้อมกัน
- 🎯 ลำดับความสำคัญ — โปรเจกต์ระดับรากจะถูกบิลด์ก่อนโปรเจกต์ย่อย
- 🛠️ ตรวจจับระบบบิลด์ — เลือกใช้ CMake, Make, Ninja, MinGW, GCC, หรือ Cargo ให้อัตโนมัติ
- 📊 รายงานผล — แสดงผลสำเร็จ/ล้มเหลวของแต่ละโปรเจกต์อย่างชัดเจน
- 🔄 โหมดบิลด์ — รองรับ Debug และ Release
- 🧹 Clean และ Rebuild — ล้างไดเรกทอรีบิลด์และบิลด์ใหม่ตั้งแต่ต้น
- 📁 บิลด์ไฟล์เดี่ยว — รองรับการคอมไพล์ไฟล์ `.cpp` เพียงไฟล์เดียว
- 🦀 รองรับ Rust — บิลด์และจัดการโปรเจกต์ Rust ด้วย Cargo
- ✅ ตรวจสอบเครื่องมือ — ตรวจสอบเครื่องมือบิลด์ที่ติดตั้งในระบบ

## การติดตั้ง

### ข้อกำหนด

- CMake 3.20 ขึ้นไป (สำหรับโปรเจกต์ CMake)
- Rust และ Cargo (สำหรับโปรเจกต์ Rust)
- คอมไพเลอร์ C++ (GCC, Clang, MSVC)
- ระบบบิลด์อย่างใดอย่างหนึ่ง:
    - CMake (สำหรับโปรเจกต์ CMake)
    - Make (สำหรับโปรเจกต์ Makefile)
    - Ninja
    - MinGW Makefiles (Windows)
    - GCC (สำหรับไฟล์เดี่ยว)
    - Cargo (สำหรับโปรเจกต์ Rust)

### คอมไพล์

```bash
g++ -std=c++17 -o focal-run.exe focal-run.cpp
```

หรือ

```bash
clang++ -std=c++17 -o focal-run.exe focal-run.cpp
```

## วิธีใช้งาน

### คำสั่งพื้นฐาน

```bash
# แสดงวิธีใช้
focal-run.exe --help
focal-run.exe -h

# สแกนและแสดงรายชื่อโปรเจกต์ทั้งหมด
focal-run.exe --scan

# ตรวจสอบเครื่องมือบิลด์ที่ติดตั้ง
focal-run.exe --check
```

### การบิลด์โปรเจกต์

```bash
# บิลด์โปรเจกต์เฉพาะในโหมด Release (ค่าเริ่มต้น)
focal-run.exe --ProjectName --release

# บิลด์โปรเจกต์เฉพาะในโหมด Debug
focal-run.exe --ProjectName --debug

# บิลด์ทุกโปรเจกต์ในโหมด Release
focal-run.exe --release --all

# บิลด์ทุกโปรเจกต์ในโหมด Debug
focal-run.exe --debug --all

# Rebuild ทุกโปรเจกต์ (ล้างของเก่า แล้วบิลด์ใหม่ทั้งหมด)
focal-run.exe --rebuild --all
```

### การล้างไฟล์บิลด์ (Clean)

```bash
# ล้างไดเรกทอรีบิลด์ของโปรเจกต์เฉพาะ
focal-run.exe --ProjectName --clean

# ล้างไดเรกทอรีบิลด์ของทุกโปรเจกต์
focal-run.exe --clean --all

# บิลด์ไฟล์ .cpp เดี่ยว
focal-run.exe --test.cpp --build

# บิลด์ทุกโปรเจกต์ Rust
focal-run.exe --rust --all

# ทดสอบทุกโปรเจกต์ Rust
focal-run.exe --rust --test --all

# สร้างเอกสารสำหรับทุกโปรเจกต์ Rust
focal-run.exe --rust --doc --all
```

## อ้างอิงคำสั่ง

| คำสั่ง | คำอธิบาย |
| --- | --- |
| `--scan` | แสดงโปรเจกต์ที่พบและระบบบิลด์ที่รองรับ |
| `--check` | ตรวจสอบเครื่องมือบิลด์ที่ติดตั้งในระบบ |
| `--help`, `-h` | แสดงวิธีใช้และตัวอย่างโดยละเอียด |
| `--debug` | บิลด์โหมด Debug (ไม่ optimize และมีสัญลักษณ์ดีบัก) |
| `--release` | บิลด์โหมด Release (ค่าเริ่มต้น ปรับแต่งประสิทธิภาพ) |
| `--rebuild` | ล้างไดเรกทอรีบิลด์เก่า แล้วบิลด์ใหม่ตั้งแต่ต้น |
| `--clean` | ลบไดเรกทอรีบิลด์โดยไม่ทำการบิลด์ |
| `--build` | บิลด์ไฟล์ .cpp เดี่ยว |
| `--all` | ใช้กับทุกโปรเจกต์ในโฟลเดอร์และไดเรกทอรีย่อย |
| `--ProjectName` | ระบุโปรเจกต์เฉพาะโดยใช้ชื่อโฟลเดอร์ |
| `--filename.cpp` | ระบุไฟล์ .cpp เฉพาะสำหรับการบิลด์เดี่ยว |
| `--rust` | กรองเฉพาะโปรเจกต์ Rust |
| `--test` | รัน cargo test (สำหรับโปรเจกต์ Rust) |
| `--doc` | สร้างเอกสารด้วย cargo doc (สำหรับโปรเจกต์ Rust) |
| `--run` | รันโปรเจกต์ด้วย cargo run (สำหรับโปรเจกต์ Rust) |
| `--check` | ตรวจสอบด้วย cargo check (สำหรับโปรเจกต์ Rust) |
| `--bench` | รัน benchmarks ด้วย cargo bench (สำหรับโปรเจกต์ Rust) |

## ตัวอย่างการใช้งาน

### กรณีที่ 1: บิลด์โปรเจกต์เดี่ยว

```bash
# บิลด์ MyProject ในโหมด Release
focal-run.exe --MyProject --release

# Output:
# Processing: MyProject (D:\Projects\MyProject)
#   Configuring with: Ninja
#   Building...
#
# === Build Report ===
#   ✅ MyProject — Success (Generator: Ninja)
```

### กรณีที่ 2: Rebuild ทุกโปรเจกต์

```bash
# Rebuild ทั้งหมด
focal-run.exe --rebuild --all

# Output:
# === Focal-RUN Build Tool ===
# Mode: Release
# Rebuild: Yes
# Projects found: 3
#
# Processing: RootProject (D:\Projects)
#   Cleaned: D:\Projects\build
#   Configuring with: MinGW Makefiles
#   Building...
#
# Processing: SubProject1 (D:\Projects\SubProject1)
#   Configuring with: Ninja
#   Building...
#
# Processing: SubProject2 (D:\Projects\SubProject2)
#   Configuring with: Unix Makefiles
#   Building...
#
# === Build Report ===
#   ✅ RootProject — Success (Generator: MinGW Makefiles)
#   ✅ SubProject1 — Success (Generator: Ninja)
#   ⚠️  SubProject2 — Failed (Generator: Unix Makefiles)
#
#   Summary: 2 succeeded, 1 failed
```

### กรณีที่ 3: บิลด์ Debug สำหรับทุกโปรเจกต์

```bash
# บิลด์ทุกโปรเจกต์ในโหมด Debug
focal-run.exe --debug --all

# โปรเจกต์ที่บิลด์จะมีสัญลักษณ์ดีบักและปิดการ optimize
```

### กรณีที่ 4: ล้างโปรเจกต์เฉพาะ

```bash
# ล้างไดเรกทอรีบิลด์ของ MyProject
focal-run.exe --MyProject --clean

# Output:
# Processing: MyProject (D:\Projects\MyProject)
#   Cleaned: D:\Projects\build
#
# === Build Report ===
#   ✓ MyProject — Cleaned
```

### กรณีที่ 5: บิลด์ไฟล์ .cpp เดี่ยว

```bash
# บิลด์ไฟล์ test.cpp
focal-run.exe --test.cpp --build

# Output:
#   [SUCCESS] Built: .\test.cpp -> ./test.exe
```

### กรณีที่ 6: ตรวจสอบเครื่องมือบิลด์

```bash
# ตรวจสอบเครื่องมือที่ติดตั้ง
focal-run.exe --check

# Output:
# === Build Tools Check ===
#
#   CMake: OK
#   Make: OK
#   Ninja: OK
#   MinGW Make: OK
#   GCC C++ Compiler: OK
#   Rust Compiler: OK
#   Cargo Package Manager: OK
#
#   Visual Studio Versions:
#     Visual Studio Installation: OK
#     Visual Studio Compiler (cl.exe): No
#     Visual Studio IDE: No
#
#   Summary: 7/7 tools available
```

### กรณีที่ 7: บิลด์โปรเจกต์ Rust

```bash
# บิลด์ทุกโปรเจกต์ Rust ในโหมด Release
focal-run.exe --rust --all

# Output:
# === Build Report ===
#   ✅ MyRustProject — Success (Generator: Cargo)

# ทดสอบทุกโปรเจกต์ Rust
focal-run.exe --rust --test --all

# สร้างเอกสารสำหรับทุกโปรเจกต์ Rust
focal-run.exe --rust --doc --all
```

## กลไกการทำงาน

```
START focal-run.exe
│
├─ วิเคราะห์อาร์กิวเมนต์ (--rebuild, --all, --debug, ...)
│
├─ ถ้า --scan หรือ --help
│    ├─ แสดงรายการโปรเจกต์หรือคู่มือ
│    └─ EXIT
│
├─ สแกนหา CMakeLists.txt, Makefile, และ Cargo.toml แบบ recursive
│    ├─ รวบรวมโปรเจกต์ทั้งหมด
│    ├─ เรียงตามความลึกของโฟลเดอร์ (รากก่อน)
│    └─ บันทึกรายการโปรเจกต์
│
├─ ลบซ้ำและคัดโปรเจกต์
│    ├─ ถ้า --all ให้รวมทุกโปรเจกต์
│    └─ ไม่เช่นนั้น เลือกเฉพาะโปรเจกต์ที่ระบุ
│
├─ วนตามลำดับความสำคัญ:
│    ├─ ตรวจจับระบบบิลด์ (Make, Ninja, MinGW, Cargo)
│    ├─ สำหรับ CMake: สร้างโฟลเดอร์บิลด์: <project_path>/build
│    ├─ สำหรับ Rust: ใช้ Cargo's target directory
│    ├─ ถ้า --rebuild ให้ลบโฟลเดอร์บิลด์เก่า
│    ├─ ถ้าไม่ใช่ --clean
│    │    ├─ สำหรับ CMake: รัน cmake -S <path> -B <path>/build -G <generator>
│    │    ├─ สำหรับ CMake: รัน cmake --build <path>/build --config <Debug|Release>
│    │    ├─ สำหรับ Rust: รัน cargo <command> [--release]
│    │    └─ ตรวจสอบ exit code → บันทึกผล
│    └─ บันทึก log
│
└─ แสดงรายงาน:
     ✅ ProjectA — Success (Generator: Ninja)
     ⚠️  ProjectB — Failed (Generator: MinGW Makefiles)
     ✅ ProjectC — Success (Generator: Unix Makefiles)

     Summary: 2 succeeded, 1 failed
END
```

## การตรวจจับระบบบิลด์

ระบบจะเลือกใช้เครื่องมือที่เหมาะสมที่สุดตามลำดับความสำคัญ

### ลำดับความสำคัญสำหรับ CMake

1. ระบุไว้ใน CMakeLists.txt เป็นพิเศษ
2. MinGW Makefiles (Windows)
3. Ninja
4. Unix Makefiles (Linux/Mac)
5. Cargo (สำหรับโปรเจกต์ Rust)

### ลำดับความสำคัญสำหรับไฟล์เดี่ยว

1. GCC C++ Compiler (g++)
2. ตรวจสอบการติดตั้ง Visual Studio

### ระบุ Generator ใน CMakeLists.txt

เพิ่มคอมเมนต์ด้านบนของไฟล์ `CMakeLists.txt`:

```
# Focal-Generator: Ninja
# หรือ
# Focal-Generator: MinGW Makefiles
# หรือ
# Focal-Generator: Unix Makefiles

cmake_minimum_required(VERSION 3.20)
project(MyProject)

add_executable(myapp main.cpp)
```

## โครงสร้างโปรเจกต์

### สำหรับโปรเจกต์ CMake

แต่ละโปรเจกต์ควรมี:

- ไฟล์ `CMakeLists.txt` ที่รากโปรเจกต์
- ไฟล์ซอร์สโค้ด
- (ไม่บังคับ) ไฟล์เฮดเดอร์

```
MyProject/
├─ CMakeLists.txt
├─ src/
│  └─ main.cpp
└─ include/
   └─ myheader.h
```

### สำหรับโปรเจกต์ Makefile

แต่ละโปรเจกต์ควรมี:

- ไฟล์ `Makefile` ที่รากโปรเจกต์
- ไฟล์ซอร์สโค้ด

```
MyProject/
├─ Makefile
├─ main.cpp
└─ utils.cpp
```

### สำหรับไฟล์เดี่ยว

เพียงแค่ไฟล์ `.cpp` เดียวก็เพียงพอ:

```
myprogram.cpp
```

## เอาต์พุตและรายงาน

### รายงานสำเร็จ

```
✅ ProjectName — Success (Generator: Ninja)
```

### รายงานล้มเหลว

```
⚠️  ProjectName — Failed (Generator: MinGW Makefiles)
```

### รายงานการล้าง

```
✓ ProjectName — Cleaned
```

## ลำดับความสำคัญของโปรเจกต์

เรียงตามระดับความลึกของไดเรกทอรี:

- ระดับ 1: โปรเจกต์ราก จะถูกบิลด์ก่อน
- ระดับ 2: โฟลเดอร์ย่อยระดับแรก
- ระดับ 3: โฟลเดอร์ย่อยซ้อนกันลึกลงไป

ช่วยให้การพึ่งพากันถูกบิลด์ตามลำดับ

## เคล็ดลับและข้อควรทราบ

- โปรเจกต์ CMake ต้องมีไฟล์ `CMakeLists.txt`
- โปรเจกต์ Makefile ต้องมีไฟล์ `Makefile`
- โปรเจกต์ Rust ต้องมีไฟล์ `Cargo.toml`
- ไฟล์เดี่ยวต้องมีนามสกุล `.cpp`
- เอาต์พุตบิลด์จะอยู่ในโฟลเดอร์ `<project>/build` สำหรับ CMake, `<project>/release` หรือ `<project>/debug` สำหรับ Makefile, และ `<project>/target` สำหรับ Rust
- ไฟล์เดี่ยวจะถูกคอมไพล์เป็น `.exe` ในโฟลเดอร์เดียวกัน
- สำหรับ Rust ใช้ `--rust` เพื่อกรองเฉพาะโปรเจกต์ Rust และใช้ `--test`, `--doc`, `--run`, `--check`, `--bench` สำหรับคำสั่ง Cargo ต่างๆ
- ใช้ `--rebuild` เมื่อต้องการล้างของเก่าและเริ่มใหม่ โดยเฉพาะหลังเปลี่ยนแปลงครั้งใหญ่
- ผสาน `--debug` กับ `--rebuild` เมื่อต้องการบิลด์ดีบักสะอาดๆ พร้อมสัญลักษณ์
- ระบบบิลด์จะถูกตรวจจับให้อัตโนมัติ
- โปรเจกต์ที่ไม่มีไฟล์บิลด์ที่เหมาะสมจะถูกข้าม
- exit code 0 = สำเร็จ ส่วนค่าอื่น = ล้มเหลว

## การแก้ไขปัญหา

### "No projects found!"

- ตรวจสอบว่ามีไฟล์ `CMakeLists.txt`, `Makefile`, `Cargo.toml`, หรือไฟล์ `.cpp` ในไดเรกทอรีโปรเจกต์
- ตรวจสอบว่าเรียกใช้เครื่องมือในไดเรกทอรีที่ถูกต้อง
- สำหรับไฟล์เดี่ยว ตรวจสอบว่าไฟล์มีนามสกุล `.cpp`
- สำหรับ Rust projects ตรวจสอบว่า `Cargo.toml` มีอยู่และถูกต้อง

### บิลด์ล้มเหลวจาก generator

- ยืนยันว่าติดตั้ง Make, Ninja, MinGW, GCC หรือ CMake แล้ว
- ตรวจสอบไวยากรณ์ใน CMakeLists.txt หรือ Makefile
- ลองระบุ generator ด้วยคอมเมนต์ `# Focal-Generator: ...`
- สำหรับไฟล์เดี่ยว ตรวจสอบว่า g++ สามารถเข้าถึงได้

### “Permission denied”

- ให้สิทธิ์อ่าน/เขียนกับเครื่องมือและซอร์ส
- บน Linux/Mac อาจต้องใช้สิทธิ์ที่เหมาะสม

## ใบอนุญาต

Focal-RUN จัดทำให้ใช้งาน "ตามสภาพที่เป็นอยู่" สำหรับการบิลด์โปรเจกต์ CMake และ Rust

## การสนับสนุน

หากมีปัญหาหรือคำถาม โปรดอ้างอิงเอกสาร CMakeLists.txt, Cargo.toml และเอกสารทางการของ CMake และ Rust
