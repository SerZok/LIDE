# LIDE

LIDE is a lightweight IDE for Common Lisp, built with C++ and Qt. It integrates Steel Bank Common Lisp (SBCL) for interactive development.

![LIDE screenshot](https://github.com/user-attachments/assets/f98f7ae8-ee6d-4233-b15f-6b1c5026e804)

### Features

- Lisp editor with syntax highlighting and tabs.
- Integrated SBCL REPL for running and testing code.
- Project and file management.
- AI assistant for chat and code explanations.
- RAG search across UTF-8 text files and text-based PDFs. Scanned PDFs and Office documents are not supported.
- An AI benchmark launched as the separate `lide-ai-benchmark` application; CLI, GUI, and result aggregation are in [`benchmark/`](./benchmark/README.md).

### Requirements

- C++17
- Qt 6.9+ (Widgets, Gui, Network, and Pdf modules)
- CMake 3.19+

### Build

```sh
cmake -S . -B build
cmake --build build --config Release
```

Inno Setup is only required to create Windows installers.

### Updating translations

Update translation files with `cmake --build build --target update_translations`, edit the generated `.ts` files in Qt Linguist, then build the project to compile the translations.

---

LIDE - легковесная IDE для Common Lisp на C++ и Qt. Для интерактивной разработки в неё интегрирован Steel Bank Common Lisp (SBCL).

### Возможности

- Редактор Lisp-кода с подсветкой синтаксиса и вкладками.
- Встроенный REPL на SBCL для запуска и проверки кода.
- Управление проектами и файлами.
- AI-ассистент для чата и объяснения кода.
- RAG-поиск по UTF-8-тексту и PDF с текстовым слоем. Сканированные PDF и офисные документы не поддерживаются.
- Бенчмарк ИИ запускается отдельной программой `lide-ai-benchmark`; CLI, GUI и агрегатор результатов находятся в [`benchmark/`](./benchmark/README.md).

### Separate benchmark repository / submodule

The benchmark lives in `benchmark/` as a standalone CMake project and is built
as a separate executable. To publish it independently, create an empty GitHub
repository named **`lide-ai-benchmark`** under the `SerZok` account, publish
the contents of `benchmark/` to its `main` branch, then replace the tracked
`benchmark/` directory with the submodule. The remote URL below is the intended
location and must exist before running these commands:

```powershell
# After all current LIDE changes have been committed:
git clone https://github.com/SerZok/lide-ai-benchmark.git ..\lide-ai-benchmark
Get-ChildItem .\benchmark -Force |
  Where-Object { $_.Name -notlike "build*" -and $_.Name -ne "__pycache__" } |
  Copy-Item -Destination ..\lide-ai-benchmark -Recurse -Force
# Then:
Set-Location ..\lide-ai-benchmark
git add .
git commit -m "Initial lide-ai-benchmark project"
git push -u origin main

# Back in the LIDE repository, preserve any local changes before removing the
# directory, then add the published repository at the existing CMake path:
Set-Location ..\LIDE
git rm -r benchmark
git submodule add https://github.com/SerZok/lide-ai-benchmark.git benchmark
git add .gitmodules benchmark
git commit -m "Add lide-ai-benchmark submodule"
```

GitHub displays a submodule link after `.gitmodules` and the submodule gitlink
are pushed. New checkouts use `git clone --recurse-submodules`; existing
checkouts use `git submodule update --init --recursive`. The project is not
configured as a submodule yet: the GitHub search available here could not
access `SerZok/lide-ai-benchmark`. Its recommended repository name is
`SerZok/lide-ai-benchmark`, with `benchmark/` as the mount path.

### Требования

- C++17
- Qt 6.9+ (модули Widgets, Gui, Network и Pdf)
- CMake 3.19+

### Сборка

```sh
cmake -S . -B build
cmake --build build --config Release
```

Inno Setup нужен только для создания установщиков Windows.

### Обновление переводов

Обновите файлы переводов командой `cmake --build build --target update_translations`, отредактируйте `.ts`-файлы в Qt Linguist и соберите проект, чтобы скомпилировать переводы.
