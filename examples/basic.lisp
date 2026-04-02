;; Арифметические операции
(print "=== Арифметика ===")
(print (+ 10 25 30))
(print (- 100 45))
(print (* 6 7 8))
(print (/ 100 4))
(print (expt 2 10))  ; 2^10

;; Работа со списками
(print "=== Списки ===")
(print (list 1 2 3 4 5))
(print (cons 'a '(b c d)))
(print (car '(apple banana cherry)))
(print (cdr '(apple banana cherry)))
(print (append '(1 2) '(3 4) '(5 6)))

;; Условные выражения
(print "=== Условные выражения ===")
(setq x 42)
(setq y -99)

(if (> x 50)
    (print "x больше 50")
    (print "x меньше или равен 50"))

(cond ((< x 0) (print "Отрицательное"))
      ((= x 0) (print "Ноль"))
      (t (print "Положительное")))

;; Циклы
(print "=== Циклы ===")
(dotimes (i 5)
  (format t "Итерация ~d~%" i))

(loop for i from 1 to 5
      do (format t "Число: ~d~%" i))