;;; a first stab at a numerics timing test

(define dolph-1
  (let ((+documentation+ "(dolph-1 n gamma) produces a Dolph-Chebyshev FFT data window of 'n' points using 'gamma' as the window parameter."))
    (lambda (N gamma)
      (let ((vals (make-vector N)))
	(let ((alpha (cosh (/ (acosh (expt 10.0 gamma)) N))))
	  (do ((den (/ 1.0 (cosh (* N (acosh alpha)))))
	       (freq (/ pi N))
	       (mult -1 (- mult))
	       (i 0 (+ i 1))
	       (phase (* -0.5 pi)))
	      ((= i N))
	    (set! (vals i) (* mult den (cos (* N (acos (* alpha (cos phase)))))))
	    (set! phase (+ phase freq))))
	;; now take the DFT
	(let ((pk 0.0)
	      (w (make-vector N)))
	  (do ((i 0 (+ i 1))
	       (sum 0.0 0.0))
	      ((= i N))
	    (do ((k 0 (+ k 1)))
		((= k N))
	      (set! sum (+ sum (* (vals k) (exp (/ (* 2.0 0+1.0i pi k i) N))))))
	    (set! (w i) (magnitude sum))
	    (set! pk (max pk (w i))))
	  ;; scale to 1.0 (it's usually pretty close already, that is pk is close to 1.0)
	  (do ((i 0 (+ i 1)))
	      ((= i N))
	    (set! (w i) (/ (w i) pk)))
	  w)))))

(dolph-1 (expt 2 10) 0.5)


(define src-duration
  (let ((+documentation+ "(src-duration envelope) returns the new duration of a sound after using 'envelope' for time-varying sampling-rate conversion"))
    (lambda (e)
      (let ((len (- (length e) 2)))
	(do ((all-x (- (e len) (e 0))) ; last x - first x
	     (dur 0.0)
	     (i 0 (+ i 2)))
	    ((>= i len) dur)
	  (let ((area (let ((x0 (e i))
			    (x1 (e (+ i 2)))
			    (y0 (e (+ i 1))) ; 1/x x points
			    (y1 (e (+ i 3))))
			(if (< (abs (real-part (- y0 y1))) .0001)
			    (/ (- x1 x0) (* y0 all-x))
			    (/ (* (log (/ y1 y0)) 
				  (- x1 x0)) 
			       (* (- y1 y0) all-x))))))
	    (set! dur (+ dur (abs area)))))))))


(define (src-test)
  (src-duration (do ((env (make-float-vector 7000))
		     (i 0 (+ i 2)))
		    ((= i 7000) env)
		  (set! (env i) i)
		  (set! (env (+ i 1)) (+ .1 (random 1.0))))))
(src-test)


(define invert-matrix
  (let ((+documentation+ "(invert-matrix matrix b (zero 1.0e-7)) inverts 'matrix'"))
    (lambda* (matrix b (zero 1.0e-7))
      ;; translated from Numerical Recipes (gaussj)
      (call-with-exit
       (lambda (return)
	 (let ((n (car (vector-dimensions matrix))))
	   (let ((cols (make-int-vector n 0))
		 (rows (make-int-vector n 0))
		 (pivots (make-int-vector n 0)))
	     (do ((i 0 (+ i 1))
		  (col 0 0)
		  (row 0 0))
		 ((= i n))
	       (do ((biggest 0.0)
		    (j 0 (+ j 1)))
		   ((= j n)
		    (if (< biggest zero) 
			(return #f))) ; this can be fooled (floats...): (invert-matrix (subvector (float-vector 1 2 3 3 2 1 4 5 6) 0 9 (list 3 3)))
		 (if (not (= (pivots j) 1))
		     (do ((k 0 (+ k 1)))
			 ((= k n))
		       (if (= (pivots k) 0)
			   (let ((val (abs (matrix j k))))
			     (if (> val biggest)
				 (begin
				   (set! col k)
				   (set! row j)
				   (set! biggest val))))
			   (if (> (pivots k) 1)
			       (return #f))))))
	       (set! (pivots col) (+ (pivots col) 1))
	       (if (not (= row col))
		   (let ((temp (if (sequence? b) (b row) 0.0)))
		     (if (sequence? b)
			 (begin
			   (set! (b row) (b col))
			   (set! (b col) temp)))
		     (do ((k 0 (+ k 1)))
			 ((= k n))
		       (set! temp (matrix row k))
		       (set! (matrix row k) (matrix col k))
		       (set! (matrix col k) temp))))
	       (set! (cols i) col)
	       (set! (rows i) row)
	       ;; round-off troubles here
	       (if (< (abs (matrix col col)) zero)
		   (return #f))
	       (let ((inverse-pivot (/ 1.0 (matrix col col))))
		 (set! (matrix col col) 1.0)
		 (do ((k 0 (+ k 1)))
		     ((= k n))
		   (set! (matrix col k) (* inverse-pivot (matrix col k))))
		 (if b (set! (b col) (* inverse-pivot (b col)))))
	       (do ((k 0 (+ k 1)))
		   ((= k n))
		 (if (not (= k col))
		     (let ((scl (matrix k col)))
		       (set! (matrix k col) 0.0)
		       (do ((m 0 (+ 1 m)))
			   ((= m n))
			 (set! (matrix k m) (- (matrix k m) (* scl (matrix col m)))))
		       (if b (set! (b k) (- (b k) (* scl (b col)))))))))
	     (do ((i (- n 1) (- i 1)))
		 ((< i 0))
	       (if (not (= (rows i) (cols i)))
		   (do ((k 0 (+ k 1)))
		       ((= k n))
		     (let ((temp (matrix k (rows i))))
		       (set! (matrix k (rows i)) (matrix k (cols i)))
		       (set! (matrix k (cols i)) temp)))))
	     (list matrix b))))))))

(define (matrix-solve A b)
  (cond ((invert-matrix A b) => cadr) (else #f)))

(define (invert-test)
  (matrix-solve (do ((A (make-float-vector '(100 100)))
		     (i 0 (+ i 1)))
		    ((= i 100) A)
		  (do ((j 0 (+ j 1)))
		      ((= j 100))
		    (set! (A i j) (random 1.0))))
		(do ((b (make-float-vector 100))
		     (i 0 (+ i 1)))
		    ((= i 100) b)
		  (set! (b i) (random 1.0)))))
(invert-test)


(define* (cfft data n (dir 1))
  (if (not n) (set! n (length data)))
  (do ((i 0 (+ i 1))
       (j 0))
      ((= i n))
    (if (> j i)
	(let ((temp (data j)))
	  (set! (data j) (data i))
	  (set! (data i) temp)))
    (do ((m (/ n 2)))
	((or (< m 2) (< j m))
	 (set! j (+ j m)))
      (set! j (- j m))
      (set! m (/ m 2))))
  
  (let ((ipow (floor (log n 2)))
	(prev 1))
    (do ((lg 0 (+ lg 1))
	 (mmax 2 (* mmax 2))
	 (pow (/ n 2) (/ pow 2))
	 (theta (complex 0.0 (* pi dir)) (* theta 0.5)))
	((= lg ipow))
      (let ((wpc (exp theta))
	    (wc 1.0))
	(do ((ii 0 (+ ii 1)))
	    ((= ii prev))
	  (do ((jj 0 (+ jj 1))
	       (i ii (+ i mmax))
	       (j (+ ii prev) (+ j mmax)))
	      ((>= jj pow))
	    (let ((tc (* wc (data j))))
	      (set! (data j) (- (data i) tc))
	      (set! (data i) (+ (data i) tc))))
	  (set! wc (* wc wpc)))
	(set! prev mmax))))
  
  data)

(define (cfft-test)
  (let* ((size (expt 2 16))
	 (data (make-vector size)))
    (do ((i 0 (+ i 1)))
	((= i size))
      (set! (data i) (complex (- 1.0 (random 2.0)) (- 1.0 (random 2.0)))))
    (cfft data size)))

(cfft-test)


;; these Bessel functions are from Numerical Recipes
(define (bes-j0-1 x)				;returns J0(x) for any real x
  (if (< (abs x) 8.0)
      (let ((y (* x x)))
	(let ((ans1 (+ 57568490574.0000 (* y (- (* y (+ 651619640.7 (* y (- (* y (+ 77392.33017 (* y -184.9052456))) 11214424.18)))) 13362590354.0))))
	      (ans2 (+ 57568490411.0 
		       (* y (+ 1029532985.0 
			       (* y (+ 9494680.718
				       (* y (+ 59272.64853
					       (* y (+ 267.8532712 y)))))))))))
	  (/ ans1 ans2)))
      (let* ((ax (abs x))
	     (z (/ 8.0 ax))
	     (y (* z z)))
	(let ((xx (- ax 0.785398164))
	      (ans1 (+ 1.0 (* y (- (* y (+ 2.734510407e-05 (* y (- (* y 2.093887211e-07) 2.073370639e-06)))) 0.001098628627))))
	      (ans2 (- (* y (+ 0.0001 (* y (- (* y (+ 7.621095160999999e-07 (* y -9.34945152e-08))) 6.911147651000001e-06)))) 0.0156)))
	  (* (sqrt (/ 0.636619772 ax))
	     (- (* ans1 (cos xx))
		(* z (sin xx) ans2)))))))

(define bes-j1-1 
  (let ((signum (lambda (x) (if (= x 0.0) 0 (if (< x 0.0) -1 1)))))
    (lambda (x)				;returns J1(x) for any real x
      (if (< (abs x) 8.0)
	  (let ((y (* x x)))
	    (let ((ans1 (* x (+ 72362614232.0000 (* y (- (* y (+ 242396853.1 (* y (- (* y (+ 15704.4826 (* y -30.16036606))) 2972611.439)))) 7895059235.0)))))
		  (ans2 (+ 144725228442.0 
			   (* y (+ 2300535178.0 
				   (* y (+ 18583304.74
					   (* y (+ 99447.43394
						   (* y (+ 376.9991397 y)))))))))))
	      (/ ans1 ans2)))
	  (let* ((ax (abs x))
		 (z (/ 8.0 ax))
		 (y (* z z)))
	    (let ((xx (- ax 2.356194491))
		  (ans1 (+ 1.0 (* y (+ 0.00183105 (* y (- (* y (+ 2.457520174e-06 (* y -2.40337019e-07))) 3.516396496e-05))))))
		  (ans2 (+ 0.0469 (* y (- (* y (+ 8.449199096000001e-06 (* y (- (* y 1.05787412e-07) 8.8228987e-07)))) 0.0002002690873)))))
	      (* (signum x)
		 (sqrt (/ 0.636619772 ax))
		 (- (* ans1 (cos xx))
		    (* z (sin xx) ans2)))))))))

(define (bes-jn nn x)
  (let ((besn (let ((n (abs nn)))
		(cond ((= n 0) (bes-j0-1 x))
		      ((= n 1) (bes-j1-1 x))
		      ((= x 0.0) 0.0)
		      (else
		       (let ((iacc 40)
			     (ans 0.0000)
			     (bigno 1.0e10)
			     (bigni 1.0e-10))
			 (if (> (abs x) n)
			     (do ((tox (/ 2.0 (abs x)))
				  (bjm (bes-j0-1 (abs x)))
				  (bj (bes-j1-1 (abs x)))
				  (j 1 (+ j 1))
				  (bjp 0.0))
				 ((= j n) (set! ans bj))
			       (set! bjp (- (* j tox bj) bjm))
			       (set! bjm bj)
			       (set! bj bjp))
			     (let ((tox (/ 2.0 (abs x)))
				   (m (* 2 (floor (/ (+ n (sqrt (* iacc n))) 2))))
				   (jsum 0)
				   (bjm 0.0000)
				   (sum 0.0000)
				   (bjp 0.0000)
				   (bj 1.0000))
			       (do ((j m (- j 1)))
				   ((= j 0))
				 (set! bjm (- (* j tox bj) bjp))
				 (set! bjp bj)
				 (set! bj bjm)
				 (when (> (abs bj) bigno)
				   (set! bj (* bj bigni))
				   (set! bjp (* bjp bigni))
				   (set! ans (* ans bigni))
				   (set! sum (* sum bigni)))
				 (if (not (= 0 jsum))
				     (set! sum (+ sum bj)))
				 (set! jsum (- 1 jsum))
				 (if (= j n) (set! ans bjp)))
			       (set! ans (/ ans (- (* 2.0 sum) bj)))))
			 (if (and (< x 0.0) (odd? n))
			     (- ans)
			     ans)))))))
    (if (and (< nn 0)
	     (odd? nn))
	(- besn)
	besn)))

(define (bes-i0 x)
  (if (< (abs x) 3.75)
      (let ((y (expt (/ x 3.75) 2)))
	(+ 1.0
	   (* y (+ 3.5156229
		   (* y (+ 3.0899424
			   (* y (+ 1.2067492
				   (* y (+ 0.2659732
					   (* y (+ 0.360768e-1
						   (* y 0.45813e-2)))))))))))))
      (let* ((ax (abs x))
	     (y (/ 3.75 ax)))
	(* (/ (exp ax) (sqrt ax))
	   (+ 0.3989 (* y (+ 0.0133 
			     (* y (+ 0.0023 
				     (* y (- (* y (+ 0.0092 
						     (* y (- (* y (+ 0.02635537 
								     (* y (- (* y 0.00392377) 0.01647633)))) 
							     0.02057706)))) 
					     0.0016)))))))))))

(define (bes-i1 x)				;I1(x)
  (if (< (abs x) 3.75)
      (let ((y (expt (/ x 3.75) 2)))
	(* x (+ 0.5
		(* y (+ 0.87890594
			(* y (+ 0.51498869
				(* y (+ 0.15084934
					(* y (+ 0.2658733e-1
						(* y (+ 0.301532e-2
							(* y 0.32411e-3))))))))))))))
      (let ((ax (abs x)))
	(let ((ans2 (let* ((y (/ 3.75 ax))
			   (ans1 (+ 0.02282967 (* y (- (* y (+ 0.01787654 (* y -0.00420059))) 0.02895312)))))
		      (+ 0.39894228 (* y (- (* y (- (* y (+ 0.00163801 (* y (- (* y ans1) 0.01031555)))) 0.00362018)) 0.03988024)))))
	      (sign (if (< x 0.0) -1.0 1.0)))
	  (/ (* (exp ax) ans2 sign) (sqrt ax))))))

(define (bes-in n x)
  (cond ((= n 0) (bes-i0 x))
	((= n 1) (bes-i1 x))
	((= x 0.0) 0.0)
	(else
	 (let ((bigno 1.0e10)
	       (bigni 1.0e-10)
	       (ans 0.0000)
	       (tox (/ 2.0 (abs x)))
	       (bip 0.0000)
	       (bi 1.0000)
	       (m (* 2 (+ n (truncate (sqrt (* 40 n)))))) ; iacc=40
	       (bim 0.0000))
	   (do ((j m (- j 1)))
	       ((= j 0))
	     (set! bim (+ bip (* j tox bi)))
	     (set! bip bi)
	     (set! bi bim)
	     (when (> (abs bi) bigno)
	       (set! ans (* ans bigni))
	       (set! bi (* bi bigni))
	       (set! bip (* bip bigni)))
	     (if (= j n) (set! ans bip)))
	   (if (and (< x 0.0) (odd? n))
	       (set! ans (- ans)))
	   (* ans (/ (bes-i0 x) bi))))))


(define (fm-complex-component freq-we-want wc wm a b interp using-sine)
  (let ((sum 0.0)
	(mxa (ceiling (* 7 a)))
	(mxb (ceiling (* 7 b))))
    (do ((k (- mxa) (+ k 1)))
	((>= k mxa))
      (do ((j (- mxb) (+ j 1)))
	  ((>= j mxb))
	(if (< (abs (- freq-we-want wc (* k wm) (* j wm))) 0.1)
	    (let ((curJI (* (bes-jn k a)
			    (bes-in (abs j) b)
			    (expt 0.0+1.0i j))))
	      (set! sum (+ sum curJI))))))
    (list sum
	  (+ (* (- 1.0 interp) (real-part sum))
	     (* interp (imag-part sum))))))

(define (fm-cascade-component freq-we-want wc wm1 a wm2 b)
  (let ((sum 0.0)
	(mxa (ceiling (* 7 a)))
	(mxb (ceiling (* 7 b))))
    (do ((k (- mxa) (+ k 1)))
	((>= k mxa))
      (do ((j (- mxb) (+ j 1)))
	  ((>= j mxb))
	(if (< (abs (- freq-we-want wc (* k wm1) (* j wm2))) 0.1)
	    (let ((curJJ (* (bes-jn k a)
			    (bes-jn j (* k b)))))
	      (set! sum (+ sum curJJ))))))
    sum))

(define (test-fm-components)
  (do ((i 0.0 (+ i .1)))
      ((>= i 3.0))
    (do ((k 1.0 (+ k 1.0)))
	((>= k 10.0))
      (fm-complex-component 1200 1000 100 i k 0.0 #f)
      (fm-cascade-component 1200 1000 100 i 50 k))))

(test-fm-components)



(exit)
