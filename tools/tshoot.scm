;;; various standard benchmarks 
;;; --------------------------------------------------------------------------------

(define (fannkuch n)
  (call-with-exit
   (lambda (return)
     (let ((perm (make-int-vector n))
	   (perm1 (make-int-vector n))
	   (count (make-int-vector n))
	   (subs (make-vector n))
	   (subs1 (make-vector n))
	   (maxFlipsCount 0)
	   (checksum 0)
	   (r n)
	   (perm0 0))
       
       (do ((i 0 (+ i 1)))
	   ((= i n))
	 (int-vector-set! perm1 i i)
	 (vector-set! subs i (subvector perm (+ i 1)))
	 (vector-set! subs1 i (subvector perm1 i 1)))
       
       (do ((permCount #t (not permCount)))
	   (#f)
	 (do ()
	     ((= r 1))
	   (int-vector-set! count (- r 1) r)
	   (set! r (- r 1)))
	 (copy perm1 perm)
	 
	 (do ((flipsCount 0 (+ flipsCount 1))
	      (k (int-vector-ref perm 0) (int-vector-ref perm 0)))
	     ((= k 0)
	      (set! maxFlipsCount (max maxFlipsCount flipsCount))
	      (if permCount 
		  (set! checksum (+ checksum flipsCount)) 
		  (set! checksum (- checksum flipsCount))))
	   (reverse! (vector-ref subs k)))
	 
	 (do ((done #f))
	     (done)
	   (when (= r n)
	     (return (cons checksum maxFlipsCount)))
	   
	   (set! perm0 (int-vector-ref perm1 0))
	   (copy (vector-ref subs1 r) perm1)
	   (int-vector-set! perm1 r perm0)

	   (if (positive? (int-vector-set! count r (- (int-vector-ref count r) 1)))
	       (set! done #t)
	       (set! r (+ r 1)))))))))

;; (fannkuch 7): (228 . 16), 8: (1616 . 22), 9: (8629 . 30), 10: (73196 . 38), 11: (556355 . 51), 12: (3968050 . 65)
(display (fannkuch 7)) (newline)
;; (fannkuch 12) takes around 5 minutes (297 secs)

;;; --------------------------------------------------------------------------------

(define (wc)
  (let ((newk 0)
	(nw 0)
	(nc 0)
	(port (open-input-file "/home/bil/test/scheme/bench/src/bib")))
    (do ((str (read-line port) (read-line port))
	 (nl 0 (+ nl 1)))
	((eof-object? str)
	 (close-input-port port)
	 (list nl (+ nw nl) nc))
      (set! nc (+ nc 1 (string-length str)))
      (set! newk 0)
      (do ((k (char-position #\space str) (char-position #\space str (+ k 1))))
	  ((not k))
	(if (not (= k newk))
	    (set! nw (+ nw 1)))
	(set! newk (+ k 1))))))

(display (wc)) (newline) ; '(31102 851820 4460056)

;;; --------------------------------------------------------------------------------

(define (cat)
  (let ((inport (open-input-file "/home/bil/test/scheme/bench/src/bib"))
	(outport (open-output-file "foo"))) 
    (catch #t
      (lambda ()
	(do () (#f) 
	  (write-string (read-line inport #t) outport)))
      (lambda args
	(close-input-port inport)
	(close-output-port outport)))))

(cat)

;;; --------------------------------------------------------------------------------

(define (string-cat n)
  (let ((s "abcdef"))
    (do ((i 0 (+ i 1)))
	((= i 10) 
	 (string-length s))
      (set! s "abcdef")
      (do ()
	  ((> (string-length s) n))
	(set! s (string-append "123" s "456" s "789"))
	(set! s (string-append
		 (substring s (quotient (string-length s) 2))
		 (substring s 0 (+ 1 (quotient (string-length s) 2)))))))))

(display (string-cat 500000)) (newline) ; 524278

;;; --------------------------------------------------------------------------------

(define mbrot
  (let ()
    (define (count cr ci)
      (let ((max-count 64)
	    (radius^2 16.0)
	    (tzr 0.0)
	    (zr cr)
	    (zi ci))
	(do ((zr^2 (* zr zr) (* zr zr))
	     (zi^2 (* zi zi) (* zi zi))
	     (c 0 (+ c 1)))
	    ((or (= c max-count)
		 (> (+ zr^2 zi^2) radius^2))
	     c)
	  (set! tzr (+ (- zr^2 zi^2) cr))
	  (set! zi (+ (* 2.0 zr zi) ci))
	  (set! zr tzr))))

    (define (mb matrix r i step n)
      (do ((x 0 (+ x 1)))
	  ((= x n))
	(do ((y 0 (+ y 1)))
	    ((= y n))
	  (vector-set! matrix x y (count (+ r (* step x)) (+ i (* step y)))))))

    (lambda (n)
      (let ((matrix (make-vector (list n n))))
	(mb matrix -1.0 -0.5 0.005 n)
	(if (not (= (matrix 0 0) 5))
	    (format #t ";mbrot: ~A~%" n))))))

(mbrot 75)

;;; --------------------------------------------------------------------------------

;;; this one is not very good

(define binary-tree
  (let ()
    (define (item-check tree)
      (if (not (car tree))
	  1
	  (+ 1 (item-check (car tree)) (item-check (cdr tree)))))
    
    (define (bottom-up-tree depth)
      (if (zero? depth)
	  (cons #f #f)
	  (cons (bottom-up-tree (- depth 1)) 
		(bottom-up-tree (- depth 1)))))
    
    (lambda (N)
      (let* ((min-depth 4)
	     (max-depth (max (+ min-depth 2) N))
	     (stretch-depth (+ max-depth 1))
	     (stretch-tree (bottom-up-tree stretch-depth)))
	(format *stderr* "stretch tree of depth ~D~30Tcheck: ~D~%" stretch-depth (item-check stretch-tree))
	(let ((long-lived-tree (bottom-up-tree max-depth)))
	  (do ((depth min-depth (+ depth 2)))
	      ((> depth max-depth))
	    (let ((iterations (expt 2 (- (+ max-depth min-depth) depth))))
	      (do ((i 0 (+ i 1))
		   (check 0 (+ check (item-check (bottom-up-tree depth)))))
		  ((= i iterations)
		   (format *stderr* "~D~9Ttrees of depth ~D~30Tcheck: ~D~%" iterations depth check)))))
	  (format *stderr* "long lived tree of depth ~D~30Tcheck: ~D~%" max-depth (item-check long-lived-tree)))))))

;(binary-tree 21) ; 30 secs if (set! (*s7* 'heap-size) (* 24 1024000))
(binary-tree 6)

;;; stretch      tree of  depth 22	 check: 8388607
;;; 2097152	 trees of depth 4	 check: 65011712
;;; 524288	 trees of depth 6	 check: 66584576
;;; 131072	 trees of depth 8	 check: 66977792
;;; 32768	 trees of depth 10	 check: 67076096
;;; 8192	 trees of depth 12	 check: 67100672
;;; 2048	 trees of depth 14	 check: 67106816
;;; 512	         trees of depth 16	 check: 67108352
;;; 128	         trees of depth 18	 check: 67108736
;;; 32	         trees of depth 20	 check: 67108832
;;; long lived   tree of  depth 21	 check: 4194303


;;; --------------------------------------------------------------------------------

(define collatz
  (let ()
    (define (collatz-count-until-1 n)
      (do ((count 0 (+ count 1)))
	  ((= n 1)
	   count)
	(if (logbit? n 0)
	    (set! n (+ (* 3 n) 1))
	    (set! n (ash n -1)))))
    (lambda (N)
      (let ((len 0)
	    (num 0)
	    (cur 0))
	(do ((i 1 (+ i 1)))
	    ((= i N)) ; 300000))
	  (set! cur (collatz-count-until-1 i))
	  (when (< len cur)
	    (set! len cur)
	    (set! num i)))
	(format *stderr* "Maximum stopping distance ~D, starting number ~D\n" len num)))))

;(collatz 300000)
;; Maximum stopping distance 442, starting number 230631
;; .66 secs
(collatz 20000)

;;; --------------------------------------------------------------------------------

(define prime?                         ; from exobrain.se
  (let ((increments (list 4 2 4 2 4 6 2 6)))
    (set-cdr! (list-tail increments 7) increments)
    (lambda (z)
      (or (memq z '(2 3 5 7))          ; memv...
	  (and (positive? (remainder z 2))
	       (positive? (remainder z 3))	  
	       (positive? (remainder z 5))	  
	       (positive? (remainder z 7))
	       (do ((L increments)
		    (lim (sqrt z))
		    (divisor 11 (+ divisor (car L))))
		   ((or (zero? (remainder z divisor))
			(>= divisor lim))
		    (> divisor lim))
		 (set! L (cdr L))))))))

(let ()
  (define (count-primes limit)          ; for limit=10000000 12.7 secs 664579
    (let ((primes 0))
      (do ((i 2 (+ i 1)))
	  ((= i limit)
	   primes)
	(if (prime? i)
	    (set! primes (+ primes 1))))))

  (display (count-primes 100000)) (newline)) ; 9592

;;; --------------------------------------------------------------------------------

(exit)
