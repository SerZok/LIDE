(defun copy (lst)
  (if (null lst)
      nil
      (cons (car lst) (copy (cdr lst)))))

(TRACE copy)

(copy '(S B kHG jhG gK hJL hjL ))



(setq x 0)

(defun req()
(print x)
(setq x (+ x 1))
(req)
)

(req)