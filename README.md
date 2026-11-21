# LIDE

LIDE is a lightweight IDE for Common Lisp, built with C++ and Qt. It integrates Steel Bank Common Lisp (SBCL) for interactive development.

![LIDE screenshot](https://github.com/user-attachments/assets/f98f7ae8-ee6d-4233-b15f-6b1c5026e804)

### Features

- Lisp editor with syntax highlighting and tabs.
- Integrated SBCL REPL for running and testing code.
- Project and file management.
- AI assistant for chat and code explanations.
- RAG search across UTF-8 text files and text-based PDFs. Scanned PDFs and Office documents are not supported.

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
