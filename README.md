# Focal-RUN

## ภาพรวม

**Focal-RUN** คือเครื่องมือที่ช่วยให้คุณบิลด์โปรเจกต์ CMake ได้โดยไม่ต้องเขียนสคริปต์บิลด์เอง รองรับการบิลด์หลายโปรเจกต์พร้อมกัน มีระบบลำดับความสำคัญให้โปรเจกต์ระดับรากถูกบิลด์ก่อน และสรุปผลอย่างละเอียดว่าโปรเจกต์ใดสำเร็จหรือผิดพลาด

## คุณสมบัติเด่น

- 🔍 สแกนอัตโนมัติ — ค้นหาไฟล์ `CMakeLists.txt` แบบ recursive ทุกไดเรกทอรีย่อย
- 🏗️ บิลด์หลายโปรเจกต์ — ใช้แฟล็ก `--all` เพื่อบิลด์พร้อมกัน
- 🎯 ลำดับความสำคัญ — โปรเจกต์ระดับรากจะถูกบิลด์ก่อนโปรเจกต์ย่อย
- 🛠️ ตรวจจับระบบบิลด์ — เลือกใช้ Make, Ninja, หรือ MinGW ให้อัตโนมัติ
- 📊 รายงานผล — แสดงผลสำเร็จ/ล้มเหลวของแต่ละโปรเจกต์อย่างชัดเจน
- 🔄 โหมดบิลด์ — รองรับ Debug และ Release
- 🧹 Clean และ Rebuild — ล้างไดเรกทอรีบิลด์และบิลด์ใหม่ตั้งแต่ต้น

## การติดตั้ง

### ข้อกำหนด

- CMake 3.20 ขึ้นไป
- คอมไพเลอร์ C++ (GCC, Clang, MSVC)
- ระบบบิลด์อย่างใดอย่างหนึ่ง:
    - MinGW Makefiles (Windows)
    - Ninja
    - Make (Unix/Linux/Mac)

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
```

## อ้างอิงคำสั่ง

| คำสั่ง | คำอธิบาย |
| --- | --- |
| `--scan` | แสดงโปรเจกต์ที่พบและระบบบิลด์ที่รองรับ |
| `--help`, `-h` | แสดงวิธีใช้และตัวอย่างโดยละเอียด |
| `--debug` | บิลด์โหมด Debug (ไม่ optimize และมีสัญลักษณ์ดีบัก) |
| `--release` | บิลด์โหมด Release (ค่าเริ่มต้น ปรับแต่งประสิทธิภาพ) |
| `--rebuild` | ล้างไดเรกทอรีบิลด์เก่า แล้วบิลด์ใหม่ตั้งแต่ต้น |
| `--clean` | ลบไดเรกทอรีบิลด์โดยไม่ทำการบิลด์ |
| `--all` | ใช้กับทุกโปรเจกต์ในโฟลเดอร์และไดเรกทอรีย่อย |
| `--ProjectName` | ระบุโปรเจกต์เฉพาะโดยใช้ชื่อโฟลเดอร์ |

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
├─ สแกนหา CMakeLists.txt แบบ recursive
│    ├─ รวบรวมโปรเจกต์ทั้งหมด
│    ├─ เรียงตามความลึกของโฟลเดอร์ (รากก่อน)
│    └─ บันทึกรายการโปรเจกต์
│
├─ ลบซ้ำและคัดโปรเจกต์
│    ├─ ถ้า --all ให้รวมทุกโปรเจกต์
│    └─ ไม่เช่นนั้น เลือกเฉพาะโปรเจกต์ที่ระบุ
│
├─ วนตามลำดับความสำคัญ:
│    ├─ ตรวจจับระบบบิลด์ (Make, Ninja, MinGW)
│    ├─ สร้างโฟลเดอร์บิลด์: <project_path>/build
│    ├─ ถ้า --rebuild ให้ลบโฟลเดอร์บิลด์เก่า
│    ├─ ถ้าไม่ใช่ --clean
│    │    ├─ รัน: cmake -S <path> -B <path>/build -G <generator>
│    │    ├─ รัน: cmake --build <path>/build --config <Debug|Release>
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

### ลำดับความสำคัญ

1. ระบุไว้ใน CMakeLists.txt เป็นพิเศษ
2. MinGW Makefiles (Windows)
3. Ninja
4. Unix Makefiles (Linux/Mac)

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

- แต่ละโปรเจกต์ต้องมีไฟล์ `CMakeLists.txt` ของตัวเอง
- เอาต์พุตบิลด์จะอยู่ในโฟลเดอร์ `<project>/build`
- ใช้ `--rebuild` เมื่อต้องการล้างของเก่าและเริ่มใหม่ โดยเฉพาะหลังเปลี่ยนแปลงครั้งใหญ่
- ผสาน `--debug` กับ `--rebuild` เมื่อต้องการบิลด์ดีบักสะอาดๆ พร้อมสัญลักษณ์
- ระบบบิลด์จะถูกตรวจจับให้อัตโนมัติ
- โปรเจกต์ที่ไม่มี `CMakeLists.txt` จะถูกข้าม
- exit code 0 = สำเร็จ ส่วนค่าอื่น = ล้มเหลว

## การแก้ไขปัญหา

### “No CMake projects found!”

- ตรวจสอบว่ามีไฟล์ `CMakeLists.txt` ในไดเรกทอรีโปรเจกต์
- ตรวจสอบว่าเรียกใช้เครื่องมือในไดเรกทอรีที่ถูกต้อง

### บิลด์ล้มเหลวจาก generator

- ยืนยันว่าติดตั้ง Make, Ninja หรือ MinGW แล้ว
- ตรวจสอบไวยากรณ์ใน CMakeLists.txt
- ลองระบุ generator ด้วยคอมเมนต์ `# Focal-Generator: ...`

### “Permission denied”

- ให้สิทธิ์อ่าน/เขียนกับเครื่องมือและซอร์ส
- บน Linux/Mac อาจต้องใช้สิทธิ์ที่เหมาะสม

## ใบอนุญาต

Focal-RUN จัดทำให้ใช้งาน “ตามสภาพที่เป็นอยู่” สำหรับการบิลด์โปรเจกต์ CMake

## การสนับสนุน

หากมีปัญหาหรือคำถาม โปรดอ้างอิงเอกสาร CMakeLists.txt และเอกสารทางการของ CMake
