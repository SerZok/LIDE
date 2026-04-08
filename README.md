# LIDE - Lisp IDE
``Qt6`` ``Cmake`` ``C++`` ``Windows``

### Интегрированная среда разработки для Common Lisp (SBCL)

![demo](https://github.com/user-attachments/assets/f98f7ae8-ee6d-4233-b15f-6b1c5026e804)

LIDE — это **простая** современная, легковесная IDE для Common Lisp, построенная на Qt/C++.
Проект направлен на создание удобной среды разработки с интуитивным интерфейсом.

Для реализации REPL был использовал Steel Bank Common Lisp (SBCL) https://www.sbcl.org/

---

## Быстрый старт
### Требования
- C++ 17
- Qt 6.9 или выше
- CMake 3.19+
- Inno Setup (для создания установщиков): https://jrsoftware.org/isinfo.php

## Как обновить переводы

1. Добавить ``tr("Текст")``
2. Обновить переводы ``cmake --build . --target update_translations``
3. Открыть .ts файл в **QtLinguist** и перевести
4. Скомпилировать ``cmake --build .`` (``lrelease`` запуститься автоматически)
