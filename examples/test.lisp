(defun factorial (n)
  "Возвращает факториал числа N."
  (if (zerop n)
      1
      (* n (factorial (- n 1)))))