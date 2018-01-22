;;; examples of using the GL bindings

(provide 'snd-snd-gl.scm)

(with-let *gl*

;;; ---------------- gl-info ----------------
(define gl-info
  ;; taken loosely from glxvisuals.c
  (let ((+documentation+ "(gl-info) prints out GL-related info")
	(class-of (lambda (n)
		    (and (number? n)
			 (copy (cond ((= n (*motif* 'StaticGray)) "static-gray")
				     ((= n (*motif* 'GrayScale)) "gray-scale")
				     ((= n (*motif* 'StaticColor)) "static-color")
				     ((= n (*motif* 'PseudoColor)) "pseudo-color")
				     ((= n (*motif* 'TrueColor)) "true-color")
				     ((= n (*motif* 'DirectColor)) "direct-color")
				     (#t "??")))))))
    (lambda ()
      (let* ((cx (snd-gl-context))
	     (dpy ((*motif* 'XtDisplay) (cadr (main-widgets))))
	     (version (glXQueryVersion dpy 0 0)))
	(if (not (car version))
	    (snd-print "no GL found!")
	    (let ((visuals ((*motif* 'XGetVisualInfo) dpy 0 (list 'XVisualInfo 0))))
	      (glXMakeCurrent dpy ((*motif* 'XtWindow) (cadr (main-widgets))) cx)
	      (snd-print (format #f "GL version: ~A.~A, (~A ~A ~A)~%"
				 (cadr version) (caddr version)
				 (glGetString GL_VENDOR) (glGetString GL_RENDERER) (glGetString GL_VERSION)))
	      (snd-print (format #f "  with: ~A~A~%"
				 (glXQueryExtensionsString dpy ((*motif* 'XScreenNumberOfScreen) ((*motif* 'DefaultScreenOfDisplay) dpy)))
				 (if (glXIsDirect dpy cx) ", direct rendering support" "")))
	      (for-each 
	       (lambda (visual)
		 (when (= (cadr (glXGetConfig dpy visual GLX_USE_GL)) 1)
		   ;; found a visual that can support GL
		   (let ((buffersize (cadr (glXGetConfig dpy visual GLX_BUFFER_SIZE)))
			 (level (cadr (glXGetConfig dpy visual GLX_LEVEL)))
			 (rgba (cadr (glXGetConfig dpy visual GLX_RGBA)))
			 (doublebuffer (cadr (glXGetConfig dpy visual GLX_DOUBLEBUFFER)))
			 (stereo (cadr (glXGetConfig dpy visual GLX_STEREO)))
			 (auxbuffers (cadr (glXGetConfig dpy visual GLX_AUX_BUFFERS)))
			 (redsize (cadr (glXGetConfig dpy visual GLX_RED_SIZE)))
			 (bluesize (cadr (glXGetConfig dpy visual GLX_BLUE_SIZE)))
			 (greensize (cadr (glXGetConfig dpy visual GLX_GREEN_SIZE)))
			 (alphasize (cadr (glXGetConfig dpy visual GLX_ALPHA_SIZE)))
			 (depthsize (cadr (glXGetConfig dpy visual GLX_DEPTH_SIZE)))
			 (stencilsize (cadr (glXGetConfig dpy visual GLX_STENCIL_SIZE)))
			 (acredsize (cadr (glXGetConfig dpy visual GLX_ACCUM_RED_SIZE)))
			 (acgreensize (cadr (glXGetConfig dpy visual GLX_ACCUM_GREEN_SIZE)))
			 (acbluesize (cadr (glXGetConfig dpy visual GLX_ACCUM_BLUE_SIZE)))
			 (acalphasize (cadr (glXGetConfig dpy visual GLX_ACCUM_ALPHA_SIZE))))
		     (for-each snd-print (vector (format #f "  id: #x~X depth: ~D class: ~S~%" 
							 ((*motif* '.visualid) visual) ((*motif* '.depth) visual) (class-of ((*motif* '.class) visual)))
						 (format #f "      buffersize: ~D, level: ~D, rgba: ~A, doublebuffer: ~A, stereo: ~A~%"
							 buffersize level
							 (if (= rgba 1) "#t" "#f")
							 (if (= doublebuffer 1) "#t" "#f")
							 (if (= stereo 1) "#t" "#f"))
						 (format #f "      r: ~A, g: ~D, b: ~D, alpha: ~D, accum-r: ~D, accum-g: ~D, accum-b: ~D, accum-alpha: ~D~%"
							 redsize greensize bluesize alphasize 
							 acredsize acgreensize acbluesize acalphasize)
						 (format #f "      auxbuffs: ~D, depth: ~D, acalpha: ~D~%"
							 auxbuffers depthsize stencilsize))))))
	       visuals)))))))



;;; -------- dump GL state

(define (gl-dump-state)
  ;; based on Mesa/util/dumpstate.c by Stephane Rehel

  (format () "GL_CURRENT_COLOR: ~A~%" (glGetFloatv GL_CURRENT_COLOR))
  (format () "GL_CURRENT_INDEX: ~A~%" (glGetIntegerv GL_CURRENT_INDEX))
  (format () "GL_CURRENT_TEXTURE_COORDS: ~A~%" (glGetFloatv GL_CURRENT_TEXTURE_COORDS))
  (format () "GL_CURRENT_NORMAL: ~A~%" (glGetFloatv GL_CURRENT_NORMAL))
  (format () "GL_CURRENT_RASTER_POSITION: ~A~%" (glGetFloatv GL_CURRENT_RASTER_POSITION))
  (format () "GL_CURRENT_RASTER_DISTANCE: ~A~%" (glGetFloatv GL_CURRENT_RASTER_DISTANCE))
  (format () "GL_CURRENT_RASTER_COLOR: ~A~%" (glGetFloatv GL_CURRENT_RASTER_COLOR))
  (format () "GL_CURRENT_RASTER_INDEX: ~A~%" (glGetIntegerv GL_CURRENT_RASTER_INDEX))
  (format () "GL_CURRENT_RASTER_TEXTURE_COORDS: ~A~%" (glGetFloatv GL_CURRENT_RASTER_TEXTURE_COORDS))
  (format () "GL_CURRENT_RASTER_POSITION_VALID: ~A~%" (glGetBooleanv GL_CURRENT_RASTER_POSITION_VALID))
  (format () "GL_EDGE_FLAG: ~A~%" (glGetBooleanv GL_EDGE_FLAG))
  (format () "GL_VERTEX_ARRAY: ~A~%" (glGetBooleanv GL_VERTEX_ARRAY))
  (format () "GL_VERTEX_ARRAY_SIZE: ~A~%" (glGetIntegerv GL_VERTEX_ARRAY_SIZE))
  (format () "GL_VERTEX_ARRAY_TYPE: ~A~%" (glGetIntegerv GL_VERTEX_ARRAY_TYPE))
  (format () "GL_VERTEX_ARRAY_STRIDE: ~A~%" (glGetIntegerv GL_VERTEX_ARRAY_STRIDE))
  (format () "GL_VERTEX_ARRAY_POINTER: ~A~%" (glGetPointerv GL_VERTEX_ARRAY_POINTER))
  (format () "GL_NORMAL_ARRAY: ~A~%" (glGetBooleanv GL_NORMAL_ARRAY))
  (format () "GL_NORMAL_ARRAY_TYPE: ~A~%" (glGetIntegerv GL_NORMAL_ARRAY_TYPE))
  (format () "GL_NORMAL_ARRAY_STRIDE: ~A~%" (glGetIntegerv GL_NORMAL_ARRAY_STRIDE))
  (format () "GL_NORMAL_ARRAY_POINTER: ~A~%" (glGetPointerv GL_NORMAL_ARRAY_POINTER))
  (format () "GL_COLOR_ARRAY: ~A~%" (glGetBooleanv GL_COLOR_ARRAY))
  (format () "GL_COLOR_ARRAY_SIZE: ~A~%" (glGetIntegerv GL_COLOR_ARRAY_SIZE))
  (format () "GL_COLOR_ARRAY_TYPE: ~A~%" (glGetIntegerv GL_COLOR_ARRAY_TYPE))
  (format () "GL_COLOR_ARRAY_STRIDE: ~A~%" (glGetIntegerv GL_COLOR_ARRAY_STRIDE))
  (format () "GL_COLOR_ARRAY_POINTER: ~A~%" (glGetPointerv GL_COLOR_ARRAY_POINTER))
  (format () "GL_INDEX_ARRAY: ~A~%" (glGetBooleanv GL_INDEX_ARRAY))
  (format () "GL_INDEX_ARRAY_TYPE: ~A~%" (glGetIntegerv GL_INDEX_ARRAY_TYPE))
  (format () "GL_INDEX_ARRAY_STRIDE: ~A~%" (glGetIntegerv GL_INDEX_ARRAY_STRIDE))
  (format () "GL_INDEX_ARRAY_POINTER: ~A~%" (glGetPointerv GL_INDEX_ARRAY_POINTER))
  (format () "GL_TEXTURE_COORD_ARRAY: ~A~%" (glGetBooleanv GL_TEXTURE_COORD_ARRAY))
  (format () "GL_TEXTURE_COORD_ARRAY_SIZE: ~A~%" (glGetIntegerv GL_TEXTURE_COORD_ARRAY_SIZE))
  (format () "GL_TEXTURE_COORD_ARRAY_TYPE: ~A~%" (glGetIntegerv GL_TEXTURE_COORD_ARRAY_TYPE))
  (format () "GL_TEXTURE_COORD_ARRAY_STRIDE: ~A~%" (glGetIntegerv GL_TEXTURE_COORD_ARRAY_STRIDE))
  (format () "GL_TEXTURE_COORD_ARRAY_POINTER: ~A~%" (glGetPointerv GL_TEXTURE_COORD_ARRAY_POINTER))
  (format () "GL_EDGE_FLAG_ARRAY: ~A~%" (glGetBooleanv GL_EDGE_FLAG_ARRAY))
  (format () "GL_EDGE_FLAG_ARRAY_STRIDE: ~A~%" (glGetIntegerv GL_EDGE_FLAG_ARRAY_STRIDE))
  (format () "GL_EDGE_FLAG_ARRAY_POINTER: ~A~%" (glGetPointerv GL_EDGE_FLAG_ARRAY_POINTER))
  (format () "GL_MODELVIEW_MATRIX: ~A~%" (glGetFloatv GL_MODELVIEW_MATRIX))
  (format () "GL_PROJECTION_MATRIX: ~A~%" (glGetFloatv GL_PROJECTION_MATRIX))
  (format () "GL_TEXTURE_MATRIX: ~A~%" (glGetFloatv GL_TEXTURE_MATRIX))
  (format () "GL_VIEWPORT: ~A~%" (glGetIntegerv GL_VIEWPORT))
  (format () "GL_DEPTH_RANGE: ~A~%" (glGetFloatv GL_DEPTH_RANGE))
  (format () "GL_MODELVIEW_STACK_DEPTH: ~A~%" (glGetIntegerv GL_MODELVIEW_STACK_DEPTH))
  (format () "GL_PROJECTION_STACK_DEPTH: ~A~%" (glGetIntegerv GL_PROJECTION_STACK_DEPTH))
  (format () "GL_TEXTURE_STACK_DEPTH: ~A~%" (glGetIntegerv GL_TEXTURE_STACK_DEPTH))
  (format () "GL_MATRIX_MODE: ~A~%" (glGetIntegerv GL_MATRIX_MODE))
  (format () "GL_NORMALIZE: ~A~%" (glGetBooleanv GL_NORMALIZE))
  (format () "GL_CLIP_PLANE0: ~A~%" (glGetBooleanv GL_CLIP_PLANE0))
  (format () "GL_CLIP_PLANE1: ~A~%" (glGetBooleanv GL_CLIP_PLANE1))
  (format () "GL_CLIP_PLANE2: ~A~%" (glGetBooleanv GL_CLIP_PLANE2))
  (format () "GL_CLIP_PLANE3: ~A~%" (glGetBooleanv GL_CLIP_PLANE3))
  (format () "GL_CLIP_PLANE4: ~A~%" (glGetBooleanv GL_CLIP_PLANE4))
  (format () "GL_CLIP_PLANE5: ~A~%" (glGetBooleanv GL_CLIP_PLANE5))
  (format () "GL_FOG_COLOR: ~A~%" (glGetFloatv GL_FOG_COLOR))
  (format () "GL_FOG_INDEX: ~A~%" (glGetIntegerv GL_FOG_INDEX))
  (format () "GL_FOG_DENSITY: ~A~%" (glGetFloatv GL_FOG_DENSITY))
  (format () "GL_FOG_START: ~A~%" (glGetFloatv GL_FOG_START))
  (format () "GL_FOG_END: ~A~%" (glGetFloatv GL_FOG_END))
  (format () "GL_FOG_MODE: ~A~%" (glGetIntegerv GL_FOG_MODE))
  (format () "GL_FOG: ~A~%" (glGetBooleanv GL_FOG))
  (format () "GL_SHADE_MODEL: ~A~%" (glGetIntegerv GL_SHADE_MODEL))
  (format () "GL_LIGHTING: ~A~%" (glGetBooleanv GL_LIGHTING))
  (format () "GL_COLOR_MATERIAL: ~A~%" (glGetBooleanv GL_COLOR_MATERIAL))
  (format () "GL_COLOR_MATERIAL_PARAMETER: ~A~%" (glGetIntegerv GL_COLOR_MATERIAL_PARAMETER))
  (format () "GL_COLOR_MATERIAL_FACE: ~A~%" (glGetIntegerv GL_COLOR_MATERIAL_FACE))
  (format () "GL_BACK GL_AMBIENT: ~A~%" (glGetMaterialfv GL_BACK GL_AMBIENT))
  (format () "GL_FRONT GL_AMBIENT: ~A~%" (glGetMaterialfv GL_FRONT GL_AMBIENT))
  (format () "GL_BACK GL_DIFFUSE: ~A~%" (glGetMaterialfv GL_BACK GL_DIFFUSE))
  (format () "GL_FRONT GL_DIFFUSE: ~A~%" (glGetMaterialfv GL_FRONT GL_DIFFUSE))
  (format () "GL_BACK GL_SPECULAR: ~A~%" (glGetMaterialfv GL_BACK GL_SPECULAR))
  (format () "GL_FRONT GL_SPECULAR: ~A~%" (glGetMaterialfv GL_FRONT GL_SPECULAR))
  (format () "GL_BACK GL_EMISSION: ~A~%" (glGetMaterialfv GL_BACK GL_EMISSION))
  (format () "GL_FRONT GL_EMISSION: ~A~%" (glGetMaterialfv GL_FRONT GL_EMISSION))
  (format () "GL_BACK GL_SHININESS: ~A~%" (glGetMaterialfv GL_BACK GL_SHININESS))
  (format () "GL_FRONT GL_SHININESS: ~A~%" (glGetMaterialfv GL_FRONT GL_SHININESS))
  (format () "GL_LIGHT_MODEL_AMBIENT: ~A~%" (glGetFloatv GL_LIGHT_MODEL_AMBIENT))
  (format () "GL_LIGHT_MODEL_LOCAL_VIEWER: ~A~%" (glGetBooleanv GL_LIGHT_MODEL_LOCAL_VIEWER))
  (format () "GL_LIGHT_MODEL_TWO_SIDE: ~A~%" (glGetBooleanv GL_LIGHT_MODEL_TWO_SIDE))

  (let ((nlights (car (glGetIntegerv GL_MAX_LIGHTS))))
    (do ((i 0 (+ 1 i)))
	((= i nlights))
      (if (car (glGetBooleanv (+ GL_LIGHT0  i)))
	  (begin

	    (glGetFloatv i AMBIENT)
	    (glGetFloatv i DIFFUSE)
	    (glGetFloatv i SPECULAR)
	    (for-each 
	     (lambda (arg) 
	       (glGetLightfv i arg)) 
	     (vector POSITION CONSTANT_ATTENUATION LINEAR_ATTENUATION QUADRATIC_ATTENUATION SPOT_DIRECTION SPOT_EXPONENT SPOT_CUTOFF))))))

  (format () "GL_POINT_SIZE: ~A~%" (glGetFloatv GL_POINT_SIZE))
  (format () "GL_POINT_SMOOTH: ~A~%" (glGetBooleanv GL_POINT_SMOOTH))
  (format () "GL_LINE_WIDTH: ~A~%" (glGetFloatv GL_LINE_WIDTH))
  (format () "GL_LINE_SMOOTH: ~A~%" (glGetBooleanv GL_LINE_SMOOTH))
  (format () "GL_LINE_STIPPLE_PATTERN: ~A~%" (glGetIntegerv GL_LINE_STIPPLE_PATTERN))
  (format () "GL_LINE_STIPPLE_REPEAT: ~A~%" (glGetIntegerv GL_LINE_STIPPLE_REPEAT))
  (format () "GL_LINE_STIPPLE: ~A~%" (glGetBooleanv GL_LINE_STIPPLE))
  (format () "GL_CULL_FACE: ~A~%" (glGetBooleanv GL_CULL_FACE))
  (format () "GL_CULL_FACE_MODE: ~A~%" (glGetIntegerv GL_CULL_FACE_MODE))
  (format () "GL_FRONT_FACE: ~A~%" (glGetIntegerv GL_FRONT_FACE))
  (format () "GL_POLYGON_SMOOTH: ~A~%" (glGetBooleanv GL_POLYGON_SMOOTH))
  (format () "GL_POLYGON_MODE: ~A~%" (glGetIntegerv GL_POLYGON_MODE))
  (format () "GL_POLYGON_OFFSET_FACTOR: ~A~%" (glGetFloatv GL_POLYGON_OFFSET_FACTOR))
  (format () "GL_POLYGON_OFFSET_UNITS: ~A~%" (glGetFloatv GL_POLYGON_OFFSET_UNITS))
  (format () "GL_POLYGON_OFFSET_POINT: ~A~%" (glGetBooleanv GL_POLYGON_OFFSET_POINT))
  (format () "GL_POLYGON_OFFSET_LINE: ~A~%" (glGetBooleanv GL_POLYGON_OFFSET_LINE))
  (format () "GL_POLYGON_OFFSET_FILL: ~A~%" (glGetBooleanv GL_POLYGON_OFFSET_FILL))
  (format () "GL_POLYGON_STIPPLE: ~A~%" (glGetBooleanv GL_POLYGON_STIPPLE))
  (format () "GL_TEXTURE_1D: ~A~%" (glGetBooleanv GL_TEXTURE_1D))
  (format () "GL_TEXTURE_2D: ~A~%" (glGetBooleanv GL_TEXTURE_2D))
  (format () "GL_TEXTURE_BINDING_1D: ~A~%" (glGetIntegerv GL_TEXTURE_BINDING_1D))
  (format () "GL_TEXTURE_BINDING_2D: ~A~%" (glGetIntegerv GL_TEXTURE_BINDING_2D))
  (format () "GL_TEXTURE_GEN_S: ~A~%" (glGetBooleanv GL_TEXTURE_GEN_S))
  (format () "GL_TEXTURE_GEN_T: ~A~%" (glGetBooleanv GL_TEXTURE_GEN_T))
  (format () "GL_TEXTURE_GEN_R: ~A~%" (glGetBooleanv GL_TEXTURE_GEN_R))
  (format () "GL_TEXTURE_GEN_Q: ~A~%" (glGetBooleanv GL_TEXTURE_GEN_Q))
  (format () "GL_SCISSOR_TEST: ~A~%" (glGetBooleanv GL_SCISSOR_TEST))
  (format () "GL_SCISSOR_BOX: ~A~%" (glGetIntegerv GL_SCISSOR_BOX))
  (format () "GL_ALPHA_TEST: ~A~%" (glGetBooleanv GL_ALPHA_TEST))
  (format () "GL_ALPHA_TEST_FUNC: ~A~%" (glGetIntegerv GL_ALPHA_TEST_FUNC))
  (format () "GL_ALPHA_TEST_REF: ~A~%" (glGetFloatv GL_ALPHA_TEST_REF))
  (format () "GL_STENCIL_TEST: ~A~%" (glGetBooleanv GL_STENCIL_TEST))
  (format () "GL_STENCIL_FUNC: ~A~%" (glGetIntegerv GL_STENCIL_FUNC))
  (format () "GL_STENCIL_VALUE_MASK: ~A~%" (glGetIntegerv GL_STENCIL_VALUE_MASK))
  (format () "GL_STENCIL_REF: ~A~%" (glGetIntegerv GL_STENCIL_REF))
  (format () "GL_STENCIL_FAIL: ~A~%" (glGetIntegerv GL_STENCIL_FAIL))
  (format () "GL_STENCIL_PASS_DEPTH_FAIL: ~A~%" (glGetIntegerv GL_STENCIL_PASS_DEPTH_FAIL))
  (format () "GL_STENCIL_PASS_DEPTH_PASS: ~A~%" (glGetIntegerv GL_STENCIL_PASS_DEPTH_PASS))
  (format () "GL_DEPTH_TEST: ~A~%" (glGetBooleanv GL_DEPTH_TEST))
  (format () "GL_DEPTH_FUNC: ~A~%" (glGetIntegerv GL_DEPTH_FUNC))
  (format () "GL_BLEND: ~A~%" (glGetBooleanv GL_BLEND))
  (format () "GL_BLEND_SRC: ~A~%" (glGetIntegerv GL_BLEND_SRC))
  (format () "GL_BLEND_DST: ~A~%" (glGetIntegerv GL_BLEND_DST))
  (format () "GL_DITHER: ~A~%" (glGetBooleanv GL_DITHER))
  (format () "GL_LOGIC_OP: ~A~%" (glGetBooleanv GL_LOGIC_OP))
  (format () "GL_COLOR_LOGIC_OP: ~A~%" (glGetBooleanv GL_COLOR_LOGIC_OP))
  (format () "GL_DRAW_BUFFER: ~A~%" (glGetIntegerv GL_DRAW_BUFFER))
  (format () "GL_INDEX_WRITEMASK: ~A~%" (glGetIntegerv GL_INDEX_WRITEMASK))
  (format () "GL_COLOR_WRITEMASK: ~A~%" (glGetBooleanv GL_COLOR_WRITEMASK))
  (format () "GL_DEPTH_WRITEMASK: ~A~%" (glGetBooleanv GL_DEPTH_WRITEMASK))
  (format () "GL_STENCIL_WRITEMASK: ~A~%" (glGetIntegerv GL_STENCIL_WRITEMASK))
  (format () "GL_COLOR_CLEAR_VALUE: ~A~%" (glGetFloatv GL_COLOR_CLEAR_VALUE))
  (format () "GL_INDEX_CLEAR_VALUE: ~A~%" (glGetIntegerv GL_INDEX_CLEAR_VALUE))
  (format () "GL_DEPTH_CLEAR_VALUE: ~A~%" (glGetFloatv GL_DEPTH_CLEAR_VALUE))
  (format () "GL_STENCIL_CLEAR_VALUE: ~A~%" (glGetIntegerv GL_STENCIL_CLEAR_VALUE))
  (format () "GL_ACCUM_CLEAR_VALUE: ~A~%" (glGetFloatv GL_ACCUM_CLEAR_VALUE))
  (format () "GL_UNPACK_SWAP_BYTES: ~A~%" (glGetBooleanv GL_UNPACK_SWAP_BYTES))
  (format () "GL_UNPACK_LSB_FIRST: ~A~%" (glGetBooleanv GL_UNPACK_LSB_FIRST))
  (format () "GL_UNPACK_ROW_LENGTH: ~A~%" (glGetIntegerv GL_UNPACK_ROW_LENGTH))
  (format () "GL_UNPACK_SKIP_ROWS: ~A~%" (glGetIntegerv GL_UNPACK_SKIP_ROWS))
  (format () "GL_UNPACK_SKIP_PIXELS: ~A~%" (glGetIntegerv GL_UNPACK_SKIP_PIXELS))
  (format () "GL_UNPACK_ALIGNMENT: ~A~%" (glGetIntegerv GL_UNPACK_ALIGNMENT))
  (format () "GL_PACK_SWAP_BYTES: ~A~%" (glGetBooleanv GL_PACK_SWAP_BYTES))
  (format () "GL_PACK_LSB_FIRST: ~A~%" (glGetBooleanv GL_PACK_LSB_FIRST))
  (format () "GL_PACK_ROW_LENGTH: ~A~%" (glGetIntegerv GL_PACK_ROW_LENGTH))
  (format () "GL_PACK_SKIP_ROWS: ~A~%" (glGetIntegerv GL_PACK_SKIP_ROWS))
  (format () "GL_PACK_SKIP_PIXELS: ~A~%" (glGetIntegerv GL_PACK_SKIP_PIXELS))
  (format () "GL_PACK_ALIGNMENT: ~A~%" (glGetIntegerv GL_PACK_ALIGNMENT))
  (format () "GL_MAP_COLOR: ~A~%" (glGetBooleanv GL_MAP_COLOR))
  (format () "GL_MAP_STENCIL: ~A~%" (glGetBooleanv GL_MAP_STENCIL))
  (format () "GL_INDEX_SHIFT: ~A~%" (glGetIntegerv GL_INDEX_SHIFT))
  (format () "GL_INDEX_OFFSET: ~A~%" (glGetIntegerv GL_INDEX_OFFSET))
  (format () "GL_RED_SCALE: ~A~%" (glGetFloatv GL_RED_SCALE))
  (format () "GL_GREEN_SCALE: ~A~%" (glGetFloatv GL_GREEN_SCALE))
  (format () "GL_BLUE_SCALE: ~A~%" (glGetFloatv GL_BLUE_SCALE))
  (format () "GL_ALPHA_SCALE: ~A~%" (glGetFloatv GL_ALPHA_SCALE))
  (format () "GL_DEPTH_SCALE: ~A~%" (glGetFloatv GL_DEPTH_SCALE))
  (format () "GL_RED_BIAS: ~A~%" (glGetFloatv GL_RED_BIAS))
  (format () "GL_GREEN_BIAS: ~A~%" (glGetFloatv GL_GREEN_BIAS))
  (format () "GL_BLUE_BIAS: ~A~%" (glGetFloatv GL_BLUE_BIAS))
  (format () "GL_ALPHA_BIAS: ~A~%" (glGetFloatv GL_ALPHA_BIAS))
  (format () "GL_DEPTH_BIAS: ~A~%" (glGetFloatv GL_DEPTH_BIAS))
  (format () "GL_ZOOM_X: ~A~%" (glGetFloatv GL_ZOOM_X))
  (format () "GL_ZOOM_Y: ~A~%" (glGetFloatv GL_ZOOM_Y))
  (format () "GL_READ_BUFFER: ~A~%" (glGetIntegerv GL_READ_BUFFER))
  (format () "GL_AUTO_NORMAL: ~A~%" (glGetBooleanv GL_AUTO_NORMAL))
  (format () "GL_PERSPECTIVE_CORRECTION_HINT: ~A~%" (glGetIntegerv GL_PERSPECTIVE_CORRECTION_HINT))
  (format () "GL_POINT_SMOOTH_HINT: ~A~%" (glGetIntegerv GL_POINT_SMOOTH_HINT))
  (format () "GL_LINE_SMOOTH_HINT: ~A~%" (glGetIntegerv GL_LINE_SMOOTH_HINT))
  (format () "GL_POLYGON_SMOOTH_HINT: ~A~%" (glGetIntegerv GL_POLYGON_SMOOTH_HINT))
  (format () "GL_FOG_HINT: ~A~%" (glGetIntegerv GL_FOG_HINT))
  (format () "GL_MAX_LIGHTS: ~A~%" (glGetIntegerv GL_MAX_LIGHTS))
  (format () "GL_MAX_CLIP_PLANES: ~A~%" (glGetIntegerv GL_MAX_CLIP_PLANES))
  (format () "GL_MAX_MODELVIEW_STACK_DEPTH: ~A~%" (glGetIntegerv GL_MAX_MODELVIEW_STACK_DEPTH))
  (format () "GL_MAX_PROJECTION_STACK_DEPTH: ~A~%" (glGetIntegerv GL_MAX_PROJECTION_STACK_DEPTH))
  (format () "GL_MAX_TEXTURE_STACK_DEPTH: ~A~%" (glGetIntegerv GL_MAX_TEXTURE_STACK_DEPTH))
  (format () "GL_SUBPIXEL_BITS: ~A~%" (glGetIntegerv GL_SUBPIXEL_BITS))
  (format () "GL_MAX_TEXTURE_SIZE: ~A~%" (glGetIntegerv GL_MAX_TEXTURE_SIZE))
  (format () "GL_MAX_PIXEL_MAP_TABLE: ~A~%" (glGetIntegerv GL_MAX_PIXEL_MAP_TABLE))
  (format () "GL_MAX_NAME_STACK_DEPTH: ~A~%" (glGetIntegerv GL_MAX_NAME_STACK_DEPTH))
  (format () "GL_MAX_LIST_NESTING: ~A~%" (glGetIntegerv GL_MAX_LIST_NESTING))
  (format () "GL_MAX_EVAL_ORDER: ~A~%" (glGetIntegerv GL_MAX_EVAL_ORDER))
  (format () "GL_MAX_VIEWPORT_DIMS: ~A~%" (glGetIntegerv GL_MAX_VIEWPORT_DIMS))
  (format () "GL_MAX_ATTRIB_STACK_DEPTH: ~A~%" (glGetIntegerv GL_MAX_ATTRIB_STACK_DEPTH))
  (format () "GL_MAX_CLIENT_ATTRIB_STACK_DEPTH: ~A~%" (glGetIntegerv GL_MAX_CLIENT_ATTRIB_STACK_DEPTH))
  (format () "GL_AUX_BUFFERS: ~A~%" (glGetIntegerv GL_AUX_BUFFERS))
  (format () "GL_RGBA_MODE: ~A~%" (glGetBooleanv GL_RGBA_MODE))
  (format () "GL_INDEX_MODE: ~A~%" (glGetBooleanv GL_INDEX_MODE))
  (format () "GL_DOUBLEBUFFER: ~A~%" (glGetBooleanv GL_DOUBLEBUFFER))
  (format () "GL_STEREO: ~A~%" (glGetBooleanv GL_STEREO))
  (format () "GL_LINE_WIDTH_RANGE: ~A~%" (glGetFloatv GL_LINE_WIDTH_RANGE))
  (format () "GL_LINE_WIDTH_GRANULARITY: ~A~%" (glGetFloatv GL_LINE_WIDTH_GRANULARITY))
  (format () "GL_RED_BITS: ~A~%" (glGetIntegerv GL_RED_BITS))
  (format () "GL_GREEN_BITS: ~A~%" (glGetIntegerv GL_GREEN_BITS))
  (format () "GL_BLUE_BITS: ~A~%" (glGetIntegerv GL_BLUE_BITS))
  (format () "GL_ALPHA_BITS: ~A~%" (glGetIntegerv GL_ALPHA_BITS))
  (format () "GL_INDEX_BITS: ~A~%" (glGetIntegerv GL_INDEX_BITS))
  (format () "GL_DEPTH_BITS: ~A~%" (glGetIntegerv GL_DEPTH_BITS))
  (format () "GL_STENCIL_BITS: ~A~%" (glGetIntegerv GL_STENCIL_BITS))
  (format () "GL_ACCUM_RED_BITS: ~A~%" (glGetIntegerv GL_ACCUM_RED_BITS))
  (format () "GL_ACCUM_GREEN_BITS: ~A~%" (glGetIntegerv GL_ACCUM_GREEN_BITS))
  (format () "GL_ACCUM_BLUE_BITS: ~A~%" (glGetIntegerv GL_ACCUM_BLUE_BITS))
  (format () "GL_ACCUM_ALPHA_BITS: ~A~%" (glGetIntegerv GL_ACCUM_ALPHA_BITS))
  (format () "GL_LIST_BASE: ~A~%" (glGetIntegerv GL_LIST_BASE))
  (format () "GL_LIST_INDEX: ~A~%" (glGetIntegerv GL_LIST_INDEX))
  (format () "GL_LIST_MODE: ~A~%" (glGetIntegerv GL_LIST_MODE))
  (format () "GL_ATTRIB_STACK_DEPTH: ~A~%" (glGetIntegerv GL_ATTRIB_STACK_DEPTH))
  (format () "GL_CLIENT_ATTRIB_STACK_DEPTH: ~A~%" (glGetIntegerv GL_CLIENT_ATTRIB_STACK_DEPTH))
  (format () "GL_NAME_STACK_DEPTH: ~A~%" (glGetIntegerv GL_NAME_STACK_DEPTH))
  (format () "GL_RENDER_MODE: ~A~%" (glGetIntegerv GL_RENDER_MODE))
  (format () "GL_SELECTION_BUFFER_POINTER: ~A~%" (glGetPointerv GL_SELECTION_BUFFER_POINTER))
  (format () "GL_SELECTION_BUFFER_SIZE: ~A~%" (glGetIntegerv GL_SELECTION_BUFFER_SIZE))
  (format () "GL_FEEDBACK_BUFFER_POINTER: ~A~%" (glGetPointerv GL_FEEDBACK_BUFFER_POINTER))
  (format () "GL_FEEDBACK_BUFFER_SIZE: ~A~%" (glGetIntegerv GL_FEEDBACK_BUFFER_SIZE))
  (format () "GL_FEEDBACK_BUFFER_TYPE: ~A~%" (glGetIntegerv GL_FEEDBACK_BUFFER_TYPE))
)


;;; -------- complexify --------

(require snd-snd-motif.scm)

(define complexify
  (let ((drawer #f))
    
    (define redraw-graph
      (let ((gl-list #f))
	(lambda ()
	  (let ((win ((*motif* 'XtWindow) drawer))
		(dpy ((*motif* 'XtDisplay) drawer)))
	    (glXMakeCurrent dpy win (snd-gl-context))
	    (if gl-list (glDeleteLists gl-list 1))
	    (set! gl-list (glGenLists 1))
	    (glEnable GL_DEPTH_TEST)
	    (glShadeModel GL_SMOOTH)
	    (glClearDepth 1.0)
	    (glClearColor 1.0 1.0 1.0 1.0)
	    (glClear (logior GL_COLOR_BUFFER_BIT GL_DEPTH_BUFFER_BIT))
	    (let ((rl (channel->float-vector (left-sample) 512))
		  (im (make-float-vector 512)))
	      (mus-fft rl im)
	      (let ((peak (/ (* 2 (max (float-vector-peak rl) (float-vector-peak im))))))
		(float-vector-scale! rl peak)
		(float-vector-scale! im peak))
	      ;; display each element in the complex plane rotated to stack along the x axis
	      (glNewList gl-list GL_COMPILE)
	      (glBegin GL_LINES)
	      (apply glColor3f (color->list *data-color*))
	      (do ((i 0 (+ 1 i)))
		  ((= i 256))
		(glVertex3f (/ i 256.0) 0.0 0.0)
		(glVertex3f (/ i 256.0) (rl i) (im i)))
	      (glEnd)
	      (glEndList))
	    (let ((vals ((*motif* 'XtVaGetValues) drawer (list (*motif* 'XmNwidth) 0 (*motif* 'XmNheight) 0))))
	      (glViewport 0 0 (list-ref vals 1) (list-ref vals 3)))
	    (glMatrixMode GL_PROJECTION)
	    (glLoadIdentity)
	    (glOrtho -0.2 1.0 -1.5 1.0 -1.0 1.0)
	    (glRotatef *spectro-x-angle* 1.0 0.0 0.0)
	    (glRotatef *spectro-y-angle* 0.0 1.0 0.0)
	    (glRotatef *spectro-z-angle* 0.0 0.0 1.0)
	    (glScalef *spectro-x-scale* *spectro-y-scale* *spectro-z-scale*)
	    (glCallList gl-list)
	    (glXSwapBuffers dpy win)
	    (glDrawBuffer GL_BACK)))))
    
    (define (add-main-pane name type args)
      ((*motif* 'XtCreateManagedWidget) name type (list-ref (main-widgets) 3) args))
    
    (lambda ()
      (unless drawer
	(let ((outer (with-let (sublet *motif*)
		       (add-main-pane "Waterfall" xmFormWidgetClass
				      (list XmNbackground *basic-color*
					    XmNpaneMinimum 320)))))
	  (set! drawer (with-let (sublet *motif* 'outer outer)
			 (XtCreateManagedWidget "draw" xmDrawingAreaWidgetClass outer
						(list XmNbackground       *graph-color*
						      XmNforeground       *data-color*
						      XmNleftAttachment   XmATTACH_FORM
						      XmNtopAttachment    XmATTACH_FORM
						      XmNbottomAttachment XmATTACH_FORM
						      XmNrightAttachment  XmATTACH_FORM)))))
	(set! *spectro-x-angle* 210.0)
	(set! *spectro-y-angle* 60.0)
	(set! *spectro-z-angle* 30.0)
	(set! *spectro-x-scale* 3.0)
	((*motif* 'XtAddCallback) drawer (*motif* 'XmNresizeCallback) (lambda (w context info) (redraw-graph)))
	((*motif* 'XtAddCallback) drawer (*motif* 'XmNexposeCallback) (lambda (w context info) (redraw-graph)))
	(hook-push after-graph-hook (lambda (hook) (redraw-graph)))
	(hook-push orientation-hook (lambda (hook) (redraw-graph)))
	(hook-push color-hook (lambda (hook) (redraw-graph))))))))

(define complexify (*gl* 'complexify))