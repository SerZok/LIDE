(defun sum-range (start end)
  "Сумма целых чисел от START до END включительно."
  (loop for i from start to end
        summing i))

(print (sum-range 1 10)) ; => 55
(print (* 12 3))