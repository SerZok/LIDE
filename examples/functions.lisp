;;; Файл для тестирования функций;;; Файл для тестирования функцийФайл для тестирования функцийФайл для тестирования функцийФайл для тестирования функцийФайл для тестирования функцийФайл для тестирования функцийФайл для тестирования функцийФайл для тестирования функций

;; Простая функция
(defun square (x)
  "Возвращает квадрат числа"
  (* x x))

;; Функция с несколькими параметрами
(defun rectangle-area (length width)
  "Вычисляет площадь прямоугольника"
  (* length width))

;; Рекурсивная функция (факториал)
(defun factorial (n)
  "Вычисляет факториал числа n"
  (if (<= n 1)
      1
      (* n (factorial (- n 1)))))

;; Функция с &optional параметрами
(defun greet (name &optional (title "Mr./Ms."))
  (format nil "Hello, ~a ~a!" title name))

;; Функция с &key параметрами
(defun make-person (&key name age city)
  (list :name name :age age :city city))

;; Тестирование функций
(print "=== Тестирование функций ===")
(print (square 12))
(print (rectangle-area 5 8))
(print (factorial 6))
(print (greet "John" "Dr."))
(print (greet "Alice"))
(print (make-person :name "Bob" :age 30 :city "New York"))