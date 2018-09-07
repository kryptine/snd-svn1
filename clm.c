/* CLM (Music V) implementation */

#include "mus-config.h"

#if USE_SND
  #include "snd.h"
#endif

#include <stddef.h>
#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifndef _MSC_VER
  #include <unistd.h>
#else
  #include <io.h>
  #pragma warning(disable: 4244)
#endif

#include "_sndlib.h"
#include "clm.h"
#include "clm-strings.h"

#if HAVE_GSL
  #include <gsl/gsl_complex.h>
  #include <gsl/gsl_complex_math.h>
#endif

#if HAVE_FFTW3
  #include <fftw3.h>
#endif

#if HAVE_COMPLEX_TRIG
  #include <complex.h>
#endif

#if (!DISABLE_SINCOS) && defined(__GNUC__) && defined(__linux__)
  #define HAVE_SINCOS 1
  void sincos(double x, double *sin, double *cos);
#else
  #define HAVE_SINCOS 0
#endif

#ifndef TWO_PI
  #define TWO_PI (2.0 * M_PI)
#endif

#if (!USE_SND)
#define mus_clear_floats(Arr, Len)		\
  do {						\
    mus_long_t K;				\
    mus_float_t *dst;				\
    dst = Arr;					\
    for (K = Len; K > 0; K--)			\
      *dst++ = 0.0;				\
  } while (0)
#define mus_copy_floats(Dst, Src, Len)		\
  do {						\
    mus_long_t K;				\
    mus_float_t *dst, *src;			\
    dst = Dst;					\
    src = Src;					\
    for (K = Len; K > 0; K--)			\
      *dst++ = *src++;				\
    } while (0)
#define mus_add_floats(Dst, Src, Len)		\
  do {						\
    mus_long_t K;				\
    mus_float_t *dst, *src;			\
    dst = Dst;					\
    src = Src;					\
    for (K = Len; K > 0; K--)			\
      *dst++ += *src++;				\
    } while (0)
#endif

struct mus_any_class {
  int type;
  char *name;
  void (*release)(mus_any *ptr);
  char *(*describe)(mus_any *ptr);                            /* caller should free the string */
  bool (*equalp)(mus_any *gen1, mus_any *gen2);
  mus_float_t *(*data)(mus_any *ptr);
  mus_float_t *(*set_data)(mus_any *ptr, mus_float_t *new_data);
  mus_long_t (*length)(mus_any *ptr);
  mus_long_t (*set_length)(mus_any *ptr, mus_long_t new_length);
  mus_float_t (*frequency)(mus_any *ptr);
  mus_float_t (*set_frequency)(mus_any *ptr, mus_float_t new_freq);
  mus_float_t (*phase)(mus_any *ptr); 
  mus_float_t (*set_phase)(mus_any *ptr, mus_float_t new_phase);
  mus_float_t (*scaler)(mus_any *ptr);
  mus_float_t (*set_scaler)(mus_any *ptr, mus_float_t val);
  mus_float_t (*increment)(mus_any *ptr);
  mus_float_t (*set_increment)(mus_any *ptr, mus_float_t val);
  mus_float_t (*run)(mus_any *gen, mus_float_t arg1, mus_float_t arg2);
  mus_clm_extended_t extended_type;
  void *(*closure)(mus_any *gen);
  int (*channels)(mus_any *ptr);
  mus_float_t (*offset)(mus_any *ptr);
  mus_float_t (*set_offset)(mus_any *ptr, mus_float_t val);
  mus_float_t (*width)(mus_any *ptr);
  mus_float_t (*set_width)(mus_any *ptr, mus_float_t val);
  mus_float_t (*xcoeff)(mus_any *ptr, int index);
  mus_float_t (*set_xcoeff)(mus_any *ptr, int index, mus_float_t val);
  mus_long_t (*hop)(mus_any *ptr);
  mus_long_t (*set_hop)(mus_any *ptr, mus_long_t new_length);
  mus_long_t (*ramp)(mus_any *ptr);
  mus_long_t (*set_ramp)(mus_any *ptr, mus_long_t new_length);
  mus_float_t (*read_sample)(mus_any *ptr, mus_long_t samp, int chan);
  mus_float_t (*write_sample)(mus_any *ptr, mus_long_t samp, int chan, mus_float_t data);
  char *(*file_name)(mus_any *ptr);
  int (*end)(mus_any *ptr);
  mus_long_t (*location)(mus_any *ptr);
  mus_long_t (*set_location)(mus_any *ptr, mus_long_t loc);
  int (*channel)(mus_any *ptr);
  mus_float_t (*ycoeff)(mus_any *ptr, int index);
  mus_float_t (*set_ycoeff)(mus_any *ptr, int index, mus_float_t val);
  mus_float_t *(*xcoeffs)(mus_any *ptr);
  mus_float_t *(*ycoeffs)(mus_any *ptr);
  void (*reset)(mus_any *ptr);
  void *(*set_closure)(mus_any *gen, void *e);
  mus_any *(*copy)(mus_any *ptr);
};


enum {MUS_OSCIL, MUS_NCOS, MUS_DELAY, MUS_COMB, MUS_NOTCH, MUS_ALL_PASS,
      MUS_TABLE_LOOKUP, MUS_SQUARE_WAVE, MUS_SAWTOOTH_WAVE, MUS_TRIANGLE_WAVE, MUS_PULSE_TRAIN,
      MUS_RAND, MUS_RAND_INTERP, MUS_ASYMMETRIC_FM, MUS_ONE_ZERO, MUS_ONE_POLE, MUS_TWO_ZERO, MUS_TWO_POLE, MUS_FORMANT, 
      MUS_SRC, MUS_GRANULATE, MUS_WAVE_TRAIN, 
      MUS_FILTER, MUS_FIR_FILTER, MUS_IIR_FILTER, MUS_CONVOLVE, MUS_ENV, MUS_LOCSIG,
      MUS_READIN, MUS_FILE_TO_SAMPLE, MUS_FILE_TO_FRAMPLE,
      MUS_SAMPLE_TO_FILE, MUS_FRAMPLE_TO_FILE, MUS_PHASE_VOCODER,
      MUS_MOVING_AVERAGE, MUS_MOVING_MAX, MUS_MOVING_NORM, MUS_NSIN, MUS_SSB_AM, MUS_POLYSHAPE, MUS_FILTERED_COMB,
      MUS_MOVE_SOUND, MUS_NRXYSIN, MUS_NRXYCOS, MUS_POLYWAVE, MUS_FIRMANT, MUS_FORMANT_BANK,
      MUS_ONE_POLE_ALL_PASS, MUS_COMB_BANK, MUS_ALL_PASS_BANK, MUS_FILTERED_COMB_BANK, MUS_OSCIL_BANK,
      MUS_PULSED_ENV, MUS_RXYKSIN, MUS_RXYKCOS,
      MUS_INITIAL_GEN_TAG};

mus_any_class *mus_generator_class(mus_any *ptr) {return(ptr->core);}

void mus_generator_set_extended_type(mus_any_class *p, mus_clm_extended_t extended_type) {p->extended_type = extended_type;}
void mus_generator_set_length(mus_any_class *p, mus_long_t (*length)(mus_any *ptr)) {p->length = length;}
void mus_generator_set_scaler(mus_any_class *p, mus_float_t (*scaler)(mus_any *ptr)) {p->scaler = scaler;}
void mus_generator_set_channels(mus_any_class *p, int (*channels)(mus_any *ptr)) {p->channels = channels;}
void mus_generator_set_location(mus_any_class *p, mus_long_t (*location)(mus_any *ptr)) {p->location = location;}
void mus_generator_set_set_location(mus_any_class *p, mus_long_t (*set_location)(mus_any *ptr, mus_long_t loc)) {p->set_location = set_location;}
void mus_generator_set_read_sample(mus_any_class *p, mus_float_t (*read_sample)(mus_any *ptr, mus_long_t samp, int chan)) {p->read_sample = read_sample;}
void mus_generator_set_channel(mus_any_class *p, int (*channel)(mus_any *ptr)) {p->channel = channel;}
void mus_generator_set_file_name(mus_any_class *p, char *(*file_name)(mus_any *ptr)) {p->file_name = file_name;}

mus_any_class *mus_make_generator(int type, char *name, 
				  void (*release)(mus_any *ptr), 
				  char *(*describe)(mus_any *ptr), 
				  bool (*equalp)(mus_any *gen1, mus_any *gen2))
{
  mus_any_class *p;
  p = (mus_any_class *)calloc(1, sizeof(mus_any_class));
  p->type = type;
  p->name = name;
  p->release = release;
  p->describe = describe;
  p->equalp = equalp;
  return(p);
}


static int mus_generator_type = MUS_INITIAL_GEN_TAG;

int mus_make_generator_type(void) {return(mus_generator_type++);}


static const char *interp_name[] = {"step", "linear", "sinusoidal", "all-pass", "lagrange", "bezier", "hermite"};

const char *mus_interp_type_to_string(int type)
{
  if (mus_is_interp_type(type))
    return(interp_name[type]);
  return("unknown");
}


static mus_float_t sampling_rate = MUS_DEFAULT_SAMPLING_RATE;

static mus_float_t w_rate = (TWO_PI / MUS_DEFAULT_SAMPLING_RATE);


static mus_float_t float_equal_fudge_factor = 0.0000001;

mus_float_t mus_float_equal_fudge_factor(void) {return(float_equal_fudge_factor);}

mus_float_t mus_set_float_equal_fudge_factor(mus_float_t val) 
{
  mus_float_t prev; 
  prev = float_equal_fudge_factor; 
  float_equal_fudge_factor = val; 
  return(prev);
}


static int array_print_length = MUS_DEFAULT_ARRAY_PRINT_LENGTH;

int mus_array_print_length(void) {return(array_print_length);}

int mus_set_array_print_length(int val) 
{
  int prev; 
  prev = array_print_length; 
  if (val >= 0) array_print_length = val; 
  return(prev);
}


static mus_long_t clm_file_buffer_size = MUS_DEFAULT_FILE_BUFFER_SIZE;

mus_long_t mus_file_buffer_size(void) {return(clm_file_buffer_size);}

mus_long_t mus_set_file_buffer_size(mus_long_t size) 
{
  /* this is set in with-sound, among other places */
  mus_long_t prev; 
  prev = clm_file_buffer_size; 
  clm_file_buffer_size = size; 
  return(prev);
}


mus_float_t mus_radians_to_hz(mus_float_t rads) {return(rads / w_rate);}
mus_float_t mus_hz_to_radians(mus_float_t hz) {return(hz * w_rate);}


mus_float_t mus_degrees_to_radians(mus_float_t degree) {return(degree * TWO_PI / 360.0);}
mus_float_t mus_radians_to_degrees(mus_float_t rads) {return(rads * 360.0 / TWO_PI);}


mus_float_t mus_db_to_linear(mus_float_t x) {return(pow(10.0, x / 20.0));}
mus_float_t mus_linear_to_db(mus_float_t x) {if (x > 0.0) return(20.0 * log10(x)); return(-100.0);}


mus_float_t mus_odd_multiple(mus_float_t x, mus_float_t y)  {mus_long_t f; f = (mus_long_t)floor(x); return(y * ((f & 1) ? f : (f + 1)));}
mus_float_t mus_even_multiple(mus_float_t x, mus_float_t y) {mus_long_t f; f = (mus_long_t)floor(x); return(y * ((f & 1) ? (f + 1) : f));}
mus_float_t mus_odd_weight(mus_float_t x)  {mus_long_t f; f = (mus_long_t)floor(x); return(1.0 - fabs(x - ((f & 1) ? f : (f + 1))));}
mus_float_t mus_even_weight(mus_float_t x) {mus_long_t f; f = (mus_long_t)floor(x); return(1.0 - fabs(x - ((f & 1) ? (f + 1) : f)));}

mus_float_t mus_srate(void) {return(sampling_rate);}

mus_float_t mus_set_srate(mus_float_t val) 
{
  mus_float_t prev; 
  prev = sampling_rate; 
  if (val > 0.0)
    {
      sampling_rate = val; 
      w_rate = (TWO_PI / sampling_rate); 
    }
  return(prev);
}


mus_long_t mus_seconds_to_samples(mus_float_t secs) {return((mus_long_t)(secs * sampling_rate));}
mus_float_t mus_samples_to_seconds(mus_long_t samps) {return((mus_float_t)((mus_float_t)samps / (mus_float_t)sampling_rate));}


#define DESCRIBE_BUFFER_SIZE 2048
#define STR_SIZE 128

static char *float_array_to_string(mus_float_t *arr, int len, int loc)
{
  /* %g is needed here rather than %f -- otherwise the number strings can be any size */
  #define MAX_NUM_SIZE 64
  char *base, *str;
  int i, lim, size = 512;
  if (!arr) 
    {
      str = (char *)malloc(4 * sizeof(char));
      snprintf(str, 4, "nil");
      return(str);
    }
  lim = (array_print_length + 4) * MAX_NUM_SIZE; /* 4 for possible bounds below */
  if (lim > size) size = lim;
  if (loc < 0) loc = 0;

  base = (char *)calloc(size, sizeof(char));
  str = (char *)malloc(STR_SIZE * sizeof(char));

  if (len > 0)
    {
      int k;
      snprintf(base, size, "[");
      lim = len;
      if (lim > array_print_length) lim = array_print_length;
      k = loc;
      if (k >= len) k = 0;
      for (i = 0; i < lim - 1; i++)
	{
	  snprintf(str, STR_SIZE, "%.3g ", arr[k]);
	  strcat(base, str);
	  if ((int)(strlen(base) + MAX_NUM_SIZE) > size)
	    {
	      base = (char *)realloc(base, size * 2 * sizeof(char));
	      base[size] = 0;
	      size *= 2;
	    }
	  k++;
	  if (k >= len) k = 0;
	}
      snprintf(str, STR_SIZE, "%.3g%s", arr[k], (len > lim) ? "..." : "]");
      strcat(base, str);
    }
  else snprintf(base, size, "[]");
  if (len > lim)
    {
      /* print ranges */
      int min_loc = 0, max_loc = 0;
      mus_float_t min_val, max_val;
      min_val = arr[0];
      max_val = arr[0];
      for (i = 1; i < len; i++)
	{
	  if (arr[i] < min_val) {min_val = arr[i]; min_loc = i;}
	  if (arr[i] > max_val) {max_val = arr[i]; max_loc = i;}
	}
      snprintf(str, STR_SIZE, "(%d: %.3g, %d: %.3g)]", min_loc, min_val, max_loc, max_val);
      strcat(base, str);
    }
  free(str);
  return(base);
}


static char *clm_array_to_string(mus_any **gens, int num_gens, const char *name, const char *indent)
{
  char *descr = NULL;
  if ((gens) && (num_gens > 0))
    {
      int i, len = 0;
      char **descrs;
      descrs = (char **)calloc(num_gens, sizeof(char *));
      for (i = 0; i < num_gens; i++)
	{
	  if (gens[i])
	    {
	      char *str = NULL;
	      descrs[i] = mus_format("\n%s[%d]: %s", indent, i, str = mus_describe(gens[i]));
	      if (str) free(str);
	    }
	  else descrs[i] = mus_format("\n%s[%d]: nil", indent, i);
	  len += strlen(descrs[i]);
	}
      len += (64 + strlen(name));
      descr = (char *)malloc(len * sizeof(char));
      snprintf(descr, len, "%s[%d]:", name, num_gens);
      for (i = 0; i < num_gens; i++)
	{
	  strcat(descr, descrs[i]);
	  free(descrs[i]);
	}
      free(descrs);
    }
  else
    {
      descr = (char *)malloc(128 * sizeof(char));
      snprintf(descr, 128, "%s: nil", name);
    }
  return(descr);
}


static char *int_array_to_string(int *arr, int num_ints, const char *name)
{
  #define MAX_INT_SIZE 32
  char *descr = NULL;
  if ((arr) && (num_ints > 0))
    {
      int i, len;
      char *intstr;
      len = num_ints * MAX_INT_SIZE + 64;
      descr = (char *)calloc(len, sizeof(char));
      intstr = (char *)malloc(MAX_INT_SIZE * sizeof(char));
      snprintf(descr, len, "%s[%d]: (", name, num_ints);      
      for (i = 0; i < num_ints - 1; i++)
	{
	  snprintf(intstr, MAX_INT_SIZE, "%d ", arr[i]);
	  strcat(descr, intstr);
	}
      snprintf(intstr, MAX_INT_SIZE, "%d)", arr[num_ints - 1]);
      strcat(descr, intstr);
      free(intstr);
    }
  else
    {
      descr = (char *)malloc(128 * sizeof(char));
      snprintf(descr, 128, "%s: nil", name);
    }
  return(descr);
}



/* ---------------- generic functions ---------------- */

#define check_gen(Ptr, Name) ((Ptr) ? true : (!mus_error(MUS_NO_GEN, "null generator passed to %s", Name)))

int mus_type(mus_any *ptr)
{
  return(((check_gen(ptr, S_mus_type)) && (ptr->core)) ? ptr->core->type : -1);
}


const char *mus_name(mus_any *ptr) 
{
  return((!ptr) ? "null" : ptr->core->name);
}


void mus_free(mus_any *gen)
{
  if (gen)
    (*(gen->core->release))(gen);
}


char *mus_describe(mus_any *gen)
{
  if (!gen)
    return(mus_strdup((char *)"null"));
  if ((gen->core) && (gen->core->describe))
    return((*(gen->core->describe))(gen));
  else mus_error(MUS_NO_DESCRIBE, "can't describe %s", mus_name(gen));
  return(NULL);
}


bool mus_equalp(mus_any *p1, mus_any *p2)
{
  if ((p1) && (p2))
    {
      if ((p1->core)->equalp)
	return((*((p1->core)->equalp))(p1, p2));
      else return(p1 == p2);
    }
  return(true); /* (eq nil nil) */
}


void mus_reset(mus_any *gen)
{
  if ((check_gen(gen, S_mus_reset)) &&
      (gen->core->reset))
    (*(gen->core->reset))(gen);
  else mus_error(MUS_NO_RESET, "can't reset %s", mus_name(gen));
}


mus_any *mus_copy(mus_any *gen)
{
  if ((check_gen(gen, S_mus_copy)) &&
      (gen->core->copy))
    return((*(gen->core->copy))(gen));
  else mus_error(MUS_NO_COPY, "can't copy %s", mus_name(gen));
  return(NULL);
}


mus_float_t mus_frequency(mus_any *gen)
{
  if ((check_gen(gen, S_mus_frequency)) &&
      (gen->core->frequency))
    return((*(gen->core->frequency))(gen));
  return((mus_float_t)mus_error(MUS_NO_FREQUENCY, "can't get %s's frequency", mus_name(gen)));
}


mus_float_t mus_set_frequency(mus_any *gen, mus_float_t val)
{
  if ((check_gen(gen, S_set S_mus_frequency)) &&
      (gen->core->set_frequency))
    return((*(gen->core->set_frequency))(gen, val));
  return((mus_float_t)mus_error(MUS_NO_FREQUENCY, "can't set %s's frequency", mus_name(gen)));
}


mus_float_t mus_phase(mus_any *gen)
{
  if ((check_gen(gen, S_mus_phase)) &&
      (gen->core->phase))
    return((*(gen->core->phase))(gen));
  return((mus_float_t)mus_error(MUS_NO_PHASE, "can't get %s's phase", mus_name(gen)));
}


mus_float_t mus_set_phase(mus_any *gen, mus_float_t val)
{
  if ((check_gen(gen, S_set S_mus_phase)) &&
      (gen->core->set_phase))
    return((*(gen->core->set_phase))(gen, val));
  return((mus_float_t)mus_error(MUS_NO_PHASE, "can't set %s's phase", mus_name(gen)));
}



mus_float_t mus_scaler(mus_any *gen)
{
  if ((check_gen(gen, S_mus_scaler)) &&
      (gen->core->scaler))
    return((*(gen->core->scaler))(gen));
  return((mus_float_t)mus_error(MUS_NO_SCALER, "can't get %s's scaler", mus_name(gen)));
}


mus_float_t mus_set_scaler(mus_any *gen, mus_float_t val)
{
  if ((check_gen(gen, S_set S_mus_scaler)) &&
      (gen->core->set_scaler))
    return((*(gen->core->set_scaler))(gen, val));
  return((mus_float_t)mus_error(MUS_NO_SCALER, "can't set %s's scaler", mus_name(gen)));
}


mus_float_t mus_feedforward(mus_any *gen) /* shares "scaler" */
{
  if ((check_gen(gen, S_mus_feedforward)) &&
      (gen->core->scaler))
    return((*(gen->core->scaler))(gen));
  return((mus_float_t)mus_error(MUS_NO_FEEDFORWARD, "can't get %s's feedforward", mus_name(gen)));
}


mus_float_t mus_set_feedforward(mus_any *gen, mus_float_t val)
{
  if ((check_gen(gen, S_set S_mus_feedforward)) &&
      (gen->core->set_scaler))
    return((*(gen->core->set_scaler))(gen, val));
  return((mus_float_t)mus_error(MUS_NO_FEEDFORWARD, "can't set %s's feedforward", mus_name(gen)));
}


mus_float_t mus_offset(mus_any *gen)
{
  if ((check_gen(gen, S_mus_offset)) &&
      (gen->core->offset))
    return((*(gen->core->offset))(gen));
  return((mus_float_t)mus_error(MUS_NO_OFFSET, "can't get %s's offset", mus_name(gen)));
}


mus_float_t mus_set_offset(mus_any *gen, mus_float_t val)
{
  if ((check_gen(gen, S_set S_mus_offset)) &&
      (gen->core->set_offset))
    return((*(gen->core->set_offset))(gen, val));
  return((mus_float_t)mus_error(MUS_NO_OFFSET, "can't set %s's offset", mus_name(gen)));
}


mus_float_t mus_width(mus_any *gen)
{
  if ((check_gen(gen, S_mus_width)) &&
      (gen->core->width))
    return((*(gen->core->width))(gen));
  return((mus_float_t)mus_error(MUS_NO_WIDTH, "can't get %s's width", mus_name(gen)));
}


mus_float_t mus_set_width(mus_any *gen, mus_float_t val)
{
  if ((check_gen(gen, S_set S_mus_width)) &&
      (gen->core->set_width))
    return((*(gen->core->set_width))(gen, val));
  return((mus_float_t)mus_error(MUS_NO_WIDTH, "can't set %s's width", mus_name(gen)));
}


mus_float_t mus_increment(mus_any *gen)
{
  if ((check_gen(gen, S_mus_increment)) &&
      (gen->core->increment))
    return((*(gen->core->increment))(gen));
  return((mus_float_t)mus_error(MUS_NO_INCREMENT, "can't get %s's increment", mus_name(gen)));
}


mus_float_t mus_set_increment(mus_any *gen, mus_float_t val)
{
  if ((check_gen(gen, S_set S_mus_increment)) &&
      (gen->core->set_increment))
    return((*(gen->core->set_increment))(gen, val));
  return((mus_float_t)mus_error(MUS_NO_INCREMENT, "can't set %s's increment", mus_name(gen)));
}


mus_float_t mus_feedback(mus_any *gen) /* shares "increment" */
{
  if ((check_gen(gen, S_mus_feedback)) &&
      (gen->core->increment))
    return((*(gen->core->increment))(gen));
  return((mus_float_t)mus_error(MUS_NO_FEEDBACK, "can't get %s's feedback", mus_name(gen)));
}


mus_float_t mus_set_feedback(mus_any *gen, mus_float_t val)
{
  if ((check_gen(gen, S_set S_mus_feedback)) &&
      (gen->core->set_increment))
    return((*(gen->core->set_increment))(gen, val));
  return((mus_float_t)mus_error(MUS_NO_FEEDBACK, "can't set %s's feedback", mus_name(gen)));
}


void *mus_environ(mus_any *gen)
{
  if (check_gen(gen, "mus-environ"))
    return((*(gen->core->closure))(gen));
  return(NULL);
}


void *mus_set_environ(mus_any *gen, void *e)
{
  if (check_gen(gen, S_set "mus-environ")) 
    return((*(gen->core->set_closure))(gen, e));
  return(NULL);
}


mus_float_t mus_run(mus_any *gen, mus_float_t arg1, mus_float_t arg2)
{
  if ((check_gen(gen, "mus-run")) &&
      (gen->core->run))
    return((*(gen->core->run))(gen, arg1, arg2));
  return((mus_float_t)mus_error(MUS_NO_RUN, "can't run %s", mus_name(gen)));
}


mus_long_t mus_length(mus_any *gen)
{
  if ((check_gen(gen, S_mus_length)) &&
      (gen->core->length))
    return((*(gen->core->length))(gen));
  return(mus_error(MUS_NO_LENGTH, "can't get %s's length", mus_name(gen)));
}


mus_long_t mus_set_length(mus_any *gen, mus_long_t len)
{
  if ((check_gen(gen, S_set S_mus_length)) &&
      (gen->core->set_length))
    return((*(gen->core->set_length))(gen, len));
  return(mus_error(MUS_NO_LENGTH, "can't set %s's length", mus_name(gen)));
}


mus_long_t mus_order(mus_any *gen) /* shares "length", no set */
{
  if ((check_gen(gen, S_mus_order)) &&
      (gen->core->length))
    return((*(gen->core->length))(gen));
  return(mus_error(MUS_NO_ORDER, "can't get %s's order", mus_name(gen)));
}


int mus_channels(mus_any *gen)
{
  if ((check_gen(gen, S_mus_channels)) &&
      (gen->core->channels))
    return((*(gen->core->channels))(gen));
  return(mus_error(MUS_NO_CHANNELS, "can't get %s's channels", mus_name(gen)));
}


int mus_interp_type(mus_any *gen) /* shares "channels", no set */
{
  if ((check_gen(gen, S_mus_interp_type)) &&
      (gen->core->channels))
    return((*(gen->core->channels))(gen));
  return(mus_error(MUS_NO_INTERP_TYPE, "can't get %s's interp type", mus_name(gen)));
}


int mus_position(mus_any *gen) /* shares "channels", no set, only used in C (snd-env.c) */
{
  if ((check_gen(gen, "mus-position")) &&
      (gen->core->channels))
    return((*(gen->core->channels))(gen));
  return(mus_error(MUS_NO_POSITION, "can't get %s's position", mus_name(gen)));
}


int mus_channel(mus_any *gen)
{
  if ((check_gen(gen, S_mus_channel)) &&
      (gen->core->channel))
    return(((*gen->core->channel))(gen));
  return(mus_error(MUS_NO_CHANNEL, "can't get %s's channel", mus_name(gen)));
}


mus_long_t mus_hop(mus_any *gen)
{
  if ((check_gen(gen, S_mus_hop)) &&
      (gen->core->hop))
    return((*(gen->core->hop))(gen));
  return(mus_error(MUS_NO_HOP, "can't get %s's hop value", mus_name(gen)));
}


mus_long_t mus_set_hop(mus_any *gen, mus_long_t len)
{
  if ((check_gen(gen, S_set S_mus_hop)) &&
      (gen->core->set_hop))
    return((*(gen->core->set_hop))(gen, len));
  return(mus_error(MUS_NO_HOP, "can't set %s's hop value", mus_name(gen)));
}


mus_long_t mus_ramp(mus_any *gen)
{
  if ((check_gen(gen, S_mus_ramp)) &&
      (gen->core->ramp))
    return((*(gen->core->ramp))(gen));
  return(mus_error(MUS_NO_RAMP, "can't get %s's ramp value", mus_name(gen)));
}


mus_long_t mus_set_ramp(mus_any *gen, mus_long_t len)
{
  if ((check_gen(gen, S_set S_mus_ramp)) &&
      (gen->core->set_ramp))
    return((*(gen->core->set_ramp))(gen, len));
  return(mus_error(MUS_NO_RAMP, "can't set %s's ramp value", mus_name(gen)));
}


mus_float_t *mus_data(mus_any *gen)
{
  if ((check_gen(gen, S_mus_data)) &&
      (gen->core->data))
    return((*(gen->core->data))(gen));
  mus_error(MUS_NO_DATA, "can't get %s's data", mus_name(gen));
  return(NULL);
}


/* every case that implements the data or set data functions needs to include
 * a var-allocated flag, since all such memory has to be handled via vcts
 */

mus_float_t *mus_set_data(mus_any *gen, mus_float_t *new_data)
{
  if (check_gen(gen, S_set S_mus_data))
    {
      if (gen->core->set_data)
	{
	  (*(gen->core->set_data))(gen, new_data);
	  return(new_data);
	}
      else mus_error(MUS_NO_DATA, "can't set %s's data", mus_name(gen));
    }
  return(new_data);
}


mus_float_t *mus_xcoeffs(mus_any *gen)
{
  if ((check_gen(gen, S_mus_xcoeffs)) &&
      (gen->core->xcoeffs))
    return((*(gen->core->xcoeffs))(gen));
  mus_error(MUS_NO_XCOEFFS, "can't get %s's xcoeffs", mus_name(gen));
  return(NULL);
}


mus_float_t *mus_ycoeffs(mus_any *gen)
{
  if ((check_gen(gen, S_mus_ycoeffs)) &&
      (gen->core->ycoeffs))
    return((*(gen->core->ycoeffs))(gen));
  mus_error(MUS_NO_YCOEFFS, "can't get %s's ycoeffs", mus_name(gen));
  return(NULL);
}


mus_float_t mus_xcoeff(mus_any *gen, int index)
{
  if ((check_gen(gen, S_mus_xcoeff)) &&
      (gen->core->xcoeff))
    return((*(gen->core->xcoeff))(gen, index));
  return(mus_error(MUS_NO_XCOEFF, "can't get %s's xcoeff[%d] value", mus_name(gen), index));
}


mus_float_t mus_set_xcoeff(mus_any *gen, int index, mus_float_t val)
{
  if ((check_gen(gen, S_set S_mus_xcoeff)) &&
      (gen->core->set_xcoeff))
    return((*(gen->core->set_xcoeff))(gen, index, val));
  return(mus_error(MUS_NO_XCOEFF, "can't set %s's xcoeff[%d] value", mus_name(gen), index));
}


mus_float_t mus_ycoeff(mus_any *gen, int index)
{
  if ((check_gen(gen, S_mus_ycoeff)) &&
      (gen->core->ycoeff))
    return((*(gen->core->ycoeff))(gen, index));
  return(mus_error(MUS_NO_YCOEFF, "can't get %s's ycoeff[%d] value", mus_name(gen), index));
}


mus_float_t mus_set_ycoeff(mus_any *gen, int index, mus_float_t val)
{
  if ((check_gen(gen, S_set S_mus_ycoeff)) &&
      (gen->core->set_ycoeff))
    return((*(gen->core->set_ycoeff))(gen, index, val));
  return(mus_error(MUS_NO_YCOEFF, "can't set %s's ycoeff[%d] value", mus_name(gen), index));
}


mus_long_t mus_location(mus_any *gen)
{
  if ((check_gen(gen, S_mus_location)) &&
      (gen->core->location))
    return(((*gen->core->location))(gen));
  return((mus_long_t)mus_error(MUS_NO_LOCATION, "can't get %s's location", mus_name(gen)));
}

mus_long_t mus_set_location(mus_any *gen, mus_long_t loc)
{
  if ((check_gen(gen, S_set S_mus_location)) &&
      (gen->core->set_location))
    return((*(gen->core->set_location))(gen, loc));
  return((mus_long_t)mus_error(MUS_NO_LOCATION, "can't set %s's location", mus_name(gen)));
}

char *mus_file_name(mus_any *gen)
{
  if ((check_gen(gen, S_mus_file_name)) &&
      (gen->core->file_name))
    return((*(gen->core->file_name))(gen));
  else mus_error(MUS_NO_FILE_NAME, "can't get %s's file name", mus_name(gen));
  return(NULL);
}


bool mus_phase_exists(mus_any *gen)       {return(gen->core->phase);}
bool mus_frequency_exists(mus_any *gen)   {return(gen->core->frequency);}
bool mus_length_exists(mus_any *gen)      {return(gen->core->length);}
bool mus_order_exists(mus_any *gen)       {return(gen->core->length);}
bool mus_data_exists(mus_any *gen)        {return(gen->core->data);}
bool mus_name_exists(mus_any *gen)        {return(gen->core->name);}
bool mus_scaler_exists(mus_any *gen)      {return(gen->core->scaler);}
bool mus_offset_exists(mus_any *gen)      {return(gen->core->offset);}
bool mus_width_exists(mus_any *gen)       {return(gen->core->width);}
bool mus_file_name_exists(mus_any *gen)   {return(gen->core->file_name);}
bool mus_xcoeffs_exists(mus_any *gen)     {return(gen->core->xcoeffs);}
bool mus_ycoeffs_exists(mus_any *gen)     {return(gen->core->ycoeffs);}
bool mus_increment_exists(mus_any *gen)   {return(gen->core->increment);}
bool mus_location_exists(mus_any *gen)    {return(gen->core->location);}
bool mus_channel_exists(mus_any *gen)     {return(gen->core->channel);}
bool mus_channels_exists(mus_any *gen)    {return(gen->core->channels);}
bool mus_interp_type_exists(mus_any *gen) {return(gen->core->channels);}
bool mus_ramp_exists(mus_any *gen)        {return(gen->core->ramp);}
bool mus_hop_exists(mus_any *gen)         {return(gen->core->hop);}
bool mus_feedforward_exists(mus_any *gen) {return(gen->core->scaler);}
bool mus_feedback_exists(mus_any *gen)    {return(gen->core->increment);}


/* ---------------- AM etc ---------------- */

mus_float_t mus_ring_modulate(mus_float_t sig1, mus_float_t sig2) 
{
  return(sig1 * sig2);
}


mus_float_t mus_amplitude_modulate(mus_float_t carrier, mus_float_t sig1, mus_float_t sig2) 
{
  return(sig1 * (carrier + sig2));
}


mus_float_t mus_contrast_enhancement(mus_float_t sig, mus_float_t index) 
{
  return(sin((sig * M_PI_2) + (index * sin(sig * TWO_PI))));
}


bool mus_arrays_are_equal(mus_float_t *arr1, mus_float_t *arr2, mus_float_t fudge, mus_long_t len)
{
  mus_long_t i;
  if (fudge == 0.0)
    {
      for (i = 0; i < len; i++)
	if (arr1[i] != arr2[i])
	  return(false);
    }
  else
    {
      mus_long_t len4;
      len4 = len - 4;
      i = 0;
      while (i <= len4)
	{
	  if (fabs(arr1[i] - arr2[i]) > fudge)
	    return(false);
	  i++;
	  if (fabs(arr1[i] - arr2[i]) > fudge)
	    return(false);
	  i++;
	  if (fabs(arr1[i] - arr2[i]) > fudge)
	    return(false);
	  i++;
	  if (fabs(arr1[i] - arr2[i]) > fudge)
	    return(false);
	  i++;
	}
      for (; i < len; i++)
	if (fabs(arr1[i] - arr2[i]) > fudge)
	  return(false);
    }
  return(true);
}


static bool clm_arrays_are_equal(mus_float_t *arr1, mus_float_t *arr2, mus_long_t len)
{
  return(mus_arrays_are_equal(arr1, arr2, float_equal_fudge_factor, len));
}

mus_float_t mus_dot_product(mus_float_t *data1, mus_float_t *data2, mus_long_t size)
{
  mus_long_t i, size4;
  mus_float_t sum = 0.0;
  size4 = size - 4;
  i = 0;
  while (i <= size4)
    {
      sum += (data1[i] * data2[i]);
      i++;
      sum += (data1[i] * data2[i]);
      i++;
      sum += (data1[i] * data2[i]);
      i++;
      sum += (data1[i] * data2[i]);
      i++;
    }
  for (; i < size; i++) 
    sum += (data1[i] * data2[i]);
  return(sum);
}


#if HAVE_COMPLEX_TRIG
#if HAVE_FORTH 
  #include "xen.h" 
#endif 

complex double mus_edot_product(complex double freq, complex double *data, mus_long_t size)
{
  int i;
  complex double sum = 0.0;
  for (i = 0; i < size; i++) 
    sum += (cexp(i * freq) * data[i]);
  return(sum);
}
#endif


mus_float_t mus_polynomial(mus_float_t *coeffs, mus_float_t x, int ncoeffs)
{
  mus_float_t sum;
  int i;
  if (ncoeffs <= 0) return(0.0);
  if (ncoeffs == 1) return(coeffs[0]); /* just a constant term */
  sum = coeffs[ncoeffs - 1];
  /* unrolled is slower */
  for (i = ncoeffs - 2; i >= 0; i--) 
    sum = (sum * x) + coeffs[i];
  return((mus_float_t)sum);
}

void mus_rectangular_to_polar(mus_float_t *rl, mus_float_t *im, mus_long_t size) 
{
  mus_long_t i; 
  for (i = 0; i < size; i++)
    {
      mus_float_t temp; /* apparently floating underflows (denormals?) in sqrt are bringing us to a halt */
      temp = rl[i] * rl[i] + im[i] * im[i];
      if (temp < .00000001) 
	{
	  rl[i] = 0.0;
	  im[i] = 0.0;
	}
      else 
	{
	  im[i] = -atan2(im[i], rl[i]); /* "-" here so that clockwise is positive? is this backwards? */
	  rl[i] = sqrt(temp);
	}
    }
}


void mus_rectangular_to_magnitudes(mus_float_t *rl, mus_float_t *im, mus_long_t size) 
{
  mus_long_t i; 
  for (i = 0; i < size; i++)
    {
      mus_float_t temp; /* apparently floating underflows in sqrt are bringing us to a halt */
      temp = rl[i] * rl[i] + im[i] * im[i];
      if (temp < .00000001) 
	rl[i] = 0.0;
      else rl[i] = sqrt(temp);
    }
}


void mus_polar_to_rectangular(mus_float_t *rl, mus_float_t *im, mus_long_t size) 
{
  mus_long_t i; 
  for (i = 0; i < size; i++)
    {
#if HAVE_SINCOS
      double sx, cx;
      sincos(-im[i], &sx, &cx);
      im[i] = sx * rl[i];
      rl[i] *= cx;
#else
      mus_float_t temp;
      temp = rl[i] * sin(-im[i]); /* minus to match sense of rectangular->polar above */
      rl[i] *= cos(-im[i]);
      im[i] = temp;
#endif
    }
}


static mus_float_t *array_normalize(mus_float_t *table, mus_long_t table_size)
{
  mus_float_t amp = 0.0;
  mus_long_t i;

  for (i = 0; i < table_size; i++) 
    if (amp < (fabs(table[i]))) 
      amp = fabs(table[i]);

  if ((amp > 0.0) && 
      (amp != 1.0))
    for (i = 0; i < table_size; i++) 
      table[i] /= amp;

  return(table);
}


/* ---------------- interpolation ---------------- */

mus_float_t mus_array_interp(mus_float_t *wave, mus_float_t phase, mus_long_t size)
{
  /* changed 26-Sep-00 to be closer to mus.lisp */
  mus_long_t int_part;
  mus_float_t frac_part;
  if ((phase < 0.0) || (phase > size))
    {
      /* 28-Mar-01 changed to fmod; I was hoping to avoid this... */
      phase = fmod((mus_float_t)phase, (mus_float_t)size);
      if (phase < 0.0) phase += size;
    }

  int_part = (mus_long_t)phase; /* (mus_long_t)floor(phase); */
  frac_part = phase - int_part;
  if (int_part == size) int_part = 0;

  if (frac_part == 0.0) 
    return(wave[int_part]);
  else
    {
      mus_long_t inx;
      inx = int_part + 1;
      if (inx >= size) inx = 0;
      return(wave[int_part] + (frac_part * (wave[inx] - wave[int_part])));
    }
}


static mus_float_t mus_array_all_pass_interp(mus_float_t *wave, mus_float_t phase, mus_long_t size, mus_float_t yn1)
{
  /* this is intended for delay lines where you have a stream of values; in table-lookup it can be a mess */
  mus_long_t int_part, inx;
  mus_float_t frac_part;
  if ((phase < 0.0) || (phase > size))
    {
      phase = fmod((mus_float_t)phase, (mus_float_t)size);
      if (phase < 0.0) phase += size;
    }
  int_part = (mus_long_t)floor(phase);
  frac_part = phase - int_part;
  if (int_part == size) int_part = 0;
  inx = int_part + 1;
  if (inx >= size) inx -= size;
#if 1
  /* from DAFX */
  if (frac_part == 0.0)
    return(wave[inx] - yn1);
  return(wave[int_part] * frac_part + (1.0 - frac_part) * (wave[inx] - yn1));
#else
  /* from Perry Cook */
  if (frac_part == 0.0) 
    return(wave[int_part] + wave[inx] - yn1);
  else return(wave[int_part] + ((1.0 - frac_part) / (1 + frac_part)) * (wave[inx] - yn1));
#endif
}


static mus_float_t mus_array_lagrange_interp(mus_float_t *wave, mus_float_t x, mus_long_t size)
{
  /* Abramovitz and Stegun 25.2.11 -- everyone badmouths this poor formula */
  /* x assumed to be in the middle, between second and third vals */
  mus_long_t x0, xp1, xm1;
  mus_float_t p, pp;
  if ((x < 0.0) || (x > size))
    {
      x = fmod((mus_float_t)x, (mus_float_t)size);
      if (x < 0.0) x += size;
    }
  x0 = (mus_long_t)floor(x);
  p = x - x0;
  if (x0 >= size) x0 -= size;
  if (p == 0.0) return(wave[x0]);
  xp1 = x0 + 1;
  if (xp1 >= size) xp1 -= size;
  xm1 = x0 - 1;
  if (xm1 < 0) xm1 += size;
  pp = p * p;
  return((wave[xm1] * 0.5 * (pp - p)) + 
	 (wave[x0] * (1.0 - pp)) + 
	 (wave[xp1] * 0.5 * (p + pp)));
}


static mus_float_t mus_array_hermite_interp(mus_float_t *wave, mus_float_t x, mus_long_t size)
{
  /* from James McCartney */
  mus_long_t x0, x1, x2, x3;
  mus_float_t p, c0, c1, c2, c3, y0, y1, y2, y3;
  if ((x < 0.0) || (x > size))
    {
      x = fmod((mus_float_t)x, (mus_float_t)size);
      if (x < 0.0) x += size;
    }
  x1 = (mus_long_t)floor(x); 
  p = x - x1;
  if (x1 == size) x1 = 0;
  if (p == 0.0) return(wave[x1]);
  x2 = x1 + 1;
  if (x2 == size) x2 = 0;
  x3 = x2 + 1;
  if (x3 == size) x3 = 0;
  x0 = x1 - 1;
  if (x0 < 0) x0 = size - 1;
  y0 = wave[x0];
  y1 = wave[x1];
  y2 = wave[x2];
  y3 = wave[x3];
  c0 = y1;
  c1 = 0.5 * (y2 - y0);
  c3 = 1.5 * (y1 - y2) + 0.5 * (y3 - y0);
  c2 = y0 - y1 + c1 - c3;
  return(((c3 * p + c2) * p + c1) * p + c0);
}


static mus_float_t mus_array_bezier_interp(mus_float_t *wave, mus_float_t x, mus_long_t size)
{
  mus_long_t x0, x1, x2, x3;
  mus_float_t p, y0, y1, y2, y3, ay, by, cy;
  if ((x < 0.0) || (x > size))
    {
      x = fmod((mus_float_t)x, (mus_float_t)size);
      if (x < 0.0) x += size;
    }
  x1 = (mus_long_t)floor(x); 
  p = ((x - x1) + 1.0) / 3.0;
  if (x1 == size) x1 = 0;
  x2 = x1 + 1;
  if (x2 == size) x2 = 0;
  x3 = x2 + 1;
  if (x3 == size) x3 = 0;
  x0 = x1 - 1;
  if (x0 < 0) x0 = size - 1;
  y0 = wave[x0];
  y1 = wave[x1];
  y2 = wave[x2];
  y3 = wave[x3];
  cy = 3 * (y1 - y0);
  by = 3 * (y2 - y1) - cy;
  ay = y3 - y0 - cy - by;
  return(y0 + p * (cy + (p * (by + (p * ay)))));
}


bool mus_is_interp_type(int val)
{
  /* this is C++'s fault. */
  switch (val)
    {
    case MUS_INTERP_NONE:    
    case MUS_INTERP_LINEAR:
    case MUS_INTERP_SINUSOIDAL:
    case MUS_INTERP_LAGRANGE:
    case MUS_INTERP_HERMITE:
    case MUS_INTERP_ALL_PASS:
    case MUS_INTERP_BEZIER:
      return(true);
    }
  return(false);
}


mus_float_t mus_interpolate(mus_interp_t type, mus_float_t x, mus_float_t *table, mus_long_t table_size, mus_float_t y)
{
  switch (type)
    {
    case MUS_INTERP_NONE:
      {
	mus_long_t x0;
	x0 = ((mus_long_t)x) % table_size;
	if (x0 < 0) x0 += table_size;
	return(table[x0]);
      }

    case MUS_INTERP_LAGRANGE:
      return(mus_array_lagrange_interp(table, x, table_size));

    case MUS_INTERP_HERMITE:
      return(mus_array_hermite_interp(table, x, table_size));

    case MUS_INTERP_LINEAR:
      return(mus_array_interp(table, x, table_size));

    case MUS_INTERP_ALL_PASS:
      return(mus_array_all_pass_interp(table, x, table_size, y));

    case MUS_INTERP_BEZIER:
      return(mus_array_bezier_interp(table, x, table_size));

    default:
      mus_error(MUS_ARG_OUT_OF_RANGE, "unknown interpolation type: %d", type);
      break;
    }
  return(0.0);
}



/* ---------------- oscil ---------------- */

typedef struct {
  mus_any_class *core;
  mus_float_t phase, freq;
} osc;


mus_float_t mus_oscil(mus_any *ptr, mus_float_t fm, mus_float_t pm)
{
  osc *gen = (osc *)ptr;
  mus_float_t result;
  result = gen->phase + pm;
  gen->phase += (gen->freq + fm);
  return(sin(result));
}


mus_float_t mus_oscil_unmodulated(mus_any *ptr)
{
  osc *gen = (osc *)ptr;
  mus_float_t result;
  result = gen->phase;
  gen->phase += gen->freq;
  return(sin(result));
}


mus_float_t mus_oscil_fm(mus_any *ptr, mus_float_t fm)
{
  osc *gen = (osc *)ptr;
  mus_float_t result;
  result = gen->phase;
  gen->phase += (gen->freq + fm);
  return(sin(result));
}


mus_float_t mus_oscil_pm(mus_any *ptr, mus_float_t pm)
{
  mus_float_t result;
  osc *gen = (osc *)ptr;
  result = gen->phase + pm;
  gen->phase += gen->freq;
  return(sin(result));
}


bool mus_is_oscil(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_OSCIL));
}
/* this could be: bool mus_is_oscil(mus_any *ptr) {return((ptr) && (ptr->core == &OSCIL_CLASS));}
 */


static void free_oscil(mus_any *ptr) {free(ptr);}

static mus_float_t oscil_freq(mus_any *ptr) {return(mus_radians_to_hz(((osc *)ptr)->freq));}
static mus_float_t oscil_set_freq(mus_any *ptr, mus_float_t val) {((osc *)ptr)->freq = mus_hz_to_radians(val); return(val);}

static mus_float_t oscil_increment(mus_any *ptr) {return(((osc *)ptr)->freq);}
static mus_float_t oscil_set_increment(mus_any *ptr, mus_float_t val) {((osc *)ptr)->freq = val; return(val);}

static mus_float_t oscil_phase(mus_any *ptr) {return(fmod(((osc *)ptr)->phase, TWO_PI));}
static mus_float_t oscil_set_phase(mus_any *ptr, mus_float_t val) {((osc *)ptr)->phase = val; return(val);}

static mus_long_t oscil_cosines(mus_any *ptr) {return(1);}
static void oscil_reset(mus_any *ptr) {((osc *)ptr)->phase = 0.0;}

static mus_any *oscil_copy(mus_any *ptr)
{
  osc *g;
  g = (osc *)malloc(sizeof(osc));
  memcpy((void *)g, (void *)ptr, sizeof(osc));
  return((mus_any *)g);
}

static mus_float_t fallback_scaler(mus_any *ptr) {return(1.0);}


static bool oscil_equalp(mus_any *p1, mus_any *p2)
{
  return((p1 == p2) ||
	 ((mus_is_oscil((mus_any *)p1)) && 
	  (mus_is_oscil((mus_any *)p2)) &&
	  ((((osc *)p1)->freq) == (((osc *)p2)->freq)) &&
	  ((((osc *)p1)->phase) == (((osc *)p2)->phase))));
}


static char *describe_oscil(mus_any *ptr)
{
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s freq: %.3fHz, phase: %.3f", 
	       mus_name(ptr),
	       mus_frequency(ptr), 
	       mus_phase(ptr));
  return(describe_buffer);
}


static mus_any_class OSCIL_CLASS = {
  MUS_OSCIL,
  (char *)S_oscil,   /* the "(char *)" business is for g++'s benefit */
  &free_oscil,
  &describe_oscil,
  &oscil_equalp,
  0, 0, 
  &oscil_cosines, 0,
  &oscil_freq,
  &oscil_set_freq,
  &oscil_phase,
  &oscil_set_phase,
  &fallback_scaler, 0, 
  &oscil_increment,
  &oscil_set_increment,
  &mus_oscil,
  MUS_NOT_SPECIAL, 
  NULL,
  0, 
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &oscil_reset,
  0,
  &oscil_copy
};



mus_any *mus_make_oscil(mus_float_t freq, mus_float_t phase)
{
  osc *gen;
  gen = (osc *)malloc(sizeof(osc));
  gen->core = &OSCIL_CLASS;
  gen->freq = mus_hz_to_radians(freq);
  gen->phase = phase;
  return((mus_any *)gen);
}

/* decided against feedback-oscil (as in cellon) because it's not clear how to handle the index,
 *   and there are many options for the filtering -- since this part of the signal path
 *   is not hidden, there's no reason to bring it out explicitly (as in filtered-comb)
 */


/* ---------------- oscil-bank ---------------- */

typedef struct {
  mus_any_class *core;
  int size, orig_size;
  mus_float_t *amps, *phases, *freqs;   /* these can change, so sincos is not always safe */
  bool free_phases;
  mus_float_t (*ob_func)(mus_any *ptr);
#if HAVE_SINCOS
  double *sn1, *cs1, *sn2, *cs2, *phs;
  bool use_sc;
#endif
} ob;


static void free_oscil_bank(mus_any *ptr) 
{
  ob *g = (ob *)ptr;
#if HAVE_SINCOS
  if (g->sn1) {free(g->sn1); g->sn1 = NULL;}
  if (g->sn2) {free(g->sn2); g->sn2 = NULL;}
  if (g->cs1) {free(g->cs1); g->cs1 = NULL;}
  if (g->cs2) {free(g->cs2); g->cs2 = NULL;}
  if (g->phs) {free(g->phs); g->phs = NULL;}
#endif
  if ((g->phases) && (g->free_phases)) {free(g->phases); g->phases = NULL;}
  free(ptr);
}

static mus_any *ob_copy(mus_any *ptr)
{
  ob *g, *p;

  p = (ob *)ptr;
  g = (ob *)malloc(sizeof(ob));
  memcpy((void *)g, (void *)ptr, sizeof(ob));

  g->ob_func = p->ob_func;

#if HAVE_SINCOS
  if (g->sn1)
    {
      int bytes;
      bytes = g->size * sizeof(double);
      g->sn1 = (double *)malloc(bytes);
      memcpy((void *)(g->sn1), (void *)(p->sn1), bytes);
      g->sn2 = (double *)malloc(bytes);
      memcpy((void *)(g->sn2), (void *)(p->sn2), bytes);
      g->cs1 = (double *)malloc(bytes);
      memcpy((void *)(g->cs1), (void *)(p->cs1), bytes);
      g->cs2 = (double *)malloc(bytes);
      memcpy((void *)(g->cs2), (void *)(p->cs2), bytes);
      g->phs = (double *)malloc(bytes);
      memcpy((void *)(g->phs), (void *)(p->phs), bytes);
      g->use_sc = p->use_sc;
    }
#endif

  /* we have to make a new phases array -- otherwise the original and copy step on each other */
  g->free_phases = true;
  g->phases = (mus_float_t *)malloc(g->size * sizeof(mus_float_t));
  mus_copy_floats(g->phases, p->phases, g->size);
  return((mus_any *)g);
}

static mus_float_t *ob_data(mus_any *ptr) {return(((ob *)ptr)->phases);}

static mus_float_t run_oscil_bank(mus_any *ptr, mus_float_t input, mus_float_t unused) 
{
  return(mus_oscil_bank(ptr));
}


static mus_long_t oscil_bank_length(mus_any *ptr)
{
  return(((ob *)ptr)->size);
}


static mus_long_t oscil_bank_set_length(mus_any *ptr, mus_long_t len)
{
  ob *g = (ob *)ptr;
  if (len < 0) 
    g->size = 0; 
  else 
    {
      if (len > g->orig_size) 
	g->size = g->orig_size;
      else g->size = len;
    }
  return(len);
}


static void oscil_bank_reset(mus_any *ptr)
{
  ob *p = (ob *)ptr;
  p->size = p->orig_size;
  mus_clear_floats(p->phases, p->orig_size);
}


static bool oscil_bank_equalp(mus_any *p1, mus_any *p2)
{
  ob *o1 = (ob *)p1;
  ob *o2 = (ob *)p2;
  if (p1 == p2) return(true);
  return((o1->size == o2->size) &&
	 (o1->orig_size == o2->orig_size) &&
	 (o1->amps == o2->amps) &&
	 (o1->freqs == o2->freqs) &&
	 ((o1->phases == o2->phases) ||
	  (clm_arrays_are_equal(o1->phases, o2->phases, o2->size))));
}


static char *describe_oscil_bank(mus_any *ptr)
{
  ob *gen = (ob *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s size: %d",
	       mus_name(ptr),
	       gen->size);
  return(describe_buffer);
}

static mus_any_class OSCIL_BANK_CLASS = {
  MUS_OSCIL_BANK,
  (char *)S_oscil_bank,
  &free_oscil_bank,
  &describe_oscil_bank,
  &oscil_bank_equalp,
  &ob_data, 0,
  &oscil_bank_length, &oscil_bank_set_length,
  0, 0, 
  0, 0,
  0, 0,
  0, 0,
  &run_oscil_bank,
  MUS_NOT_SPECIAL, 
  NULL, 0,
  0, 0, 0, 0,
  0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &oscil_bank_reset,
  0, &ob_copy
};


bool mus_is_oscil_bank(mus_any *ptr)
{
  return((ptr) && 
	 (ptr->core->type == MUS_OSCIL_BANK));
}


static mus_float_t oscil_bank(mus_any *ptr)
{
  ob *p = (ob *)ptr;
  int i;
  mus_float_t sum = 0.0;
  if (!p->amps)
    {
      for (i = 0; i < p->size; i++)
	{
	  sum += sin(p->phases[i]);
	  p->phases[i] += p->freqs[i];
	}
    }
  else
    {
      for (i = 0; i < p->size; i++)
	{
	  sum += (p->amps[i] * sin(p->phases[i]));
	  p->phases[i] += p->freqs[i];
	}
    }
  return(sum);
}


#if HAVE_SINCOS
static mus_float_t stable_oscil_bank(mus_any *ptr)
{
  ob *p = (ob *)ptr;
  int i;
  mus_float_t sum = 0.0;
  if (p->use_sc)
    {
      for (i = 0; i < p->size; i++)
	sum += (p->sn1[i] * p->cs2[i] + p->cs1[i] * p->sn2[i]);
      p->use_sc = false;
    }
  else
    {
      double s, c;
      if (!p->amps)
	{
	  for (i = 0; i < p->size; i++)
	    {
	      sincos(p->phases[i], &s, &c);
	      p->sn2[i] = s;
	      p->cs2[i] = c;
	      sum += s;
	      p->phases[i] += p->phs[i];
	    }
	}
      else
	{
	  for (i = 0; i < p->size; i++)
	    {
	      sincos(p->phases[i], &s, &c);
	      p->sn2[i] = s;
	      p->cs2[i] = c;
	      sum += p->amps[i] * s;
	      p->phases[i] += p->phs[i];
	    }
	}
      p->use_sc = true;
    }
  return(sum);
}
#endif


mus_float_t mus_oscil_bank(mus_any *ptr)
{
  ob *p = (ob *)ptr;
  return(p->ob_func(ptr));
}


mus_any *mus_make_oscil_bank(int size, mus_float_t *freqs, mus_float_t *phases, mus_float_t *amps, bool stable)
{
  ob *gen;

  gen = (ob *)malloc(sizeof(ob));
  gen->core = &OSCIL_BANK_CLASS;
  gen->orig_size = size;
  gen->size = size;
  gen->amps = amps;
  gen->freqs = freqs;
  gen->phases = phases;
  gen->free_phases = false;
  gen->ob_func = oscil_bank;

#if HAVE_SINCOS
  if (stable)
    {
      int i;
      double s, c;

      gen->ob_func = stable_oscil_bank;
      gen->use_sc = false;
      gen->sn1 = (double *)malloc(size * sizeof(double));
      gen->sn2 = (double *)malloc(size * sizeof(double));
      gen->cs1 = (double *)malloc(size * sizeof(double));
      gen->cs2 = (double *)malloc(size * sizeof(double));
      gen->phs = (double *)malloc(size * sizeof(double));

      for (i = 0; i < size; i++)
	{
	  sincos(freqs[i], &s, &c);
	  if (amps)
	    {
	      s *= amps[i];
	      c *= amps[i];
	    }
	  gen->sn1[i] = s;
	  gen->cs1[i] = c;
	  gen->phs[i] = freqs[i] * 2.0;
	}
    }
  else
    {
      gen->sn1 = NULL;
      gen->sn2 = NULL;
      gen->cs1 = NULL;
      gen->cs2 = NULL;
      gen->phs = NULL;
    }
#endif

  return((mus_any *)gen);
}



/* ---------------- ncos ---------------- */

typedef struct {
  mus_any_class *core;
  int n;
  mus_float_t scaler, cos5, phase, freq;
} cosp;

#define DIVISOR_NEAR_ZERO(Den) (fabs(Den) < 1.0e-14)

mus_float_t mus_ncos(mus_any *ptr, mus_float_t fm)
{
  /* changed 25-Apr-04: use less stupid formula */
  /*   (/ (- (/ (sin (* (+ n 0.5) angle)) (* 2 (sin (* 0.5 angle)))) 0.5) n) */
  mus_float_t val, den;
  cosp *gen = (cosp *)ptr;
  den = sin(gen->phase * 0.5);
  if (DIVISOR_NEAR_ZERO(den))    /* see note -- this was den == 0.0 1-Aug-07 */
                                 /* perhaps use DBL_EPSILON (1.0e-9 I think) */
    val = 1.0;
  else 
    {
      val = (gen->scaler * (((sin(gen->phase * gen->cos5)) / (2.0 * den)) - 0.5));
      if (val > 1.0) val = 1.0; 
      /* I think this can't happen now that we check den above, but just in case... */
      /*   this check is actually incomplete, since we can be much below the correct value also, but the den check should fix those cases too */
    }
  gen->phase += (gen->freq + fm);
  return((mus_float_t)val);
}


/* I think we could add ncos_pm via:
 *
 *   mus_float_t mus_ncos_pm(mus_any *ptr, mus_float_t fm, mus_float_t pm)
 *    {
 *      cosp *gen = (cosp *)ptr;
 *      mus_float_t result;
 *      gen->phase += pm;
 *      result = mus_ncos(ptr, fm);
 *      gen->phase -= pm;
 *      return(result);
 *    }
 *
 * and the same trick could add pm to anything:
 *
 *   mus_float_t mus_run_with_pm(mus_any *ptr, mus_float_t fm, mus_float_t pm)
 *    {
 *      mus_float_t result;
 *      mus_set_phase(ptr, mus_phase(ptr) + pm);
 *      result = mus_run(ptr, fm, 0.0);
 *      mus_set_phase (ptr, mus_phase(ptr) - pm);
 *      return(result);
 *    }
 *
 * fm could also be handled here so the 4 cases become gen, mus_run_with_fm|pm|fm_and_pm(gen)
 * but... this could just as well happen at the extension language level, except that run doesn't expand macros?
 * The problem with differentiating the pm and using the fm arg is that we'd need a closure.
 */

#if 0
/* if the current phase is close to 0.0, there were numerical troubles here:
    :(/ (cos (* 1.5 pi 1.0000000000000007)) (cos (* 0.5 pi 1.0000000000000007)))
    -3.21167411694788
    :(/ (cos (* 1.5 pi 1.0000000000000004)) (cos (* 0.5 pi 1.0000000000000004)))
    -2.63292557243357
    :(/ (cos (* 1.5 pi 1.0000000000000002)) (cos (* 0.5 pi 1.0000000000000002)))
    -1.84007079646018
    :(/ (cos (* 1.5 pi 1.0000000000000001)) (cos (* 0.5 pi 1.0000000000000001)))
    -3.0
    :(/ (cos (* 1.5 pi 1.0000000000000008)) (cos (* 0.5 pi 1.0000000000000008)))
    -3.34939116712516
    ;; 16 bits in is probably too much for mus_float_ts
    ;; these numbers can be hit in normal cases:

 (define (ncos-with-inversions n x)
   ;; Andrews Askey Roy 261 
   (let* ((num (cos (* x (+ 0.5 n))))
          (den (cos (* x 0.5)))
          (val (/ num den)))  ; Chebyshev polynomial of the third kind! (4th uses sin = our current formula)
     (/ (- (if (even? n) val (- val))
           0.5)
        (+ 1 (* n 2)))))

 (with-sound (:scaled-to 1.0)
   (do ((i 0 (+ i 1))
         (x 0.0 (+ x .01)))
       ((= i 200)) ; glitch at 100 (= 1)
     (outa i (ncos-with-inversions 1 (* pi x)) *output*)))
*/
#endif


bool mus_is_ncos(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_NCOS));
}


static void free_ncos(mus_any *ptr) {free(ptr);}
static void ncos_reset(mus_any *ptr) {((cosp *)ptr)->phase = 0.0;}

static mus_float_t ncos_freq(mus_any *ptr) {return(mus_radians_to_hz(((cosp *)ptr)->freq));}
static mus_float_t ncos_set_freq(mus_any *ptr, mus_float_t val) {((cosp *)ptr)->freq = mus_hz_to_radians(val); return(val);}

static mus_float_t ncos_increment(mus_any *ptr) {return(((cosp *)ptr)->freq);}
static mus_float_t ncos_set_increment(mus_any *ptr, mus_float_t val) {((cosp *)ptr)->freq = val; return(val);}

static mus_float_t ncos_phase(mus_any *ptr) {return(fmod(((cosp *)ptr)->phase, TWO_PI));}
static mus_float_t ncos_set_phase(mus_any *ptr, mus_float_t val) {((cosp *)ptr)->phase = val; return(val);}

static mus_float_t ncos_scaler(mus_any *ptr) {return(((cosp *)ptr)->scaler);}
static mus_float_t ncos_set_scaler(mus_any *ptr, mus_float_t val) {((cosp *)ptr)->scaler = val; return(val);}

static mus_long_t ncos_n(mus_any *ptr) {return(((cosp *)ptr)->n);}

static mus_any *cosp_copy(mus_any *ptr)
{
  cosp *g;
  g = (cosp *)malloc(sizeof(cosp));
  memcpy((void *)g, (void *)ptr, sizeof(cosp));
  return((mus_any *)g);
}

static mus_long_t ncos_set_n(mus_any *ptr, mus_long_t val) 
{
  cosp *gen = (cosp *)ptr;
  if (val > 0)
    {
      gen->n = (int)val;
      gen->cos5 = val + 0.5;
      gen->scaler = 1.0 / (mus_float_t)val; 
    }
  return(val);
}

static mus_float_t run_ncos(mus_any *ptr, mus_float_t fm, mus_float_t unused) {return(mus_ncos(ptr, fm));}


static bool ncos_equalp(mus_any *p1, mus_any *p2)
{
  return((p1 == p2) ||
	 ((mus_is_ncos((mus_any *)p1)) && (mus_is_ncos((mus_any *)p2)) &&
	  ((((cosp *)p1)->freq) == (((cosp *)p2)->freq)) &&
	  ((((cosp *)p1)->phase) == (((cosp *)p2)->phase)) &&
	  ((((cosp *)p1)->n) == (((cosp *)p2)->n)) &&
	  ((((cosp *)p1)->scaler) == (((cosp *)p2)->scaler))));
}


static char *describe_ncos(mus_any *ptr)
{
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s freq: %.3fHz, phase: %.3f, n: %d",
	       mus_name(ptr),
	       mus_frequency(ptr),
	       mus_phase(ptr),
	       (int)mus_order(ptr));
  return(describe_buffer);
}


static mus_any_class NCOS_CLASS = {
  MUS_NCOS,
  (char *)S_ncos,
  &free_ncos,
  &describe_ncos,
  &ncos_equalp,
  0, 0, /* data */
  &ncos_n,
  &ncos_set_n,
  &ncos_freq,
  &ncos_set_freq,
  &ncos_phase,
  &ncos_set_phase,
  &ncos_scaler,
  &ncos_set_scaler,
  &ncos_increment,
  &ncos_set_increment,
  &run_ncos,
  MUS_NOT_SPECIAL, 
  NULL,
  0,
  0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &ncos_reset,
  0,
  &cosp_copy
};


mus_any *mus_make_ncos(mus_float_t freq, int n)
{
  cosp *gen;
  gen = (cosp *)malloc(sizeof(cosp));
  gen->core = &NCOS_CLASS;
  if (n == 0) n = 1;
  gen->scaler = 1.0 / (mus_float_t)n;
  gen->n = n;
  gen->cos5 = n + 0.5;
  gen->freq = mus_hz_to_radians(freq);
  gen->phase = 0.0;
  return((mus_any *)gen);
}


/* ---------------- nsin ---------------- */

static bool nsin_equalp(mus_any *p1, mus_any *p2)
{
  return((p1 == p2) ||
	 ((mus_is_nsin((mus_any *)p1)) && (mus_is_nsin((mus_any *)p2)) &&
	  ((((cosp *)p1)->freq) == (((cosp *)p2)->freq)) &&
	  ((((cosp *)p1)->phase) == (((cosp *)p2)->phase)) &&
	  ((((cosp *)p1)->n) == (((cosp *)p2)->n)) &&
	  ((((cosp *)p1)->scaler) == (((cosp *)p2)->scaler))));
}


static char *describe_nsin(mus_any *ptr)
{
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s freq: %.3fHz, phase: %.3f, n: %d",
	       mus_name(ptr),
	       mus_frequency(ptr),
	       mus_phase(ptr),
	       (int)mus_order(ptr));
  return(describe_buffer);
}


bool mus_is_nsin(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_NSIN));
}


#if 0
/* its simplest to get the maxes by running an example and recording the maxamp, but it also
 *   works for small "n" to use the derivative of the sum-of-sines as a Chebyshev polynomial in cos x,
 *   find its roots, and plug acos(root) into the original, recording the max:
 *
  (define (smax coeffs)
    (let* ((n (vct-length coeffs))
	   (dcos (make-vct n 1.0)))
      (do ((i 0 (+ i 1)))
  	  ((= i n))
        (vct-set! dcos i (* (+ i 1) (vct-ref coeffs i))))
      (let ((partials ()))
        (do ((i 0 (+ i 1)))
	    ((= i n))
	  (set! partials (append (list (vct-ref dcos i) (+ i 1)) partials)))
        (let ((Tn (partials->polynomial (reverse partials))))
	  (let ((roots (poly-roots Tn)))
	    (let ((mx (* -2 n)))
	      (for-each
	       (lambda (root)
	         (let ((acr (acos root))
		       (sum 0.0))
		   (do ((i 0 (+ i 1)))
		       ((= i n))
		     (set! sum (+ sum (* (vct-ref coeffs i) (sin (* (+ i 1) acr))))))
		   (if (> (abs sum) mx)
		       (set! mx (abs sum)))))
	       roots)
	      mx))))))
  
     (smax (make-vct n 1.0))

  * but that's too much effort for an initialization function.
  * much faster is this search (it usually hits an answer in 2 or 3 tries):
  *
  (define (find-nsin-max n)
    
    (define (ns x n) 
      (let* ((a2 (/ x 2))
	     (den (sin a2)))
        (if (= den 0.0)
	    0.0
	    (/ (* (sin (* n a2)) (sin (* (+ n 1) a2))) den))))

    (define (find-mid-max n lo hi)
      (let ((mid (/ (+ lo hi) 2)))
        (let ((ylo (ns lo n))
	      (yhi (ns hi n)))
  	  (if (< (abs (- ylo yhi)) 1e-100)
	      (list (ns mid n)
		    (rationalize (/ mid pi) 0.0))
	      (if (> ylo yhi)
		  (find-mid-max n lo mid)
		  (find-mid-max n mid hi))))))

  (find-mid-max n 0.0 (/ pi (+ n .5))))
  *
  * the 'mid' point has a surprisingly simple relation to pi:
  *
  * (find-max 100000000000000)
  * 7.24518620297426541161857919764185053934850053037407235e13
  *
  * (find-max 1000000000000000000000000)
  * 7.24518620297422918568756794921595308358209723004380140e23   1/1333333333333333333333334 = .75e-24 -> (3*pi)/(4*n)
  *
  * (find-max 10000000000000000000000000000000000)
  * 7.24518620297422918568756432662285195872681453497436666955413707681801083640192066844820049586929551886747925783e33
  *
  * which is approximately (/ (* 8 (expt (sin (* pi 3/8)) 2)) (* 3 pi)):
  * 7.245186202974229185687564326622851596467504
  *
  * (to get that expression, plug in 3pi/4n, treat (n+1)/n as essentially 1 as n gets very large,
  *    treat (sin x) as about x when x is very small, and simplify)
  * so if n>10, we could use (ns (/ (* 3 pi) (* 4 n)) n) without major error
  * It's possible to differentiate the nsin formula:
  *
  * -(cos(x/2)sin(nx/2)sin((n+1)x/2))/(2sin^2(x/2)) + ncos(nx/2)sin((n+1)x/2)/(2sin(x/2)) + (n+1)sin(nx/2)cos((n+1)x/2)/(2sin(x/2))
  *
  * and find the first 0 when n is very large -- it is very close to 3pi/(4*n)
  */
#endif


static mus_float_t nsin_ns(mus_float_t x, int n)
{
  mus_float_t a2, den;
  a2 = x / 2;
  den = sin(a2);
  if (den == 0.0)
    return(0.0);
  return(sin(n * a2) * sin((n + 1) * a2) / den);
}


static mus_float_t find_nsin_scaler(int n, mus_float_t lo, mus_float_t hi)
{
  mus_float_t mid, ylo, yhi;
  mid = (lo + hi) / 2;
  ylo = nsin_ns(lo, n);
  yhi = nsin_ns(hi, n);
  if (fabs(ylo - yhi) < 1e-12)
    return(nsin_ns(mid, n));
  if (ylo > yhi)
    return(find_nsin_scaler(n, lo, mid));
  return(find_nsin_scaler(n, mid, hi));
}


static mus_float_t nsin_scaler(int n)
{
  return(1.0 / find_nsin_scaler(n, 0.0, M_PI / (n + 0.5)));
}


static mus_long_t nsin_set_n(mus_any *ptr, mus_long_t val) 
{
  cosp *gen = (cosp *)ptr;
  gen->n = (int)val;
  gen->cos5 = val + 1.0;
  gen->scaler = nsin_scaler((int)val);
  return(val);
}


mus_float_t mus_nsin(mus_any *ptr, mus_float_t fm)
{
  /* (let* ((a2 (* angle 0.5))
	    (den (sin a2)))
       (if (= den 0.0)
	   0.0
	   (/ (* (sin (* n a2)) (sin (* (+ n 1) a2))) den)))
  */
#if HAVE_SINCOS
  double val, a2, ns, nc, s, c;
  cosp *gen = (cosp *)ptr;
  a2 = gen->phase * 0.5;
  sincos(a2, &s, &c);
  if (DIVISOR_NEAR_ZERO(s)) /* see note under ncos */
    val = 0.0;
  else 
    {
      sincos(gen->n * a2, &ns, &nc);
      val = gen->scaler * ns * (ns * c + nc * s) / s;
    }
#else
  mus_float_t val, den, a2;
  cosp *gen = (cosp *)ptr;
  a2 = gen->phase * 0.5;
  den = sin(a2);
  if (DIVISOR_NEAR_ZERO(den)) /* see note under ncos */
    val = 0.0;
  else val = gen->scaler * sin(gen->n * a2) * sin(a2 * gen->cos5) / den;
#endif
  gen->phase += (gen->freq + fm);
  return((mus_float_t)val);
}


static mus_float_t run_nsin(mus_any *ptr, mus_float_t fm, mus_float_t unused) {return(mus_nsin(ptr, fm));}

static mus_any_class NSIN_CLASS = {
  MUS_NSIN,
  (char *)S_nsin,
  &free_ncos,
  &describe_nsin,
  &nsin_equalp,
  0, 0, /* data */
  &ncos_n,
  &nsin_set_n,
  &ncos_freq,
  &ncos_set_freq,
  &ncos_phase,
  &ncos_set_phase,
  &ncos_scaler,
  &ncos_set_scaler,
  &ncos_increment,
  &ncos_set_increment,
  &run_nsin,
  MUS_NOT_SPECIAL, 
  NULL,
  0,
  0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &ncos_reset,
  0,
  &cosp_copy
};


mus_any *mus_make_nsin(mus_float_t freq, int n)
{
  cosp *gen;
  gen = (cosp *)mus_make_ncos(freq, n);
  gen->core = &NSIN_CLASS;
  gen->scaler = nsin_scaler(n);
  gen->cos5 = gen->n + 1.0;
  return((mus_any *)gen);
}


/* ---------------- asymmetric-fm ---------------- */

/* changed from sin(sin) to cos(sin) and added amplitude normalization 6-Sep-07 */

typedef struct {
  mus_any_class *core;
  mus_float_t r;
  mus_float_t freq, phase;
  mus_float_t ratio;
  mus_float_t cosr, sinr;
  mus_float_t one;
} asyfm;


static void free_asymmetric_fm(mus_any *ptr) {free(ptr);}
static void asyfm_reset(mus_any *ptr) {((asyfm *)ptr)->phase = 0.0;}

static mus_any *asyfm_copy(mus_any *ptr)
{
  asyfm *g;
  g = (asyfm *)malloc(sizeof(asyfm));
  memcpy((void *)g, (void *)ptr, sizeof(asyfm));
  return((mus_any *)g);
}

static mus_float_t asyfm_freq(mus_any *ptr) {return(mus_radians_to_hz(((asyfm *)ptr)->freq));}
static mus_float_t asyfm_set_freq(mus_any *ptr, mus_float_t val) {((asyfm *)ptr)->freq = mus_hz_to_radians(val); return(val);}

static mus_float_t asyfm_increment(mus_any *ptr) {return(((asyfm *)ptr)->freq);}
static mus_float_t asyfm_set_increment(mus_any *ptr, mus_float_t val) {((asyfm *)ptr)->freq = val; return(val);}

static mus_float_t asyfm_phase(mus_any *ptr) {return(fmod(((asyfm *)ptr)->phase, TWO_PI));}
static mus_float_t asyfm_set_phase(mus_any *ptr, mus_float_t val) {((asyfm *)ptr)->phase = val; return(val);}

static mus_float_t asyfm_ratio(mus_any *ptr) {return(((asyfm *)ptr)->ratio);}

static mus_float_t asyfm_r(mus_any *ptr) {return(((asyfm *)ptr)->r);}

static mus_float_t asyfm_set_r(mus_any *ptr, mus_float_t val) 
{
  asyfm *gen = (asyfm *)ptr;
  if (val != 0.0)
    {
      gen->r = val; 
      gen->cosr = 0.5 * (val - (1.0 / val));
      gen->sinr = 0.5 * (val + (1.0 / val));
      if ((val > 1.0) ||
	  ((val < 0.0) && (val > -1.0)))
	gen->one = -1.0; 
      else gen->one = 1.0;
    }
  return(val);
}


static bool asyfm_equalp(mus_any *p1, mus_any *p2)
{
  return((p1 == p2) ||
	 (((p1->core)->type == (p2->core)->type) &&
	  ((((asyfm *)p1)->freq) == (((asyfm *)p2)->freq)) && 
	  ((((asyfm *)p1)->phase) == (((asyfm *)p2)->phase)) &&
	  ((((asyfm *)p1)->ratio) == (((asyfm *)p2)->ratio)) &&
	  ((((asyfm *)p1)->r) == (((asyfm *)p2)->r))));
}


static char *describe_asyfm(mus_any *ptr)
{
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s freq: %.3fHz, phase: %.3f, ratio: %.3f, r: %.3f",
	       mus_name(ptr),
	       mus_frequency(ptr),
	       mus_phase(ptr),
	       ((asyfm *)ptr)->ratio, 
	       asyfm_r(ptr));
  return(describe_buffer);
}


bool mus_is_asymmetric_fm(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_ASYMMETRIC_FM));
}


mus_float_t mus_asymmetric_fm(mus_any *ptr, mus_float_t index, mus_float_t fm)
{
  asyfm *gen = (asyfm *)ptr;
  mus_float_t result;
  mus_float_t mth;
  mth = gen->ratio * gen->phase;
  result = exp(index * gen->cosr * (gen->one + cos(mth))) * cos(gen->phase + index * gen->sinr * sin(mth));
  /* second index factor added 4-Mar-02 and (+/-)1.0 + cos to normalize amps 6-Sep-07 */
  gen->phase += (gen->freq + fm);
  return(result);
}


mus_float_t mus_asymmetric_fm_unmodulated(mus_any *ptr, mus_float_t index)
{
  asyfm *gen = (asyfm *)ptr;
  mus_float_t result, mth;
  mth = gen->ratio * gen->phase;
  result = exp(index * gen->cosr * (gen->one + cos(mth))) * cos(gen->phase + index * gen->sinr * sin(mth));
  /* second index factor added 4-Mar-02 */
  gen->phase += gen->freq;
  return(result);
}

static mus_any_class ASYMMETRIC_FM_CLASS = {
  MUS_ASYMMETRIC_FM,
  (char *)S_asymmetric_fm,
  &free_asymmetric_fm,
  &describe_asyfm,
  &asyfm_equalp,
  0, 0, 0, 0,
  &asyfm_freq,
  &asyfm_set_freq,
  &asyfm_phase,
  &asyfm_set_phase,
  &asyfm_r,
  &asyfm_set_r,
  &asyfm_increment,
  &asyfm_set_increment,
  &mus_asymmetric_fm,
  MUS_NOT_SPECIAL, 
  NULL, 0,
  &asyfm_ratio, 
  0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &asyfm_reset,
  0,
  &asyfm_copy
};


mus_any *mus_make_asymmetric_fm(mus_float_t freq, mus_float_t phase, mus_float_t r, mus_float_t ratio) /* r default 1.0, ratio 1.0 */
{
 asyfm *gen = NULL;
 if (r == 0.0)
   mus_error(MUS_ARG_OUT_OF_RANGE, S_make_asymmetric_fm ": r can't be 0.0");
 else
   {
     gen = (asyfm *)malloc(sizeof(asyfm));
     gen->core = &ASYMMETRIC_FM_CLASS;
     gen->freq = mus_hz_to_radians(freq);
     gen->phase = phase;
     gen->r = r;
     gen->ratio = ratio;
     gen->cosr = 0.5 * (r - (1.0 / r)); /* 0.5 factor for I/2 */
     gen->sinr = 0.5 * (r + (1.0 / r));
     if ((r > 1.0) ||
	 ((r < 0.0) && (r > -1.0)))
       gen->one = -1.0; 
     else gen->one = 1.0;
   }
 return((mus_any *)gen);
}




/*---------------- nrxysin (sine-summation) and nrxycos ---------------- */

/* the generator uses x and y (frequencies), but it's very common to start up with 0 freqs
 *   and let the fm arg set the frequency, so it seems like we want to give the ratio between
 *   the frequencies at make time, rather than two (possibly dummy) frequencies).
 *   xy-ratio negative to build (via r) backwards.
 */

#define MAX_R 0.999999
#define MIN_R -0.999999

typedef struct {
  mus_any_class *core;
  mus_float_t freq, phase;
  int n;
  mus_float_t norm, r, r_to_n_plus_1, r_squared_plus_1, y_over_x;
} nrxy;


static void free_nrxy(mus_any *ptr) {free(ptr);}
static void nrxy_reset(mus_any *ptr) {((nrxy *)ptr)->phase = 0.0;}

static mus_any *nrxy_copy(mus_any *ptr)
{
  nrxy *g;
  g = (nrxy *)malloc(sizeof(nrxy));
  memcpy((void *)g, (void *)ptr, sizeof(nrxy));
  return((mus_any *)g);
}

static mus_float_t nrxy_freq(mus_any *ptr) {return(mus_radians_to_hz(((nrxy *)ptr)->freq));}
static mus_float_t nrxy_set_freq(mus_any *ptr, mus_float_t val) {((nrxy *)ptr)->freq = mus_hz_to_radians(val); return(val);}

static mus_float_t nrxy_increment(mus_any *ptr) {return(((nrxy *)ptr)->freq);}
static mus_float_t nrxy_set_increment(mus_any *ptr, mus_float_t val) {((nrxy *)ptr)->freq = val; return(val);}

static mus_float_t nrxy_phase(mus_any *ptr) {return(fmod(((nrxy *)ptr)->phase, TWO_PI));}
static mus_float_t nrxy_set_phase(mus_any *ptr, mus_float_t val) {((nrxy *)ptr)->phase = val; return(val);}

static mus_long_t nrxy_n(mus_any *ptr) {return((mus_long_t)(((nrxy *)ptr)->n));}

static mus_float_t nrxy_y_over_x(mus_any *ptr) {return(((nrxy *)ptr)->y_over_x);}
static mus_float_t nrxy_set_y_over_x(mus_any *ptr, mus_float_t val) {((nrxy *)ptr)->y_over_x = val; return(val);}

static mus_float_t nrxy_r(mus_any *ptr) {return(((nrxy *)ptr)->r);}

static mus_float_t nrxy_set_r(mus_any *ptr, mus_float_t r)
{
  nrxy *gen = (nrxy *)ptr;
  int n;
  n = gen->n;
  if (r > MAX_R) r = MAX_R;
  if (r < MIN_R) r = MIN_R;
  gen->r = r;
  gen->r_to_n_plus_1 = pow(r, n + 1);
  gen->r_squared_plus_1 = 1.0 + r * r;
  if (n == 0)
    gen->norm = 1.0;
  else gen->norm = (pow(fabs(r), n + 1) - 1.0) / (fabs(r) - 1.0); 
  /* fabs here because if r<0.0, we line up at (2k-1)*pi rather than 2k*pi, but
   *   otherwise the waveform is identical
   */
  return(r);
}

static bool nrxy_equalp(mus_any *p1, mus_any *p2)
{
  nrxy *g1 = (nrxy *)p1;
  nrxy *g2 = (nrxy *)p2;
  return((p1 == p2) ||
	 (((g1->core)->type == (g2->core)->type) &&
	  (g1->freq == g2->freq) &&
	  (g1->phase == g2->phase) &&
	  (g1->n == g2->n) &&
	  (g1->r == g2->r) &&
	  (g1->y_over_x == g2->y_over_x)));
}


bool mus_is_nrxysin(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_NRXYSIN));
}


static char *describe_nrxysin(mus_any *ptr)
{
  nrxy *gen = (nrxy *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s frequency: %.3f, ratio: %.3f, phase: %.3f, n: %d, r: %.3f",
	       mus_name(ptr),
	       mus_frequency(ptr),
	       gen->y_over_x,
	       mus_phase(ptr),
	       gen->n, 
	       nrxy_r(ptr));
  return(describe_buffer);
}


mus_float_t mus_nrxysin(mus_any *ptr, mus_float_t fm)
{
  /* Jolley 475 but 0..n rather than 0..n-1 */
  /*   see also Durell and Robson "Advanced Trigonometry" p 175 */

  nrxy *gen = (nrxy *)ptr;
  mus_float_t x, y, r, divisor;
  int n;

  x = gen->phase;
  n = gen->n;
  r = gen->r;
  gen->phase += (gen->freq + fm);
  

  if (gen->y_over_x == 1.0)
    {
#if (!HAVE_SINCOS)
      divisor = gen->norm * (gen->r_squared_plus_1 - (2 * r * cos(x)));
      if (DIVISOR_NEAR_ZERO(divisor))
	return(0.0);
      return((sin(x) - gen->r_to_n_plus_1 * (sin(x * (n + 2)) - r * sin(x * (n + 1)))) / divisor);
#else
      double sx, cx, snx, cnx;
      sincos(x, &sx, &cx);
      divisor = gen->norm * (gen->r_squared_plus_1 - (2 * r * cx));
      if (DIVISOR_NEAR_ZERO(divisor))
	return(0.0);
      sincos((n + 1) * x, &snx, &cnx);
      return((sx - gen->r_to_n_plus_1 * (sx * cnx + (cx - r) * snx)) / divisor);
#endif
    }

#if HAVE_SINCOS
  {
    double xs, xc, ys, yc, nys, nyc, sin_x_y, sin_x_ny, sin_x_n1y, cos_x_ny;

    y = x * gen->y_over_x;
    sincos(y, &ys, &yc);
    divisor = gen->norm * (gen->r_squared_plus_1 - (2 * r * yc));
    if (DIVISOR_NEAR_ZERO(divisor))
      return(0.0);

    sincos(x, &xs, &xc);
    sincos(n * y, &nys, &nyc);
    sin_x_y = (xs * yc - ys * xc);
    sin_x_ny = (xs * nyc + nys * xc);
    cos_x_ny = (xc * nyc - xs * nys);
    sin_x_n1y = (sin_x_ny * yc + cos_x_ny * ys);

    return((xs -
	    r * sin_x_y - 
	    gen->r_to_n_plus_1 * (sin_x_n1y - r * sin_x_ny)) / divisor);
  }
#else
  y = x * gen->y_over_x;
  divisor = gen->norm * (gen->r_squared_plus_1 - (2 * r * cos(y)));
  if (DIVISOR_NEAR_ZERO(divisor))
    return(0.0);

  return((sin(x) - 
	  r * sin(x - y) - 
	  gen->r_to_n_plus_1 * (sin(x + (n + 1) * y) - 
				r * sin(x + n * y))) / 
	 divisor);
#endif
}


static mus_float_t run_nrxysin(mus_any *ptr, mus_float_t fm, mus_float_t unused) {return(mus_nrxysin(ptr, fm));}


static mus_any_class NRXYSIN_CLASS = {
  MUS_NRXYSIN,
  (char *)S_nrxysin,
  &free_nrxy,
  &describe_nrxysin,
  &nrxy_equalp,
  0, 0, 
  &nrxy_n, 0,
  &nrxy_freq,
  &nrxy_set_freq,
  &nrxy_phase,
  &nrxy_set_phase,
  &nrxy_r,
  &nrxy_set_r,
  &nrxy_increment,
  &nrxy_set_increment,
  &run_nrxysin,
  MUS_NOT_SPECIAL, 
  NULL, 0,
  &nrxy_y_over_x, 
  &nrxy_set_y_over_x,
  0, 0, 0, 0, 
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &nrxy_reset,
  0,
  &nrxy_copy
};


mus_any *mus_make_nrxysin(mus_float_t frequency, mus_float_t y_over_x, int n, mus_float_t r)
{
  nrxy *gen;
  gen = (nrxy *)malloc(sizeof(nrxy));
  gen->core = &NRXYSIN_CLASS;
  gen->freq = mus_hz_to_radians(frequency);
  gen->y_over_x = y_over_x;
  gen->phase = 0.0;
  gen->n = n;
  nrxy_set_r((mus_any *)gen, r);
  return((mus_any *)gen);
}


bool mus_is_nrxycos(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_NRXYCOS));
}


static char *describe_nrxycos(mus_any *ptr)
{
  nrxy *gen = (nrxy *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s frequency: %.3f, ratio: %.3f, phase: %.3f, n: %d, r: %.3f",
	       mus_name(ptr),
	       mus_frequency(ptr),
	       gen->y_over_x,
	       mus_phase(ptr),
	       gen->n, 
	       nrxy_r(ptr));
  return(describe_buffer);
}


mus_float_t mus_nrxycos(mus_any *ptr, mus_float_t fm)
{
  nrxy *gen = (nrxy *)ptr;
  mus_float_t x, y, r, divisor;
  int n;

  x = gen->phase;
  y = x * gen->y_over_x;
  n = gen->n;
  r = gen->r;

  gen->phase += (gen->freq + fm);

#if HAVE_SINCOS
  {
    double xs, xc, ys, yc, nys, nyc, cos_x_y, cos_x_ny, cos_x_n1y, sin_x_ny;

    sincos(y, &ys, &yc);
    divisor = gen->norm * (gen->r_squared_plus_1 - (2 * r * yc));
    if (DIVISOR_NEAR_ZERO(divisor))
      return(1.0);

    sincos(x, &xs, &xc);
    sincos(n * y, &nys, &nyc);
    cos_x_y = (xc * yc + ys * xs);
    sin_x_ny = (xs * nyc + nys * xc);
    cos_x_ny = (xc * nyc - xs * nys);
    cos_x_n1y = (cos_x_ny * yc - sin_x_ny * ys);
    return((xc -
	    r * cos_x_y - 
	    gen->r_to_n_plus_1 * (cos_x_n1y - r * cos_x_ny)) / divisor);
  }
#else
  divisor = gen->norm * (gen->r_squared_plus_1 - (2 * r * cos(y)));
  if (DIVISOR_NEAR_ZERO(divisor))
    return(1.0);
  /* this can happen if r>0.9999999 or thereabouts;
   */

  return((cos(x) - 
	  r * cos(x - y) - 
	  gen->r_to_n_plus_1 * (cos(x + (n + 1) * y) - 
				r * cos(x + n * y))) / 
	 divisor);
#endif
}


static mus_float_t run_nrxycos(mus_any *ptr, mus_float_t fm, mus_float_t unused) {return(mus_nrxycos(ptr, fm));}


static mus_any_class NRXYCOS_CLASS = {
  MUS_NRXYCOS,
  (char *)S_nrxycos,
  &free_nrxy,
  &describe_nrxycos,
  &nrxy_equalp,
  0, 0, 
  &nrxy_n, 0,
  &nrxy_freq,
  &nrxy_set_freq,
  &nrxy_phase,
  &nrxy_set_phase,
  &nrxy_r,
  &nrxy_set_r,
  &nrxy_increment,
  &nrxy_set_increment,
  &run_nrxycos,
  MUS_NOT_SPECIAL, 
  NULL, 0,
  &nrxy_y_over_x, 
  &nrxy_set_y_over_x,
  0, 0, 0, 0, 
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &nrxy_reset,
  0,
  &nrxy_copy
};


mus_any *mus_make_nrxycos(mus_float_t frequency, mus_float_t y_over_x, int n, mus_float_t r)
{
  nrxy *gen;
  gen = (nrxy *)mus_make_nrxysin(frequency, y_over_x, n, r);
  gen->core = &NRXYCOS_CLASS;
  return((mus_any *)gen);
}



/* ---------------- rxykcos/sin ---------------- */

typedef struct {
  mus_any_class *core;
  mus_float_t r, ar;
  mus_float_t freq, phase;
  mus_float_t ratio;
} rxyk;


static void free_rxykcos(mus_any *ptr) {free(ptr);}
static void rxyk_reset(mus_any *ptr) {((rxyk *)ptr)->phase = 0.0;}

static mus_any *rxyk_copy(mus_any *ptr)
{
  rxyk *g;
  g = (rxyk *)malloc(sizeof(rxyk));
  memcpy((void *)g, (void *)ptr, sizeof(rxyk));
  return((mus_any *)g);
}

static mus_float_t rxyk_freq(mus_any *ptr) {return(mus_radians_to_hz(((rxyk *)ptr)->freq));}
static mus_float_t rxyk_set_freq(mus_any *ptr, mus_float_t val) {((rxyk *)ptr)->freq = mus_hz_to_radians(val); return(val);}

static mus_float_t rxyk_increment(mus_any *ptr) {return(((rxyk *)ptr)->freq);}
static mus_float_t rxyk_set_increment(mus_any *ptr, mus_float_t val) {((rxyk *)ptr)->freq = val; return(val);}

static mus_float_t rxyk_phase(mus_any *ptr) {return(fmod(((rxyk *)ptr)->phase, TWO_PI));}
static mus_float_t rxyk_set_phase(mus_any *ptr, mus_float_t val) {((rxyk *)ptr)->phase = val; return(val);}

static mus_float_t rxyk_ratio(mus_any *ptr) {return(((rxyk *)ptr)->ratio);}

static mus_float_t rxyk_r(mus_any *ptr) {return(((rxyk *)ptr)->r);}

static mus_float_t rxyk_set_r(mus_any *ptr, mus_float_t val) 
{
  rxyk *gen = (rxyk *)ptr;
  gen->r = val; 
  gen->ar = 1.0 / exp(fabs(val));
  return(val);
}

static mus_float_t run_rxykcos(mus_any *ptr, mus_float_t fm, mus_float_t unused) {return(mus_rxykcos(ptr, fm));}
static mus_float_t run_rxyksin(mus_any *ptr, mus_float_t fm, mus_float_t unused) {return(mus_rxyksin(ptr, fm));}

static bool rxyk_equalp(mus_any *p1, mus_any *p2)
{
  return((p1 == p2) ||
	 (((p1->core)->type == (p2->core)->type) &&
	  ((((rxyk *)p1)->freq) == (((rxyk *)p2)->freq)) && 
	  ((((rxyk *)p1)->phase) == (((rxyk *)p2)->phase)) &&
	  ((((rxyk *)p1)->ratio) == (((rxyk *)p2)->ratio)) &&
	  ((((rxyk *)p1)->r) == (((rxyk *)p2)->r))));
}


static char *describe_rxyk(mus_any *ptr)
{
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s freq: %.3fHz, phase: %.3f, ratio: %.3f, r: %.3f",
	       mus_name(ptr),
	       mus_frequency(ptr),
	       mus_phase(ptr),
	       ((rxyk *)ptr)->ratio, 
	       rxyk_r(ptr));
  return(describe_buffer);
}


bool mus_is_rxykcos(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_RXYKCOS));
}


mus_float_t mus_rxykcos(mus_any *ptr, mus_float_t fm)
{
  rxyk *gen = (rxyk *)ptr;
  mus_float_t result, rx;

  rx = gen->ratio * gen->phase;
  result = gen->ar * exp(gen->r * cos(rx)) * cos(gen->phase + (gen->r * sin(rx)));
  gen->phase += (fm + gen->freq);

  return(result);
}


static mus_any_class RXYKCOS_CLASS = {
  MUS_RXYKCOS,
  (char *)S_rxykcos,
  &free_rxykcos,
  &describe_rxyk,
  &rxyk_equalp,
  0, 0, 0, 0,
  &rxyk_freq,
  &rxyk_set_freq,
  &rxyk_phase,
  &rxyk_set_phase,
  &rxyk_r,
  &rxyk_set_r,
  &rxyk_increment,
  &rxyk_set_increment,
  &run_rxykcos,
  MUS_NOT_SPECIAL, 
  NULL, 0,
  &rxyk_ratio, 
  0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &rxyk_reset,
  0,
  &rxyk_copy
};


mus_any *mus_make_rxykcos(mus_float_t freq, mus_float_t phase, mus_float_t r, mus_float_t ratio) /* r default 0.5, ratio 1.0 */
{
 rxyk *gen;
 gen = (rxyk *)malloc(sizeof(rxyk));
 gen->core = &RXYKCOS_CLASS;
 gen->freq = mus_hz_to_radians(freq);
 gen->phase = phase;
 gen->r = r;
 gen->ar = 1.0 / exp(fabs(r));
 gen->ratio = ratio;
 return((mus_any *)gen);
}



bool mus_is_rxyksin(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_RXYKSIN));
}


mus_float_t mus_rxyksin(mus_any *ptr, mus_float_t fm)
{
  rxyk *gen = (rxyk *)ptr;
  mus_float_t result, rx;

  rx = gen->ratio * gen->phase;
  result = gen->ar * exp(gen->r * cos(rx)) * sin(gen->phase + (gen->r * sin(rx)));
  gen->phase += (fm + gen->freq);

  return(result);
}


static mus_any_class RXYKSIN_CLASS = {
  MUS_RXYKSIN,
  (char *)S_rxyksin,
  &free_rxykcos,
  &describe_rxyk,
  &rxyk_equalp,
  0, 0, 0, 0,
  &rxyk_freq,
  &rxyk_set_freq,
  &rxyk_phase,
  &rxyk_set_phase,
  &rxyk_r,
  &rxyk_set_r,
  &rxyk_increment,
  &rxyk_set_increment,
  &run_rxyksin,
  MUS_NOT_SPECIAL, 
  NULL, 0,
  &rxyk_ratio, 
  0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &rxyk_reset,
  0,
  &rxyk_copy
};


mus_any *mus_make_rxyksin(mus_float_t freq, mus_float_t phase, mus_float_t r, mus_float_t ratio) /* r default 0.5, ratio 1.0 */
{
 rxyk *gen;
 gen = (rxyk *)malloc(sizeof(rxyk));
 gen->core = &RXYKSIN_CLASS;
 gen->freq = mus_hz_to_radians(freq);
 gen->phase = phase;
 gen->r = r;
 gen->ar = 1.0 / exp(fabs(r));
 gen->ratio = ratio;
 return((mus_any *)gen);
}




/* ---------------- table lookup ---------------- */

typedef struct {
  mus_any_class *core;
  mus_float_t freq, internal_mag, phase;
  mus_float_t *table;
  mus_long_t table_size;
  mus_interp_t type;
  bool table_allocated;
  mus_float_t yn1;
  mus_float_t (*tbl_look)(mus_any *ptr, mus_float_t fm);
  mus_float_t (*tbl_look_unmod)(mus_any *ptr);
} tbl;


mus_float_t *mus_partials_to_wave(mus_float_t *partial_data, int partials, mus_float_t *table, mus_long_t table_size, bool normalize)
{
  int partial, k;
  if (!table) return(NULL);
  mus_clear_floats(table, table_size);
  for (partial = 0, k = 1; partial < partials; partial++, k += 2)
    {
      mus_float_t amp;
      amp = partial_data[k];
      if (amp != 0.0)
	{
	  mus_long_t i;
	  mus_float_t freq, angle;
	  freq = (partial_data[partial * 2] * TWO_PI) / (mus_float_t)table_size;
	  for (i = 0, angle = 0.0; i < table_size; i++, angle += freq) 
	    table[i] += amp * sin(angle);
	}
    }
  if (normalize) 
    return(array_normalize(table, table_size));
  return(table);
}


mus_float_t *mus_phase_partials_to_wave(mus_float_t *partial_data, int partials, mus_float_t *table, mus_long_t table_size, bool normalize)
{
  int partial, k, n;
  if (!table) return(NULL);
  mus_clear_floats(table, table_size);
  for (partial = 0, k = 1, n = 2; partial < partials; partial++, k += 3, n += 3)
    {
      mus_float_t amp;
      amp = partial_data[k];
      if (amp != 0.0)
	{
	  mus_long_t i;
	  mus_float_t freq, angle;
	  freq = (partial_data[partial * 3] * TWO_PI) / (mus_float_t)table_size;
	  for (i = 0, angle = partial_data[n]; i < table_size; i++, angle += freq) 
	    table[i] += amp * sin(angle);
	}
    }
  if (normalize) 
    return(array_normalize(table, table_size));
  return(table);
}


mus_float_t mus_table_lookup(mus_any *ptr, mus_float_t fm)
{
  return(((tbl *)ptr)->tbl_look(ptr, fm));  
}

static mus_float_t table_look_linear(mus_any *ptr, mus_float_t fm)
{
  tbl *gen = (tbl *)ptr;

  /* we're checking already for out-of-range indices, so mus_array_interp is more than we need */
  mus_long_t int_part;
  mus_float_t frac_part, f1;
  
  int_part = (mus_long_t)(gen->phase); /* floor(gen->phase) -- slow! modf is even worse */
  frac_part = gen->phase - int_part;
  f1 = gen->table[int_part];
  int_part++;
  
  if (int_part == gen->table_size)
    gen->yn1 = f1 + frac_part * (gen->table[0] - f1);
  else gen->yn1 = f1 + frac_part * (gen->table[int_part] - f1);

  gen->phase += (gen->freq + (fm * gen->internal_mag));
  if ((gen->phase >= gen->table_size) || 
      (gen->phase < 0.0))
    {
      gen->phase = fmod(gen->phase, gen->table_size);
      if (gen->phase < 0.0) 
	gen->phase += gen->table_size;
    }
  return(gen->yn1);
}


static mus_float_t table_look_any(mus_any *ptr, mus_float_t fm)
{
  tbl *gen = (tbl *)ptr;

  gen->yn1 = mus_interpolate(gen->type, gen->phase, gen->table, gen->table_size, gen->yn1);
  gen->phase += (gen->freq + (fm * gen->internal_mag));
  if ((gen->phase >= gen->table_size) || 
      (gen->phase < 0.0))
    {
      gen->phase = fmod(gen->phase, gen->table_size);
      if (gen->phase < 0.0) 
	gen->phase += gen->table_size;
    }
  return(gen->yn1);
}


mus_float_t mus_table_lookup_unmodulated(mus_any *ptr)
{
  return(((tbl *)ptr)->tbl_look_unmod(ptr));  
}

static mus_float_t table_look_unmodulated_linear(mus_any *ptr)
{
  tbl *gen = (tbl *)ptr;
  mus_long_t int_part;
  mus_float_t frac_part, f1;
  
  int_part = (mus_long_t)(gen->phase);
  frac_part = gen->phase - int_part;
  f1 = gen->table[int_part];
  int_part++;
  
  if (int_part == gen->table_size)
    f1 += frac_part * (gen->table[0] - f1);
  else f1 += frac_part * (gen->table[int_part] - f1);

  gen->phase += gen->freq;
  if ((gen->phase >= gen->table_size) || 
      (gen->phase < 0.0))
    {
      gen->phase = fmod(gen->phase, gen->table_size);
      if (gen->phase < 0.0) 
	gen->phase += gen->table_size;
    }
  return(f1);
}


static mus_float_t table_look_unmodulated_any(mus_any *ptr)
{
  tbl *gen = (tbl *)ptr;

  gen->yn1 = mus_interpolate(gen->type, gen->phase, gen->table, gen->table_size, gen->yn1);
  gen->phase += gen->freq;
  if ((gen->phase >= gen->table_size) || 
      (gen->phase < 0.0))
    {
      gen->phase = fmod(gen->phase, gen->table_size);
      if (gen->phase < 0.0) 
	gen->phase += gen->table_size;
    }
  return(gen->yn1);
}


static mus_float_t run_table_lookup(mus_any *ptr, mus_float_t fm, mus_float_t unused) {return(((tbl *)ptr)->tbl_look(ptr, fm)); }

bool mus_is_table_lookup(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_TABLE_LOOKUP));
}

static mus_long_t table_lookup_length(mus_any *ptr) {return(((tbl *)ptr)->table_size);}
static mus_float_t *table_lookup_data(mus_any *ptr) {return(((tbl *)ptr)->table);}

static mus_float_t table_lookup_freq(mus_any *ptr) {return((((tbl *)ptr)->freq * sampling_rate) / (((tbl *)ptr)->table_size));}
static mus_float_t table_lookup_set_freq(mus_any *ptr, mus_float_t val) {((tbl *)ptr)->freq = (val * ((tbl *)ptr)->table_size) / sampling_rate; return(val);}

static mus_float_t table_lookup_increment(mus_any *ptr) {return(((tbl *)ptr)->freq);}
static mus_float_t table_lookup_set_increment(mus_any *ptr, mus_float_t val) {((tbl *)ptr)->freq = val; return(val);}

static mus_float_t table_lookup_phase(mus_any *ptr) {return(fmod(((TWO_PI * ((tbl *)ptr)->phase) / ((tbl *)ptr)->table_size), TWO_PI));}
static mus_float_t table_lookup_set_phase(mus_any *ptr, mus_float_t val) {((tbl *)ptr)->phase = (val * ((tbl *)ptr)->table_size) / TWO_PI; return(val);}

static int table_lookup_interp_type(mus_any *ptr) {return((int)(((tbl *)ptr)->type));} /* ints here and elsewhere to fit mus_channels method = interp-type */

static void table_lookup_reset(mus_any *ptr) {((tbl *)ptr)->phase = 0.0;}


static char *describe_table_lookup(mus_any *ptr)
{
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s freq: %.3fHz, phase: %.3f, length: %d, interp: %s",
	       mus_name(ptr),
	       mus_frequency(ptr),
	       mus_phase(ptr),
	       (int)mus_length(ptr),
	       mus_interp_type_to_string(table_lookup_interp_type(ptr)));
  return(describe_buffer);
}


static bool table_lookup_equalp(mus_any *p1, mus_any *p2)
{
  tbl *t1 = (tbl *)p1;
  tbl *t2 = (tbl *)p2;
  if (p1 == p2) return(true);
  return((t1) && (t2) &&
	 (t1->core->type == t2->core->type) &&
	 (t1->table_size == t2->table_size) &&
	 (t1->freq == t2->freq) &&
	 (t1->phase == t2->phase) &&
	 (t1->type == t2->type) &&
	 (t1->internal_mag == t2->internal_mag) &&
	 (clm_arrays_are_equal(t1->table, t2->table, t1->table_size)));
}


static void free_table_lookup(mus_any *ptr) 
{
  tbl *gen = (tbl *)ptr;
  if ((gen->table) && (gen->table_allocated)) free(gen->table);
  free(gen); 
}

static mus_any *tbl_copy(mus_any *ptr)
{
  tbl *g, *p;

  p = (tbl *)ptr;
  g = (tbl *)malloc(sizeof(tbl));
  memcpy((void *)g, (void *)ptr, sizeof(tbl));

  g->table = (mus_float_t *)malloc(g->table_size * sizeof(mus_float_t));
  mus_copy_floats(g->table, p->table, g->table_size);
  g->table_allocated = true;

  return((mus_any *)g);
}

static mus_float_t *table_set_data(mus_any *ptr, mus_float_t *val) 
{
  tbl *gen = (tbl *)ptr;
  if (gen->table_allocated) {free(gen->table); gen->table_allocated = false;}
  gen->table = val; 
  return(val);
}


static mus_any_class TABLE_LOOKUP_CLASS = {
  MUS_TABLE_LOOKUP,
  (char *)S_table_lookup,
  &free_table_lookup,
  &describe_table_lookup,
  &table_lookup_equalp,
  &table_lookup_data,
  &table_set_data,
  &table_lookup_length,
  0,
  &table_lookup_freq,
  &table_lookup_set_freq,
  &table_lookup_phase,
  &table_lookup_set_phase,
  &fallback_scaler, 0,
  &table_lookup_increment,
  &table_lookup_set_increment,
  &run_table_lookup,
  MUS_NOT_SPECIAL, 
  NULL,
  &table_lookup_interp_type,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &table_lookup_reset,
  0, &tbl_copy
};


mus_any *mus_make_table_lookup(mus_float_t freq, mus_float_t phase, mus_float_t *table, mus_long_t table_size, mus_interp_t type)
{
  tbl *gen;
  gen = (tbl *)malloc(sizeof(tbl));
  gen->core = &TABLE_LOOKUP_CLASS;
  gen->table_size = table_size;
  gen->internal_mag = table_size / TWO_PI;
  gen->freq = (freq * table_size) / sampling_rate;
  gen->phase = (fmod(phase, TWO_PI) * table_size) / TWO_PI;
  gen->type = type;
  if (type == MUS_INTERP_LINEAR)
    {
      gen->tbl_look = table_look_linear;
      gen->tbl_look_unmod = table_look_unmodulated_linear;
    }
  else
    {
      gen->tbl_look = table_look_any;
      gen->tbl_look_unmod = table_look_unmodulated_any;
    }
  gen->yn1 = 0.0;
  if (table)
    {
      gen->table = table;
      gen->table_allocated = false;
    }
  else
    {
      gen->table = (mus_float_t *)calloc(table_size, sizeof(mus_float_t));
      gen->table_allocated = true;
    }
  return((mus_any *)gen);
}



/* ---------------- polywave ---------------- */

mus_float_t *mus_partials_to_polynomial(int npartials, mus_float_t *partials, mus_polynomial_t kind)
{
  /* coeffs returned in partials */
  int i;
  mus_long_t *T0, *T1, *Tn;
  mus_float_t *Cc1;

  T0 = (mus_long_t *)calloc(npartials + 1, sizeof(mus_long_t));
  T1 = (mus_long_t *)calloc(npartials + 1, sizeof(mus_long_t));
  Tn = (mus_long_t *)calloc(npartials + 1, sizeof(mus_long_t));
  Cc1 = (mus_float_t *)calloc(npartials + 1, sizeof(mus_float_t));

  if (kind == MUS_CHEBYSHEV_FIRST_KIND)
    T0[0] = 1;
  else T0[0] = 0;
  T1[1] = 1;

  Cc1[0] = partials[0]; /* DC requested? */

  for (i = 1; i < npartials; i++)
    {
      int k;
      mus_float_t amp;
      amp = partials[i];
      if (amp != 0.0)
	{
	  if (kind == MUS_CHEBYSHEV_FIRST_KIND)
	    for (k = 0; k <= i; k++) 
	      Cc1[k] += (amp * T1[k]);
	  else
	    for (k = 1; k <= i; k++) 
	      Cc1[k - 1] += (amp * T1[k]);
	}
      for (k = i + 1; k > 0; k--) 
	Tn[k] = (2 * T1[k - 1]) - T0[k];
      Tn[0] = -T0[0];
      for (k = i + 1; k >= 0; k--)
	{
	  T0[k] = T1[k];
	  T1[k] = Tn[k];
	}
    }

  for (i = 0; i < npartials; i++) 
    partials[i] = Cc1[i];

  free(T0);
  free(T1);
  free(Tn);
  free(Cc1);
  return(partials);
}


mus_float_t *mus_normalize_partials(int num_partials, mus_float_t *partials)
{
  int i;
  mus_float_t sum = 0.0;
  for (i = 0; i < num_partials; i++)
    sum += fabs(partials[2 * i + 1]);
  if ((sum != 0.0) &&
      (sum != 1.0))
    {
      sum = 1.0 / sum;
      for (i = 0; i < num_partials; i++)
	partials[2 * i + 1] *= sum;
    }
  return(partials);
}


typedef struct {
  mus_any_class *core;
  mus_float_t phase, freq;
  mus_float_t *coeffs, *ucoeffs;
  int n, cheby_choice;
  mus_float_t index;
  mus_float_t (*polyw)(mus_any *ptr, mus_float_t fm);
} pw;


mus_float_t (*mus_polywave_function(mus_any *g))(mus_any *gen, mus_float_t fm)
{
  if (mus_is_polywave(g))
    return(((pw *)g)->polyw);
  return(NULL);
}

static void free_pw(mus_any *pt) {free(pt);}

static mus_any *pw_copy(mus_any *ptr)
{
  pw *g;
  g = (pw *)malloc(sizeof(pw));
  memcpy((void *)g, (void *)ptr, sizeof(pw));
  return((mus_any *)g);
}

static void pw_reset(mus_any *ptr)
{
  pw *gen = (pw *)ptr;
  gen->phase = 0.0;
}


static bool pw_equalp(mus_any *p1, mus_any *p2)
{
  pw *w1 = (pw *)p1;
  pw *w2 = (pw *)p2;
  if (p1 == p2) return(true);
  return((w1) && (w2) &&
	 (w1->core->type == w2->core->type) &&
	 (w1->freq == w2->freq) &&
	 (w1->phase == w2->phase) &&
	 (w1->n == w2->n) &&
	 (w1->index == w2->index) &&
	 (w1->cheby_choice == w2->cheby_choice) &&
	 (clm_arrays_are_equal(w1->coeffs, w2->coeffs, w1->n)));
}


static mus_float_t pw_freq(mus_any *ptr) {return(mus_radians_to_hz(((pw *)ptr)->freq));}
static mus_float_t pw_set_freq(mus_any *ptr, mus_float_t val) {((pw *)ptr)->freq = mus_hz_to_radians(val); return(val);}

static mus_float_t pw_increment(mus_any *ptr) {return(((pw *)ptr)->freq);}
static mus_float_t pw_set_increment(mus_any *ptr, mus_float_t val) {((pw *)ptr)->freq = val; return(val);}

static mus_float_t pw_phase(mus_any *ptr) {return(fmod(((pw *)ptr)->phase, TWO_PI));}
static mus_float_t pw_set_phase(mus_any *ptr, mus_float_t val) {((pw *)ptr)->phase = val; return(val);}

static mus_long_t pw_n(mus_any *ptr) {return(((pw *)ptr)->n);}
static mus_long_t pw_set_n(mus_any *ptr, mus_long_t val) {((pw *)ptr)->n = (int)val; return(val);}

static mus_float_t *pw_data(mus_any *ptr) {return(((pw *)ptr)->coeffs);}
static mus_float_t *pw_udata(mus_any *ptr) {return(((pw *)ptr)->ucoeffs);}
static mus_float_t *pw_set_data(mus_any *ptr, mus_float_t *val) {((pw *)ptr)->coeffs = val; return(val);}

static mus_float_t pw_xcoeff(mus_any *ptr, int index) {return(((pw *)ptr)->coeffs[index]);}
static mus_float_t pw_set_xcoeff(mus_any *ptr, int index, mus_float_t val) {((pw *)ptr)->coeffs[index] = val; return(val);}

static mus_float_t pw_ycoeff(mus_any *ptr, int index) {if (((pw *)ptr)->ucoeffs) return(((pw *)ptr)->ucoeffs[index]); return(0.0);}
static mus_float_t pw_set_ycoeff(mus_any *ptr, int index, mus_float_t val) {if (((pw *)ptr)->ucoeffs) ((pw *)ptr)->ucoeffs[index] = val; return(val);}

static mus_float_t pw_index(mus_any *ptr) {return(((pw *)ptr)->index);}
static mus_float_t pw_set_index(mus_any *ptr, mus_float_t val) {((pw *)ptr)->index = val; return(val);}

static int pw_choice(mus_any *ptr) {return(((pw *)ptr)->cheby_choice);}


mus_float_t mus_chebyshev_tu_sum(mus_float_t x, int n, mus_float_t *tn, mus_float_t *un)
{
  /* the Clenshaw algorithm -- beware of -cos(nx) where you'd expect cos(nx) */
  mus_float_t x2, tb, tb1 = 0.0, tb2, cx, ub, ub1 = 0.0;
  mus_float_t *tp, *up;

  if (n == 1) return(tn[0]); /* added 18-Oct-17 -- looks plausible */
  cx = cos(x);
  x2 = 2.0 * cx;

  tp = (mus_float_t *)(tn + n - 1);
  up = (mus_float_t *)(un + n - 1);
  tb = (*tp--);
  ub = (*up--);

  while (up != un)
    {
      mus_float_t ub2;
      tb2 = tb1;
      tb1 = tb;
      tb = x2 * tb1 - tb2 + (*tp--);

      ub2 = ub1;
      ub1 = ub;
      ub = x2 * ub1 - ub2 + (*up--);
    }

  tb2 = tb1;
  tb1 = tb;
  tb = x2 * tb1 - tb2 + tn[0];

  return((mus_float_t)((tb - tb1 * cx) + (sin(x) * ub)));
}


mus_float_t mus_chebyshev_t_sum(mus_float_t x, int n, mus_float_t *tn)
{
  int i;
  mus_float_t x2, b, b1 = 0.0, cx;

  cx = cos(x);
  x2 = 2.0 * cx;

  /* Tn calc */
  b = tn[n - 1];
  for (i = n - 2; i >= 0; i--)
    {
      mus_float_t b2;
      b2 = b1;
      b1 = b;
      b = x2 * b1 - b2 + tn[i];
    }
  return((mus_float_t)(b - b1 * cx));
}

#if 0
/* here is the trick to do odd Tn without doing the intervening evens: */
(define (mus-chebyshev-odd-t-sum x n t2n)
  (let* ((b1 0.0)
	 (b2 0.0)
	 (cx1 (cos x))
	 (cx (- (* 2 cx1 cx1) 1))
	 (x2 (* 2.0 cx))
	 (b (vct-ref t2n (- n 1))))
    (do ((i (- n 2) (1- i)))
	((< i 0))
      (set! b2 b1)
      (set! b1 b)
      (set! b (- (+ (* b1 x2) (vct-ref t2n i)) b2)))
    (* cx1 (- b b1))))

(with-sound () 
  (let ((t2n (vct 0.5 0.25 0.25))
	(x 0.0)
	(dx (hz->radians 10.0)))
    (do ((i 0 (+ i 1)))
	((= i 22050))
      (outa i (mus-chebyshev-odd-t-sum x 3 t2n))
      (set! x (+ x dx)))))
#endif


mus_float_t mus_chebyshev_u_sum(mus_float_t x, int n, mus_float_t *un)
{
  int i;
  mus_float_t x2, b, b1 = 0.0, cx;

  cx = cos(x);
  x2 = 2.0 * cx;

  /* Un calc */
  b = un[n - 1];
  for (i = n - 2; i > 0; i--)
    {
      mus_float_t b2;
      b2 = b1;
      b1 = b;
      b = x2 * b1 - b2 + un[i];
    }

  return((mus_float_t)(sin(x) * b));
}


static mus_float_t mus_chebyshev_t_sum_with_index(mus_float_t x, mus_float_t index, int n, mus_float_t *tn)
{
  int i;
  mus_float_t x2, b, b1 = 0.0, b2, cx;
  cx = index * cos(x);
  x2 = 2.0 * cx;

  /* Tn calc */
  b = tn[n - 1];
  i = n - 2;
  while (i >= 4)
    {
      b2 = b1;
      b1 = b;
      b = x2 * b1 - b2 + tn[i--];

      b2 = b1;
      b1 = b;
      b = x2 * b1 - b2 + tn[i--];

      b2 = b1;
      b1 = b;
      b = x2 * b1 - b2 + tn[i--];

      b2 = b1;
      b1 = b;
      b = x2 * b1 - b2 + tn[i--];
    }
  for (; i >= 0; i--)
    {
      b2 = b1;
      b1 = b;
      b = x2 * b1 - b2 + tn[i];
    }
  return((mus_float_t)(b - b1 * cx));
}


static mus_float_t mus_chebyshev_t_sum_with_index_2(mus_float_t x, mus_float_t index, int n, mus_float_t *tn)
{
  int i;
  mus_float_t x2, b, b1 = 0.0, cx;

  cx = index * cos(x);
  x2 = 2.0 * cx;

  /* Tn calc */
  b = tn[n - 1];
  for (i = n - 2; i > 0;)
    {
      mus_float_t b2;
      b2 = b1;
      b1 = b;
      b = x2 * b1 - b2 + tn[i--];

      b2 = b1;
      b1 = b;
      b = x2 * b1 - b2 + tn[i--];
    }
  return((mus_float_t)(b - b1 * cx));
}


static mus_float_t mus_chebyshev_t_sum_with_index_3(mus_float_t x, mus_float_t index, int n, mus_float_t *tn)
{
  int i;
  mus_float_t x2, b, b1 = 0.0, cx;

  cx = index * cos(x);
  x2 = 2.0 * cx;

  /* Tn calc */
  b = tn[n - 1];
  for (i = n - 2; i > 0;)
    {
      mus_float_t b2;
      b2 = b1;
      b1 = b;
      b = x2 * b1 - b2 + tn[i--];

      b2 = b1;
      b1 = b;
      b = x2 * b1 - b2 + tn[i--];

      b2 = b1;
      b1 = b;
      b = x2 * b1 - b2 + tn[i--];
    }
  return((mus_float_t)(b - b1 * cx));
}


static mus_float_t mus_chebyshev_t_sum_with_index_5(mus_float_t x, mus_float_t index, int n, mus_float_t *tn)
{
  int i;
  mus_float_t x2, b, b1 = 0.0, cx;

  cx = index * cos(x);
  x2 = 2.0 * cx;

  /* Tn calc */
  b = tn[n - 1];
  for (i = n - 2; i > 0;) /* this was >= ?? (also cases above) -- presumably a copy-and-paste typo? */
    {
      mus_float_t b2;
      b2 = b1;
      b1 = b;
      b = x2 * b1 - b2 + tn[i--];

      b2 = b1;
      b1 = b;
      b = x2 * b1 - b2 + tn[i--];

      b2 = b1;
      b1 = b;
      b = x2 * b1 - b2 + tn[i--];

      b2 = b1;
      b1 = b;
      b = x2 * b1 - b2 + tn[i--];

      b2 = b1;
      b1 = b;
      b = x2 * b1 - b2 + tn[i--];
    }
  return((mus_float_t)(b - b1 * cx));
}


static mus_float_t mus_chebyshev_u_sum_with_index(mus_float_t x, mus_float_t index, int n, mus_float_t *un)
{
  int i;
  mus_float_t x2, b, b1 = 0.0, cx;

  cx = index * cos(x);
  x2 = 2.0 * cx;

  /* Un calc */
  b = un[n - 1];
  for (i = n - 2; i > 0; i--)
    {
      mus_float_t b2;
      b2 = b1;
      b1 = b;
      b = x2 * b1 - b2 + un[i];
    }

  return((mus_float_t)(sin(x) * b + un[0]));  /* don't drop the constant, 16-Jan-14 */
}

/* (with-sound () (let ((p (make-polywave 100 (list 0 0.5 1 -.2) mus-chebyshev-second-kind))) (do ((i 0 (+ i 1))) ((= i 1000)) (outa i (polywave p)))))
 */

static mus_float_t polyw_second_2(mus_any *ptr, mus_float_t fm)
{
  pw *gen = (pw *)ptr;
  mus_float_t x;

  x = gen->phase; /* this order (as opposed to saving the full expr below) is much faster?! */
  gen->phase += (gen->freq + fm);
  return(gen->coeffs[1] * sin(x) + gen->coeffs[0]);
}


static mus_float_t polyw_first_1(mus_any *ptr, mus_float_t fm)
{
  pw *gen = (pw *)ptr;
  mus_float_t x;

  x = gen->phase;
  gen->phase += (gen->freq + fm);
  return(gen->index * cos(x));
}


static mus_float_t polyw_first_3(mus_any *ptr, mus_float_t fm)
{
  pw *gen = (pw *)ptr;
  mus_float_t x, cx;
  mus_float_t *tn;

  x = gen->phase;
  tn = gen->coeffs;
  gen->phase += (gen->freq + fm);

  cx = cos(x);
  return((2.0 * cx * tn[2] + tn[1]) * cx - tn[2]);

  /* b = x2 * b1 - b2;, then return(b - b1 * cx)
   *   but x2 = 2 * cx, so b1*(x2 - cx) -> b1 * cx
   *   and the final recursion unrolls.  The old code
   *   (which thought tn[0] might not be 0.0) was:
   * cx = cos(x);
   * x2 = 2.0 * cx;
   * b = tn[2];
   * b2 = b1; -- but b1 is 0
   * b1 = b;  -- b not used so this is tn[2]
   * b = x2 * b1 - b2 + tn[1]; -- b2 is 0.0
   * b2 = b1;
   * b1 = b;
   * b = x2 * b1 - b2 + tn[0];
   * return(b - b1 * cx);
   */
}

/* (with-sound () (let ((p (make-polywave 100 (list 1 .5 2 .25)))) (do ((i 0 (+ i 1))) ((= i 30000)) (outa i (polywave p))))) */


static mus_float_t polyw_first_4(mus_any *ptr, mus_float_t fm)
{
  pw *gen = (pw *)ptr;
  mus_float_t x, x2, b, cx;
  mus_float_t *tn;

  x = gen->phase;
  tn = gen->coeffs;
  gen->phase += (gen->freq + fm);

  cx = cos(x);
  x2 = 2.0 * cx;
  b = x2 * tn[3] + tn[2];  /* was -tn[2]! 19-Feb-14 */
  return((x2 * b - tn[3] + tn[1]) * cx - b);
}


static mus_float_t polyw_first_5(mus_any *ptr, mus_float_t fm)
{
  pw *gen = (pw *)ptr;
  mus_float_t x;
  mus_float_t *tn;
  mus_float_t x2, b, b1, b2, cx;

  x = gen->phase;
  tn = gen->coeffs;
  gen->phase += (gen->freq + fm);

  cx = cos(x);
  x2 = 2.0 * cx;
  b1 = tn[4];
  b = x2 * b1 + tn[3]; 
  b2 = b1; b1 = b; b = x2 * b1 - b2 + tn[2];

  return((x2 * b - b1 + tn[1]) * cx - b);
}


static mus_float_t polyw_first_6(mus_any *ptr, mus_float_t fm)
{
  pw *gen = (pw *)ptr;
  mus_float_t x;
  mus_float_t *tn;
  mus_float_t x2, b, b1, b2, cx;

  x = gen->phase;
  tn = gen->coeffs;
  gen->phase += (gen->freq + fm);

  cx = cos(x);
  x2 = 2.0 * cx;
  b1 = tn[5];
  b = x2 * b1 + tn[4]; 
  b2 = b1; b1 = b; b = x2 * b1 - b2 + tn[3]; 
  b2 = b1; b1 = b; b = x2 * b1 - b2 + tn[2];

  return((x2 * b - b1 + tn[1]) * cx - b);
}


static mus_float_t polyw_first_8(mus_any *ptr, mus_float_t fm)
{
  pw *gen = (pw *)ptr;
  mus_float_t x;
  mus_float_t *tn;
  mus_float_t x2, b, b1, b2, cx;

  x = gen->phase;
  tn = gen->coeffs;
  gen->phase += (gen->freq + fm);

  cx = cos(x);
  x2 = 2.0 * cx;
  b1 = tn[7];
  b = x2 * b1 + tn[6]; 
  b2 = b1; b1 = b; b = x2 * b1 - b2 + tn[5]; 
  b2 = b1; b1 = b; b = x2 * b1 - b2 + tn[4]; 
  b2 = b1; b1 = b; b = x2 * b1 - b2 + tn[3]; 
  b2 = b1; b1 = b; b = x2 * b1 - b2 + tn[2];

  return((x2 * b - b1 + tn[1]) * cx - b);
}


static mus_float_t polyw_first_11(mus_any *ptr, mus_float_t fm)
{
  pw *gen = (pw *)ptr;
  mus_float_t x;
  mus_float_t *tn;
  mus_float_t x2, b, b1, b2, cx;

  x = gen->phase;
  tn = gen->coeffs;
  gen->phase += (gen->freq + fm);

  cx = cos(x);
  x2 = 2.0 * cx;
  b1 = tn[10];
  b = x2 * b1 + tn[9]; 
  b2 = b1; b1 = b; b = x2 * b1 - b2 + tn[8]; 
  b2 = b1; b1 = b; b = x2 * b1 - b2 + tn[7]; 
  b2 = b1; b1 = b; b = x2 * b1 - b2 + tn[6]; 
  b2 = b1; b1 = b; b = x2 * b1 - b2 + tn[5]; 
  b2 = b1; b1 = b; b = x2 * b1 - b2 + tn[4]; 
  b2 = b1; b1 = b; b = x2 * b1 - b2 + tn[3]; 
  b2 = b1; b1 = b; b = x2 * b1 - b2 + tn[2];

  return((x2 * b - b1 + tn[1]) * cx - b);
}


static mus_float_t polyw_first(mus_any *ptr, mus_float_t fm)
{
  pw *gen = (pw *)ptr;
  mus_float_t ph;
  ph = gen->phase;
  gen->phase += (gen->freq + fm);
  return(mus_chebyshev_t_sum_with_index(ph, gen->index, gen->n, gen->coeffs));
}


static mus_float_t polyw_f1(mus_any *ptr, mus_float_t fm)
{
  pw *gen = (pw *)ptr;
  mus_float_t cx;
  cx = gen->index * cos(gen->phase);
  gen->phase += (gen->freq + fm);
  return(cx * gen->coeffs[1]  + gen->coeffs[0]);
}


static mus_float_t polyw_f2(mus_any *ptr, mus_float_t fm)
{
  pw *gen = (pw *)ptr;
  mus_float_t ph;
  ph = gen->phase;
  gen->phase += (gen->freq + fm);
  return(mus_chebyshev_t_sum_with_index_2(ph, gen->index, gen->n, gen->coeffs));
}


static mus_float_t polyw_f3(mus_any *ptr, mus_float_t fm)
{
  pw *gen = (pw *)ptr;
  mus_float_t ph;
  ph = gen->phase;
  gen->phase += (gen->freq + fm);
  return(mus_chebyshev_t_sum_with_index_3(ph, gen->index, gen->n, gen->coeffs));
}


static mus_float_t polyw_f5(mus_any *ptr, mus_float_t fm)
{
  pw *gen = (pw *)ptr;
  mus_float_t ph;
  ph = gen->phase;
  gen->phase += (gen->freq + fm);
  return(mus_chebyshev_t_sum_with_index_5(ph, gen->index, gen->n, gen->coeffs));
}


static mus_float_t polyw_second(mus_any *ptr, mus_float_t fm)
{
  pw *gen = (pw *)ptr;
  mus_float_t ph;
  ph = gen->phase;
  gen->phase += (gen->freq + fm);
  return(mus_chebyshev_u_sum_with_index(ph, gen->index, gen->n, gen->coeffs));
}


static mus_float_t polyw_second_5(mus_any *ptr, mus_float_t fm)
{
  pw *gen = (pw *)ptr;
  mus_float_t *un;
  mus_float_t x, b, b1, cx;

  x = gen->phase;
  gen->phase += (gen->freq + fm);
  /* gen->n is 5 */
  un = gen->coeffs;

  /* this is a candidate for sincos, but gcc is already using it here! */
  cx = 2.0 * cos(x);
  b1 = cx * un[4] + un[3]; 
  b = cx * b1 + gen->index;

  return(sin(x) * (cx * b - b1 + un[1]));
}


static mus_float_t polyw_third(mus_any *ptr, mus_float_t fm)
{
  pw *gen = (pw *)ptr;
  mus_float_t ph;
  ph = gen->phase;
  gen->phase += (gen->freq + fm);
  return(mus_chebyshev_tu_sum(ph, gen->n, gen->coeffs, gen->ucoeffs));
}


mus_float_t mus_polywave(mus_any *ptr, mus_float_t fm)
{
  /* changed to use recursion, rather than polynomial in x, 25-May-08
   *   this algorithm taken from Mason and Handscomb, "Chebyshev Polynomials" p27
   */
  return((((pw *)ptr)->polyw)(ptr, fm));
}


mus_float_t mus_polywave_unmodulated(mus_any *ptr)
{
  return(mus_polywave(ptr, 0.0)); 
}


static mus_float_t run_polywave(mus_any *ptr, mus_float_t fm, mus_float_t ignored) {return(mus_polywave(ptr, fm));}


static char *describe_polywave(mus_any *ptr)
{
  pw *gen = (pw *)ptr;
  char *str;
  char *describe_buffer;
  str = float_array_to_string(gen->coeffs, gen->n, 0);
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s freq: %.3fHz, phase: %.3f, coeffs[%d]: %s",
	   mus_name(ptr),
	   mus_frequency(ptr),
	   mus_phase(ptr),
	   gen->n,
	   str);
  free(str);
  return(describe_buffer);
}


static mus_float_t pw_set_index_and_func(mus_any *ptr, mus_float_t val) 
{
  pw *gen = (pw *)ptr;
  gen->index = val; 
  if (gen->cheby_choice == MUS_CHEBYSHEV_FIRST_KIND)
    gen->polyw = polyw_first;
  else gen->polyw = polyw_second;
  return(val);
}


static mus_any_class POLYWAVE_CLASS = {
  MUS_POLYWAVE,
  (char *)S_polywave,
  &free_pw,
  &describe_polywave,
  &pw_equalp,
  &pw_data,
  &pw_set_data,
  &pw_n,
  &pw_set_n,
  &pw_freq,
  &pw_set_freq,
  &pw_phase,
  &pw_set_phase,
  &pw_index, 
  &pw_set_index_and_func,
  &pw_increment,
  &pw_set_increment,
  &run_polywave,
  MUS_NOT_SPECIAL, 
  NULL, 0,
  0, 0, 0, 0,  
  &pw_xcoeff, &pw_set_xcoeff,
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0,
  &pw_choice,
  &pw_ycoeff, &pw_set_ycoeff,
  &pw_data, &pw_udata, 
  &pw_reset,
  0, &pw_copy
};


mus_any *mus_make_polywave(mus_float_t frequency, mus_float_t *coeffs, int n, int cheby_choice)
{
  pw *gen;
  gen = (pw *)malloc(sizeof(pw));
  gen->core = &POLYWAVE_CLASS;
  gen->phase = 0.0; /* cos used in cheby funcs above */
  gen->freq = mus_hz_to_radians(frequency);
  gen->coeffs = coeffs;
  gen->ucoeffs = NULL;
  gen->n = n;
  gen->index = 1.0;
  gen->cheby_choice = cheby_choice;
  if (cheby_choice != MUS_CHEBYSHEV_SECOND_KIND)
    {
      if (coeffs[0] == 0.0)
	{
	  /* these also ignore gen->index (assumed to be 1.0) (leaving aside the first_1 case)
	   *   pw_set_index_and_func protects against that case
	   */
	  if (n == 2)
	    {
	      gen->polyw = polyw_first_1;
	      gen->index = coeffs[1];
	    }
	  else
	    {
	      if (n == 3)
		gen->polyw = polyw_first_3;
	      else
		{
		  if (n == 4)
		    gen->polyw = polyw_first_4;
		  else
		    {
		      if (n == 5)
			gen->polyw = polyw_first_5;
		      else
			{
			  if (n == 6)
			    gen->polyw = polyw_first_6;
			  else 
			    {
			      if (n == 8)
				gen->polyw = polyw_first_8;
			      else 
				{
				  if (n == 11) /* a common case oddly enough */
				    gen->polyw = polyw_first_11;
				  else 
				    {
				      if (((n - 1) % 5) == 0)
					gen->polyw = polyw_f5;
				      else
					{
					  if (((n - 1) % 3) == 0)
					    gen->polyw = polyw_f3;
					  else
					    {
					      if (((n - 1) % 2) == 0)
						gen->polyw = polyw_f2;
					      else 
						{
						  /* lots of n=8 here */
						  gen->polyw = polyw_first;
						}
					    }
					}
				    }
				}
			    }
			}
		    }
		}
	    }
	}
      else 
	{
	  if (n == 2)
	    gen->polyw = polyw_f1;
	  else
	    {
	      if (((n - 1) % 3) == 0)
		gen->polyw = polyw_f3;
	      else
		{
		  if (((n - 1) % 2) == 0)
		    gen->polyw = polyw_f2;
		  else gen->polyw = polyw_first;
		}
	    }
	}
    }
  else 
    {
      if ((n == 5) && 
	  (coeffs[0] == 0.0))
	{
	  gen->polyw = polyw_second_5;
	  gen->index = coeffs[2] - coeffs[4];
	}
      else 
	{
	  if (n == 2)
	    gen->polyw = polyw_second_2;
	  else gen->polyw = polyw_second;
	}
    }
  return((mus_any *)gen);
}


mus_any *mus_make_polywave_tu(mus_float_t frequency, mus_float_t *tcoeffs, mus_float_t *ucoeffs, int n)
{
  pw *gen;
  gen = (pw *)malloc(sizeof(pw));
  gen->core = &POLYWAVE_CLASS;
  gen->phase = 0.0; /* cos used in cheby funcs above */
  gen->freq = mus_hz_to_radians(frequency);
  gen->coeffs = tcoeffs;
  gen->ucoeffs = ucoeffs;
  gen->n = n;
  gen->index = 1.0;
  gen->cheby_choice = MUS_CHEBYSHEV_BOTH_KINDS;
  gen->polyw = polyw_third;
  return((mus_any *)gen);
}


bool mus_is_polywave(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_POLYWAVE));
}



/* ---------------- polyshape ---------------- */

static char *describe_polyshape(mus_any *ptr)
{
  pw *gen = (pw *)ptr;
  char *str;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  str = float_array_to_string(gen->coeffs, gen->n, 0);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s freq: %.3fHz, phase: %.3f, coeffs[%d]: %s",
	       mus_name(ptr),
	       mus_frequency(ptr),
	       mus_phase(ptr),
	       gen->n,
	       str);
  free(str);
  return(describe_buffer);
}


mus_float_t mus_polyshape(mus_any *ptr, mus_float_t index, mus_float_t fm)
{
  pw *gen = (pw *)ptr;
  mus_float_t result;
  gen->index = index;
  result = mus_polynomial(gen->coeffs,
			  index * cos(gen->phase),
			  gen->n);

  if (gen->cheby_choice == MUS_CHEBYSHEV_SECOND_KIND)
    result *= sin(gen->phase);

  gen->phase += (gen->freq + fm);
  return(result);
}


mus_float_t mus_polyshape_unmodulated(mus_any *ptr, mus_float_t index)
{
  pw *gen = (pw *)ptr;
  mus_float_t result;
  gen->index = index;
  result = mus_polynomial(gen->coeffs,
			  index * cos(gen->phase),
			  gen->n);

  if (gen->cheby_choice == MUS_CHEBYSHEV_SECOND_KIND)
    result *= sin(gen->phase);

  gen->phase += gen->freq;
  return(result);
}


static mus_any_class POLYSHAPE_CLASS = {
  MUS_POLYSHAPE,
  (char *)S_polyshape,
  &free_pw,
  &describe_polyshape,
  &pw_equalp,
  &pw_data,
  &pw_set_data,
  &pw_n,
  &pw_set_n,
  &pw_freq,
  &pw_set_freq,
  &pw_phase,
  &pw_set_phase,
  &pw_index, &pw_set_index,
  &pw_increment,
  &pw_set_increment,
  &mus_polyshape,
  MUS_NOT_SPECIAL, 
  NULL, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 
  &pw_choice,
  0, 0, 0, 0,
  &pw_reset,
  0, &pw_copy
};


mus_any *mus_make_polyshape(mus_float_t frequency, mus_float_t phase, mus_float_t *coeffs, int size, int cheby_choice)
{
  mus_any *gen;
  gen = mus_make_polywave(frequency, coeffs, size, cheby_choice);
  gen->core = &POLYSHAPE_CLASS;
  pw_set_phase(gen, phase);
  return(gen);
}


bool mus_is_polyshape(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_POLYSHAPE));
}





/* ---------------- wave-train ---------------- */

typedef struct {
  mus_any_class *core;
  mus_float_t freq, phase;
  mus_float_t *wave;        /* passed in from caller */
  mus_long_t wave_size;
  mus_float_t *out_data;
  mus_long_t out_data_size;
  mus_interp_t interp_type; /* "type" field exists in core -- avoid confusion */
  mus_float_t next_wave_time;
  mus_long_t out_pos;
  bool first_time;
  mus_float_t yn1;
} wt;


static mus_float_t wt_freq(mus_any *ptr) {return(((wt *)ptr)->freq);}
static mus_float_t wt_set_freq(mus_any *ptr, mus_float_t val) {((wt *)ptr)->freq = val; return(val);}

static mus_float_t wt_phase(mus_any *ptr) {return(fmod(((TWO_PI * ((wt *)ptr)->phase) / ((mus_float_t)((wt *)ptr)->wave_size)), TWO_PI));}
static mus_float_t wt_set_phase(mus_any *ptr, mus_float_t val) {((wt *)ptr)->phase = (fmod(val, TWO_PI) * ((wt *)ptr)->wave_size) / TWO_PI; return(val);}

static mus_long_t wt_length(mus_any *ptr) {return(((wt *)ptr)->wave_size);}
static mus_long_t wt_set_length(mus_any *ptr, mus_long_t val) {if (val > 0) ((wt *)ptr)->wave_size = val; return(((wt *)ptr)->wave_size);}

static int wt_interp_type(mus_any *ptr) {return((int)(((wt *)ptr)->interp_type));}

static mus_float_t *wt_data(mus_any *ptr) {return(((wt *)ptr)->wave);}
static mus_float_t *wt_set_data(mus_any *ptr, mus_float_t *data) {((wt *)ptr)->wave = data; return(data);}

static mus_any *wt_copy(mus_any *ptr)
{
  wt *g, *p;
  p = (wt *)ptr;
  g = (wt *)malloc(sizeof(wt));
  memcpy((void *)g, (void *)ptr, sizeof(wt));
  g->out_data = (mus_float_t *)malloc(g->out_data_size * sizeof(mus_float_t));
  mus_copy_floats(g->out_data, p->out_data, g->out_data_size);
  /* g->wave is caller's data */
  return((mus_any *)g);
}

static bool wt_equalp(mus_any *p1, mus_any *p2)
{
  wt *w1 = (wt *)p1;
  wt *w2 = (wt *)p2;
  if (p1 == p2) return(true);
  return((w1) && (w2) &&
	 (w1->core->type == w2->core->type) &&
	 (w1->freq == w2->freq) &&
	 (w1->phase == w2->phase) &&
	 (w1->interp_type == w2->interp_type) &&
	 (w1->wave_size == w2->wave_size) &&
	 (w1->out_data_size == w2->out_data_size) &&
	 (w1->out_pos == w2->out_pos) &&
	 (clm_arrays_are_equal(w1->wave, w2->wave, w1->wave_size)) &&
	 (clm_arrays_are_equal(w1->out_data, w2->out_data, w1->out_data_size)));
}

static char *describe_wt(mus_any *ptr)
{
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s freq: %.3fHz, phase: %.3f, size: %" print_mus_long ", interp: %s",
	       mus_name(ptr),
	       mus_frequency(ptr), 
	       mus_phase(ptr), 
	       mus_length(ptr), 
	       mus_interp_type_to_string(wt_interp_type(ptr)));
  return(describe_buffer);
}


static mus_float_t mus_wave_train_any(mus_any *ptr, mus_float_t fm) 
{
  wt *gen = (wt *)ptr;
  mus_float_t result = 0.0;

  if (gen->out_pos < gen->out_data_size)
    result = gen->out_data[gen->out_pos];
  gen->out_pos++;
  if (gen->out_pos >= gen->next_wave_time)
    {
      mus_long_t i;
      mus_float_t *wave, *out_data; 
      mus_long_t wave_size;

      wave = gen->wave;
      wave_size = gen->wave_size;
      out_data = gen->out_data;

      if (gen->out_pos < gen->out_data_size)
	{
	  mus_long_t good_samps;
	  good_samps = gen->out_data_size - gen->out_pos;
	  memmove((void *)out_data, (void *)(out_data + gen->out_pos), good_samps * sizeof(mus_float_t));
	  mus_clear_floats(out_data + good_samps, gen->out_pos);
	}
      else mus_clear_floats(out_data, gen->out_data_size);
      if (gen->interp_type == MUS_INTERP_LINEAR)
	{
	  /* gen->phase doesn't change, and i is an int, so we can precalculate the fractional part, etc
	   */
	  mus_float_t phase, frac_part;
	  mus_long_t int_part;
	  
	  phase = gen->phase;
	  if ((phase < 0.0) || (phase > wave_size))
	    {
	      phase = fmod((mus_float_t)phase, (mus_float_t)wave_size);
	      if (phase < 0.0) phase += wave_size;
	    }

	  int_part = (mus_long_t)floor(phase);
	  frac_part = phase - int_part;
	  if (int_part == wave_size) int_part = 0;

	  if (frac_part == 0.0)
	    {
	      mus_long_t p;
	      for (i = 0, p = int_part; i < wave_size; i++, p++)
		{
		  if (p == wave_size) p = 0;
		  out_data[i] += wave[p];
		}
	    }
	  else
	    {
	      mus_long_t p, p1;
	      for (i = 0, p = int_part, p1 = int_part + 1; i < wave_size; i++, p1++)
		{
		  if (p1 == wave_size) p1 = 0;
		  out_data[i] += (wave[p] + frac_part * (wave[p1] - wave[p]));
		  p = p1;
		}
	    }
	}
      else
	{
	  for (i = 0; i < wave_size; i++)
	    {
	      gen->yn1 = mus_interpolate(gen->interp_type, gen->phase + i, wave, wave_size, gen->yn1);
	      out_data[i] += gen->yn1;
	    }
	}
      if (gen->first_time)
	{
	  gen->first_time = false;
	  gen->out_pos = (mus_long_t)(gen->phase); /* initial phase, but as an integer in terms of wave table size (gad...) */
	  if (gen->out_pos >= wave_size)
	    gen->out_pos = gen->out_pos % wave_size; /* both are mus_long_t */
	  result = out_data[gen->out_pos++];
	  if (gen->freq == -fm)
	    gen->next_wave_time = (mus_float_t)sampling_rate;
	  else gen->next_wave_time = ((mus_float_t)sampling_rate / (gen->freq + fm));
	}
      else 
	{
	  gen->next_wave_time += (((mus_float_t)sampling_rate / (gen->freq + fm)) - gen->out_pos);
	  gen->out_pos = 0;
	}
    }
  return(result);
}


mus_float_t mus_wave_train(mus_any *ptr, mus_float_t fm) {return(mus_wave_train_any(ptr, fm / w_rate));}

mus_float_t mus_wave_train_unmodulated(mus_any *ptr) {return(mus_wave_train(ptr, 0.0));}

static mus_float_t run_wave_train(mus_any *ptr, mus_float_t fm, mus_float_t unused) {return(mus_wave_train_any(ptr, fm / w_rate));}

static void free_wt(mus_any *p) 
{
  wt *ptr = (wt *)p;
  if (ptr->out_data)
    {
      free(ptr->out_data); 
      ptr->out_data = NULL;
    }
  free(ptr);
}


static void wt_reset(mus_any *ptr)
{
  wt *gen = (wt *)ptr;
  gen->phase = 0.0;
  mus_clear_floats(gen->out_data, gen->out_data_size);
  gen->out_pos = gen->out_data_size;
  gen->next_wave_time = 0.0;
  gen->first_time = true;
}


static mus_any_class WAVE_TRAIN_CLASS = {
  MUS_WAVE_TRAIN,
  (char *)S_wave_train,
  &free_wt,
  &describe_wt,
  &wt_equalp,
  &wt_data,
  &wt_set_data,
  &wt_length,
  &wt_set_length,
  &wt_freq,
  &wt_set_freq,
  &wt_phase,
  &wt_set_phase,
  &fallback_scaler, 0,
  0, 0,
  &run_wave_train,
  MUS_NOT_SPECIAL, 
  NULL,
  &wt_interp_type,
  0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &wt_reset,
  0, &wt_copy
};


mus_any *mus_make_wave_train(mus_float_t freq, mus_float_t phase, mus_float_t *wave, mus_long_t wave_size, mus_interp_t type)
{
  wt *gen;
  gen = (wt *)malloc(sizeof(wt));
  gen->core = &WAVE_TRAIN_CLASS;
  gen->freq = freq;
  gen->phase = (wave_size * fmod(phase, TWO_PI)) / TWO_PI;
  gen->wave = wave;
  gen->wave_size = wave_size;
  gen->interp_type = type;
  gen->out_data_size = wave_size + 2;
  gen->out_data = (mus_float_t *)calloc(gen->out_data_size, sizeof(mus_float_t));
  gen->out_pos = gen->out_data_size;
  gen->next_wave_time = 0.0;
  gen->first_time = true;
  gen->yn1 = 0.0;
  return((mus_any *)gen);
}


bool mus_is_wave_train(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_WAVE_TRAIN));
}




/* ---------------- delay, comb, notch, all-pass, moving-average, filtered-comb ---------------- */

typedef struct dly {
  mus_any_class *core;
  uint32_t loc, size;
  bool zdly, line_allocated, filt_allocated;
  mus_float_t *line;
  uint32_t zloc, zsize;
  mus_float_t xscl, yscl, yn1, y1, norm;
  mus_interp_t type;
  mus_any *filt;
  struct dly *next;
  mus_float_t (*runf)(mus_any *gen, mus_float_t arg1, mus_float_t arg2);
  mus_float_t (*del)(mus_any *ptr, mus_float_t input);                 /* zdelay or normal tap */
  mus_float_t (*delt)(mus_any *ptr, mus_float_t input);                /*   just tick */
  mus_float_t (*delu)(mus_any *ptr, mus_float_t input);                /*   unmodulated */
} dly;



mus_float_t mus_delay_tick(mus_any *ptr, mus_float_t input)
{
  return(((dly *)ptr)->delt(ptr, input));
}


mus_float_t mus_tap(mus_any *ptr, mus_float_t loc)
{
  return(((dly *)ptr)->del(ptr, loc));
}


mus_float_t mus_delay_unmodulated(mus_any *ptr, mus_float_t input)
{
  return(((dly *)ptr)->delu(ptr, input));
}


static mus_float_t ztap(mus_any *ptr, mus_float_t loc)
{
  dly *gen = (dly *)ptr;
  /* this is almost always linear */
  if (gen->type == MUS_INTERP_LINEAR)
    return(mus_array_interp(gen->line, gen->zloc - loc, gen->zsize));
  gen->yn1 = mus_interpolate(gen->type, gen->zloc - loc, gen->line, gen->zsize, gen->yn1);
  return(gen->yn1);
}


static mus_float_t dtap(mus_any *ptr, mus_float_t loc)
{
  dly *gen = (dly *)ptr;
  int taploc;
  if (gen->size == 0) return(gen->line[0]);
  if ((int)loc == 0) return(gen->line[gen->loc]);
  taploc = (int)(gen->loc - (int)loc) % (int)gen->size;
  /* cast to int for gen->size is needed, as Tito Latini noticed, because the % operator in C is not smart about uint32_ts:
   *    (int)-1 % (uint32_t)10    => 5
   *    (int)-1 % (int)10             => -1
   */
  if (taploc < 0) taploc += (int)gen->size;
  return(gen->line[taploc]);
}


mus_float_t mus_tap_unmodulated(mus_any *ptr)
{
  dly *gen = (dly *)ptr;
  return(gen->line[gen->loc]);
}


static mus_float_t zdelt(mus_any *ptr, mus_float_t input)
{
  dly *gen = (dly *)ptr;
  gen->line[gen->loc] = input;
  gen->loc++;
  if (gen->loc >= gen->zsize) gen->loc = 0;
  gen->zloc++;
  if (gen->zloc >= gen->zsize) gen->zloc = 0;
  return(input);
}


static mus_float_t delt(mus_any *ptr, mus_float_t input)
{
  dly *gen = (dly *)ptr;
  gen->line[gen->loc] = input;
  gen->loc++;
  if (gen->loc >= gen->size) gen->loc = 0;
  return(input);
}


mus_float_t mus_delay(mus_any *ptr, mus_float_t input, mus_float_t pm)
{
  mus_float_t result;
  dly *gen = (dly *)ptr;
  if ((gen->size == 0) && (pm < 1.0))
    result = pm * gen->line[0] + (1.0 - pm) * input;
  else result = mus_tap(ptr, pm);
  mus_delay_tick(ptr, input);
  return(result);
}


static mus_float_t zdelay_unmodulated(mus_any *ptr, mus_float_t input)
{
  dly *gen = (dly *)ptr;
  mus_float_t result;
  result = gen->line[gen->zloc];
  mus_delay_tick(ptr, input);
  return(result);
}


static mus_float_t delay_unmodulated_zero(mus_any *ptr, mus_float_t input)
{
  return(input);
}


mus_float_t mus_delay_unmodulated_noz(mus_any *ptr, mus_float_t input)
{
  dly *gen = (dly *)ptr;
  mus_float_t result;
  result = gen->line[gen->loc];
  gen->line[gen->loc] = input;
  gen->loc++;
  if (gen->loc >= gen->size) 
    gen->loc = 0;
  return(result);
}

static dly *dly_free_list = NULL;

static void free_delay(mus_any *gen) 
{
  dly *ptr = (dly *)gen;
  if ((ptr->line) && (ptr->line_allocated)) free(ptr->line);
  if ((ptr->filt) && (ptr->filt_allocated)) mus_free(ptr->filt);
  /* free(ptr); */
  ptr->next = dly_free_list;
  dly_free_list = ptr;
}


static mus_any *dly_copy(mus_any *ptr)
{
  dly *g, *p;
  p = (dly *)ptr;
  if (dly_free_list)
    {
      g = dly_free_list;
      dly_free_list = g->next;
    }
  else g = (dly *)malloc(sizeof(dly));
  memcpy((void *)g, (void *)ptr, sizeof(dly));

  g->line = (mus_float_t *)malloc(g->size * sizeof(mus_float_t));
  mus_copy_floats(g->line, p->line, g->size);
  g->line_allocated = true;

  if (p->filt)
    {
      g->filt = mus_copy(p->filt);
      g->filt_allocated = true;
    }
  return((mus_any *)g);
}


static char *describe_delay(mus_any *ptr)
{
  char *str = NULL;
  dly *gen = (dly *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  if (gen->zdly)
    snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s line[%u,%u, %s]: %s", 
		 mus_name(ptr),
		 gen->size, 
		 gen->zsize, 
		 mus_interp_type_to_string(gen->type),
		 str = float_array_to_string(gen->line, gen->size, gen->zloc));
  else snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s line[%u, %s]: %s", 
		    mus_name(ptr),
		    gen->size, 
		    mus_interp_type_to_string(gen->type), 
		    str = float_array_to_string(gen->line, gen->size, gen->loc));
  if (str) free(str);
  return(describe_buffer);
}


static bool delay_equalp(mus_any *p1, mus_any *p2)
{
  dly *d1 = (dly *)p1;
  dly *d2 = (dly *)p2;
  if (p1 == p2) return(true);
  return((d1) && (d2) &&
	 (d1->core->type == d2->core->type) &&
	 (d1->size == d2->size) &&
	 (d1->loc == d2->loc) &&
	 (d1->zdly == d2->zdly) &&
	 (d1->zloc == d2->zloc) &&
	 (d1->zsize == d2->zsize) &&
	 (d1->xscl == d2->xscl) &&
	 (d1->yscl == d2->yscl) &&
	 (d1->yn1 == d2->yn1) &&
	 (d1->type == d2->type) &&
	 (clm_arrays_are_equal(d1->line, d2->line, d1->size)));
}


static mus_long_t delay_length(mus_any *ptr) 
{
  dly *d = (dly *)ptr;
  if (d->size > 0) /* this is possible (not sure it's a good idea...) */
    return(d->size);
  return(d->zsize); /* maybe always use this? */
}


static mus_float_t delay_scaler(mus_any *ptr) {return(((dly *)ptr)->xscl);}
static mus_float_t delay_set_scaler(mus_any *ptr, mus_float_t val) {((dly *)ptr)->xscl = val; return(val);}

static mus_float_t delay_fb(mus_any *ptr) {return(((dly *)ptr)->yscl);}
static mus_float_t delay_set_fb(mus_any *ptr, mus_float_t val) {((dly *)ptr)->yscl = val; return(val);}

static int delay_interp_type(mus_any *ptr) {return((int)(((dly *)ptr)->type));}
static mus_long_t delay_loc(mus_any *ptr){return((mus_long_t)(((dly *)ptr)->loc));}
static mus_float_t *delay_data(mus_any *ptr) {return(((dly *)ptr)->line);}
static mus_float_t *delay_set_data(mus_any *ptr, mus_float_t *val) 
{
  dly *gen = (dly *)ptr;
  if (gen->line_allocated) {free(gen->line); gen->line_allocated = false;}
  gen->line = val; 
  return(val);
}


static mus_long_t delay_set_length(mus_any *ptr, mus_long_t val) 
{
  dly *gen = (dly *)ptr;  
  if (val > 0) 
    {
      uint32_t old_size;
      old_size = gen->size;
      gen->size = (uint32_t)val; 
      if (gen->size < old_size)
	{
	  if (gen->loc > gen->size) gen->loc = 0;
	  gen->zdly = false; /* otherwise too many ways to screw up */
	}
    }
  return((mus_long_t)(gen->size));
}


bool mus_is_tap(mus_any *gen)
{
  return((gen) && 
	 (gen->core->extended_type == MUS_DELAY_LINE));
}


static void delay_reset(mus_any *ptr)
{
  dly *gen = (dly *)ptr;
  gen->loc = 0;
  gen->zloc = 0;
  gen->yn1 = 0.0;
  mus_clear_floats(gen->line, gen->zsize);
}


static mus_any_class DELAY_CLASS = {
  MUS_DELAY,
  (char *)S_delay,
  &free_delay,
  &describe_delay,
  &delay_equalp,
  &delay_data,
  &delay_set_data,
  &delay_length,
  &delay_set_length,
  0, 0, 0, 0, /* freq phase */
  &delay_scaler,
  &delay_set_scaler,
  &delay_fb,
  &delay_set_fb,
  &mus_delay,
  MUS_DELAY_LINE,
  NULL, 
  &delay_interp_type,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &delay_loc,
  0, 0,
  0, 0, 0, 0,
  &delay_reset,
  0, &dly_copy
};


mus_any *mus_make_delay(int size, mus_float_t *preloaded_line, int line_size, mus_interp_t type) 
{
  /* if preloaded_line null, allocated locally.
   * if size == line_size, normal (non-interpolating) delay
   *    in clm2xen.c, if size=0 and max-size unset, max-size=1 (line_size here)
   */
  dly *gen;
  if (dly_free_list)
    {
      gen = dly_free_list;
      dly_free_list = gen->next;
    }
  else gen = (dly *)malloc(sizeof(dly));
  gen->core = &DELAY_CLASS;
  gen->loc = 0;
  if (line_size < size) line_size = size;
  gen->size = size;
  gen->zsize = line_size;
  gen->zdly = ((line_size != size) || (type != MUS_INTERP_NONE));
  if (gen->zdly)
    {
      gen->del = ztap;
      gen->delt = zdelt;
      if (gen->size == 0)
	gen->delu = delay_unmodulated_zero;
      else gen->delu = zdelay_unmodulated;
    }
  else
    {
      gen->del = dtap;
      gen->delt = delt;
      if (gen->size == 0)
	gen->delu = delay_unmodulated_zero;
      else gen->delu = mus_delay_unmodulated_noz;
    }
  gen->type = type;
  if (preloaded_line)
    {
      gen->line = preloaded_line;
      gen->line_allocated = false;
    }
  else 
    {
      gen->line = (mus_float_t *)calloc((line_size <= 0) ? 1 : line_size, sizeof(mus_float_t));
      gen->line_allocated = true;
    }
  gen->zloc = line_size - size;
  gen->filt = NULL;
  gen->filt_allocated = false;
  gen->xscl = 0.0;
  gen->yscl = 0.0;
  gen->yn1 = 0.0;
  gen->runf = NULL;
  return((mus_any *)gen);
}


bool mus_is_delay(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_DELAY));
}


/* ---------------- comb ---------------- */

mus_float_t mus_comb(mus_any *ptr, mus_float_t input, mus_float_t pm) 
{
  dly *gen = (dly *)ptr;
  if (gen->zdly)
    return(mus_delay(ptr, input + (gen->yscl * mus_tap(ptr, pm)), pm)); 
  /* mus.lisp has 0 in place of the final pm -- the question is whether the delay
     should interpolate as well as the tap.  There is a subtle difference in
     output (the pm case is low-passed by the interpolation ("average")),
     but I don't know if there's a standard here, or what people expect.
     We're doing the outer-level interpolation in notch and all-pass.
     Should mus.lisp be changed?
  */
  else return(mus_delay_unmodulated(ptr, input + (gen->line[gen->loc] * gen->yscl)));
}


mus_float_t mus_comb_unmodulated(mus_any *ptr, mus_float_t input) 
{
  dly *gen = (dly *)ptr;
  if (gen->zdly) 
    return(mus_delay_unmodulated(ptr, input + (gen->line[gen->zloc] * gen->yscl)));
  return(mus_delay_unmodulated(ptr, input + (gen->line[gen->loc] * gen->yscl)));
}


mus_float_t mus_comb_unmodulated_noz(mus_any *ptr, mus_float_t input) 
{
  dly *gen = (dly *)ptr;
  mus_float_t result;

  result = gen->line[gen->loc];
  gen->line[gen->loc] = input + (result * gen->yscl);
  gen->loc++;
  if (gen->loc >= gen->size) 
    gen->loc = 0;

  return(result);
}


static char *describe_comb(mus_any *ptr)
{
  char *str = NULL;
  dly *gen = (dly *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  if (gen->zdly)
    snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s scaler: %.3f, line[%u,%u, %s]: %s", 
		 mus_name(ptr),
		 gen->yscl, 
		 gen->size, 
		 gen->zsize, 
		 mus_interp_type_to_string(gen->type),
		 str = float_array_to_string(gen->line, gen->size, gen->zloc));
  else snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s scaler: %.3f, line[%u, %s]: %s", 
		    mus_name(ptr),
		    gen->yscl, 
		    gen->size, 
		    mus_interp_type_to_string(gen->type),
		    str = float_array_to_string(gen->line, gen->size, gen->loc));
  if (str) free(str);
  return(describe_buffer);
}


static mus_any_class COMB_CLASS = {
  MUS_COMB,
  (char *)S_comb,
  &free_delay,
  &describe_comb,
  &delay_equalp,
  &delay_data,
  &delay_set_data,
  &delay_length,
  &delay_set_length,
  0, 0, 0, 0, /* freq phase */
  &delay_scaler,
  &delay_set_scaler,
  &delay_fb,
  &delay_set_fb,
  &mus_comb,
  MUS_DELAY_LINE,
  NULL,
  &delay_interp_type,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &delay_loc,
  0, 0,
  0, 0, 0, 0,
  &delay_reset,
  0, &dly_copy
};


mus_any *mus_make_comb(mus_float_t scaler, int size, mus_float_t *line, int line_size, mus_interp_t type)
{
  dly *gen;
  gen = (dly *)mus_make_delay(size, line, line_size, type);
  if (gen)
    {
      gen->core = &COMB_CLASS;
      gen->yscl = scaler;
      return((mus_any *)gen);
    }
  return(NULL);
}


bool mus_is_comb(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_COMB));
}



/* ---------------- comb-bank ---------------- */

typedef struct {
  mus_any_class *core;
  int size;
  mus_any **gens;
  mus_float_t (*cmbf)(mus_any *ptr, mus_float_t input);
} cmb_bank;


static void free_comb_bank(mus_any *ptr) 
{
  cmb_bank *f = (cmb_bank *)ptr;
  if (f->gens) {free(f->gens); f->gens = NULL;}
  free(ptr); 
}


static mus_any *cmb_bank_copy(mus_any *ptr)
{
  cmb_bank *g, *p;
  int i;

  p = (cmb_bank *)ptr;
  g = (cmb_bank *)malloc(sizeof(cmb_bank));
  memcpy((void *)g, (void *)ptr, sizeof(cmb_bank));
  g->gens = (mus_any **)malloc(p->size * sizeof(mus_any *));
  for (i = 0; i < p->size; i++)
    g->gens[i] = mus_copy(p->gens[i]);

  return((mus_any *)g);
}


static mus_float_t run_comb_bank(mus_any *ptr, mus_float_t input, mus_float_t unused) 
{
  return(mus_comb_bank(ptr, input));
}


static mus_long_t comb_bank_length(mus_any *ptr)
{
  return(((cmb_bank *)ptr)->size);
}


static void comb_bank_reset(mus_any *ptr)
{
  cmb_bank *f = (cmb_bank *)ptr;
  int i;
  for (i = 0; i < f->size; i++)
    mus_reset(f->gens[i]);
}


static bool comb_bank_equalp(mus_any *p1, mus_any *p2)
{
  cmb_bank *f1 = (cmb_bank *)p1;
  cmb_bank *f2 = (cmb_bank *)p2;
  int i, size;

  if (f1 == f2) return(true);
  if (f1->size != f2->size) return(false);
  size = f1->size;

  for (i = 0; i < size; i++)
    if (!delay_equalp(f1->gens[i], f2->gens[i]))
      return(false);
  
  /* now check the locals... */
  return(true);
}


static char *describe_comb_bank(mus_any *ptr)
{
  cmb_bank *gen = (cmb_bank *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s size: %d",
	       mus_name(ptr),
	       gen->size);
  return(describe_buffer);
}

static mus_any_class COMB_BANK_CLASS = {
  MUS_COMB_BANK,
  (char *)S_comb_bank,
  &free_comb_bank,
  &describe_comb_bank,
  &comb_bank_equalp,
  0, 0,
  &comb_bank_length, 0,
  0, 0, 
  0, 0,
  0, 0,
  0, 0,
  &run_comb_bank,
  MUS_NOT_SPECIAL, 
  NULL, 0,
  0, 0, 0, 0,
  0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &comb_bank_reset,
  0, &cmb_bank_copy
};


static mus_float_t comb_bank_any(mus_any *combs, mus_float_t inval)
{
  int i;
  mus_float_t sum = 0.0;
  cmb_bank *c = (cmb_bank *)combs;
  for (i = 0; i < c->size; i++) 
    sum += mus_comb_unmodulated_noz(c->gens[i], inval);
  return(sum);
}

static mus_float_t comb_bank_4(mus_any *combs, mus_float_t inval)
{
  cmb_bank *c = (cmb_bank *)combs;
  mus_any **gs;
  gs = c->gens;
  return(mus_comb_unmodulated_noz(gs[0], inval) +
	 mus_comb_unmodulated_noz(gs[1], inval) +
	 mus_comb_unmodulated_noz(gs[2], inval) +
	 mus_comb_unmodulated_noz(gs[3], inval));
}

static mus_float_t comb_bank_6(mus_any *combs, mus_float_t inval)
{
  cmb_bank *c = (cmb_bank *)combs;
  mus_any **gs;
  gs = c->gens;
  return(mus_comb_unmodulated_noz(gs[0], inval) +
	 mus_comb_unmodulated_noz(gs[1], inval) +
	 mus_comb_unmodulated_noz(gs[2], inval) +
	 mus_comb_unmodulated_noz(gs[3], inval) +
	 mus_comb_unmodulated_noz(gs[4], inval) +
	 mus_comb_unmodulated_noz(gs[5], inval));
}


mus_any *mus_make_comb_bank(int size, mus_any **combs)
{
  cmb_bank *gen;
  int i;

  gen = (cmb_bank *)malloc(sizeof(cmb_bank));
  gen->core = &COMB_BANK_CLASS;
  gen->size = size;

  gen->gens = (mus_any **)malloc(size * sizeof(mus_any *));
  for (i = 0; i < size; i++)
    gen->gens[i] = combs[i];

  if (size == 4)
    gen->cmbf = comb_bank_4;
  else
    {
      if (size == 6)
	gen->cmbf = comb_bank_6;
      else gen->cmbf = comb_bank_any;
    }

  return((mus_any *)gen);
}

bool mus_is_comb_bank(mus_any *ptr)
{
  return((ptr) && 
	 (ptr->core->type == MUS_COMB_BANK));
}

mus_float_t mus_comb_bank(mus_any *combs, mus_float_t inval)
{
  cmb_bank *gen = (cmb_bank *)combs;
  return((gen->cmbf)(combs, inval));
}




/* ---------------- notch ---------------- */

static char *describe_notch(mus_any *ptr)
{
  char *str = NULL;
  dly *gen = (dly *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  if (gen->zdly)
    snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s scaler: %.3f, line[%u,%u, %s]: %s", 
		 mus_name(ptr),
		 gen->xscl, 
		 gen->size, 
		 gen->zsize, 
		 mus_interp_type_to_string(gen->type),
		 str = float_array_to_string(gen->line, gen->size, gen->zloc));
  else snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s scaler: %.3f, line[%u, %s]: %s", 
		    mus_name(ptr),
		    gen->xscl, 
		    gen->size, 
		    mus_interp_type_to_string(gen->type),
		    str = float_array_to_string(gen->line, gen->size, gen->loc));
  if (str) free(str);
  return(describe_buffer);
}


static mus_any_class NOTCH_CLASS = {
  MUS_NOTCH,
  (char *)S_notch,
  &free_delay,
  &describe_notch,
  &delay_equalp,
  &delay_data,
  &delay_set_data,
  &delay_length,
  &delay_set_length,
  0, 0, 0, 0, /* freq phase */
  &delay_scaler,
  &delay_set_scaler,
  0, 0,
  &mus_notch,
  MUS_DELAY_LINE,
  NULL,
  &delay_interp_type,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &delay_loc,
  0, 0,
  0, 0, 0, 0,
  &delay_reset,
  0, &dly_copy
};


mus_float_t mus_notch(mus_any *ptr, mus_float_t input, mus_float_t pm) 
{
  dly *gen = (dly *)ptr;
  return((input * gen->xscl) + mus_delay(ptr, input, pm));
}


mus_float_t mus_notch_unmodulated(mus_any *ptr, mus_float_t input) 
{
  return((input * ((dly *)ptr)->xscl) + mus_delay_unmodulated(ptr, input));
}

#if 0
static mus_float_t mus_notch_unmodulated_noz(mus_any *ptr, mus_float_t input) 
{
  dly *gen = (dly *)ptr;
  mus_float_t result;
  result = gen->line[gen->loc] + (input * gen->xscl);
  gen->line[gen->loc] = input;
  gen->loc++;
  if (gen->loc >= gen->size) 
    gen->loc = 0;
  return(result);
}
#endif

bool mus_is_notch(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_NOTCH));
}


mus_any *mus_make_notch(mus_float_t scaler, int size, mus_float_t *line, int line_size, mus_interp_t type)
{
  dly *gen;
  gen = (dly *)mus_make_delay(size, line, line_size, type);
  if (gen)
    {
      gen->core = &NOTCH_CLASS;
      gen->xscl = scaler;
      return((mus_any *)gen);
    }
  return(NULL);
}


mus_float_t mus_all_pass(mus_any *ptr, mus_float_t input, mus_float_t pm)
{
  mus_float_t din;
  dly *gen = (dly *)ptr;
  if (gen->zdly)
    din = input + (gen->yscl * mus_tap(ptr, pm));
  else din = input + (gen->yscl * gen->line[gen->loc]);
  return(mus_delay(ptr, din, pm) + (gen->xscl * din));
}


mus_float_t mus_all_pass_unmodulated(mus_any *ptr, mus_float_t input)
{
  mus_float_t din;
  dly *gen = (dly *)ptr;
  if (gen->zdly)
    din = input + (gen->yscl * gen->line[gen->zloc]);
  else din = input + (gen->yscl * gen->line[gen->loc]);
  return(mus_delay_unmodulated(ptr, din) + (gen->xscl * din));
}


mus_float_t mus_all_pass_unmodulated_noz(mus_any *ptr, mus_float_t input)
{
  mus_float_t result, din;
  dly *gen = (dly *)ptr;
  uint32_t loc;
  loc = gen->loc++;
  din = input + (gen->yscl * gen->line[loc]);
  result = gen->line[loc] + (gen->xscl * din);
  gen->line[loc] = din;
  if (gen->loc >= gen->size)
    gen->loc = 0;
  return(result);
}


bool mus_is_all_pass(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_ALL_PASS));
}


static char *describe_all_pass(mus_any *ptr)
{
  char *str = NULL;
  dly *gen = (dly *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  if (gen->zdly)
    snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s feedback: %.3f, feedforward: %.3f, line[%u,%u, %s]:%s",
		 mus_name(ptr),
		 gen->yscl, 
		 gen->xscl, 
		 gen->size, 
		 gen->zsize, 
		 mus_interp_type_to_string(gen->type),
		 str = float_array_to_string(gen->line, gen->size, gen->zloc));
  else snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s feedback: %.3f, feedforward: %.3f, line[%u, %s]:%s",
		    mus_name(ptr),
		    gen->yscl, 
		    gen->xscl, 
		    gen->size, 
		    mus_interp_type_to_string(gen->type),
		    str = float_array_to_string(gen->line, gen->size, gen->loc));
  if (str) free(str);
  return(describe_buffer);
}


static mus_any_class ALL_PASS_CLASS = {
  MUS_ALL_PASS,
  (char *)S_all_pass,
  &free_delay,
  &describe_all_pass,
  &delay_equalp,
  &delay_data,
  &delay_set_data,
  &delay_length,
  &delay_set_length,
  0, 0, 0, 0, /* freq phase */
  &delay_scaler,
  &delay_set_scaler,
  &delay_fb,
  &delay_set_fb,
  &mus_all_pass,
  MUS_DELAY_LINE,
  NULL,
  &delay_interp_type,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &delay_loc,
  0, 0,
  0, 0, 0, 0,
  &delay_reset,
  0, &dly_copy
};


mus_any *mus_make_all_pass(mus_float_t backward, mus_float_t forward, int size, mus_float_t *line, int line_size, mus_interp_t type)
{
  dly *gen;
  gen = (dly *)mus_make_delay(size, line, line_size, type);
  if (gen)
    {
      gen->core = &ALL_PASS_CLASS;
      gen->xscl = forward;
      gen->yscl = backward;
      return((mus_any *)gen);
    }
  return(NULL);
}


/* ---------------- all_pass-bank ---------------- */

typedef struct {
  mus_any_class *core;
  int size;
  mus_any **gens;
  mus_float_t (*apf)(mus_any *ptr, mus_float_t input);
} allp_bank;


static void free_all_pass_bank(mus_any *ptr) 
{
  allp_bank *f = (allp_bank *)ptr;
  if (f->gens) {free(f->gens); f->gens = NULL;}
  free(ptr); 
}


static mus_any *allp_bank_copy(mus_any *ptr)
{
  allp_bank *g, *p;
  int i;

  p = (allp_bank *)ptr;
  g = (allp_bank *)malloc(sizeof(allp_bank));
  memcpy((void *)g, (void *)ptr, sizeof(allp_bank));
  g->gens = (mus_any **)malloc(p->size * sizeof(mus_any *));
  for (i = 0; i < p->size; i++)
    g->gens[i] = mus_copy(p->gens[i]);

  return((mus_any *)g);
}


static mus_float_t run_all_pass_bank(mus_any *ptr, mus_float_t input, mus_float_t unused) 
{
  return(mus_all_pass_bank(ptr, input));
}


static mus_long_t all_pass_bank_length(mus_any *ptr)
{
  return(((allp_bank *)ptr)->size);
}


static void all_pass_bank_reset(mus_any *ptr)
{
  allp_bank *f = (allp_bank *)ptr;
  int i;
  for (i = 0; i < f->size; i++)
    mus_reset(f->gens[i]);
}


static bool all_pass_bank_equalp(mus_any *p1, mus_any *p2)
{
  allp_bank *f1 = (allp_bank *)p1;
  allp_bank *f2 = (allp_bank *)p2;
  int i, size;

  if (f1 == f2) return(true);
  if (f1->size != f2->size) return(false);
  size = f1->size;

  for (i = 0; i < size; i++)
    if (!delay_equalp(f1->gens[i], f2->gens[i]))
      return(false);

  return(true);
}


static char *describe_all_pass_bank(mus_any *ptr)
{
  allp_bank *gen = (allp_bank *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s size: %d",
	       mus_name(ptr),
	       gen->size);
  return(describe_buffer);
}

static mus_any_class ALL_PASS_BANK_CLASS = {
  MUS_ALL_PASS_BANK,
  (char *)S_all_pass_bank,
  &free_all_pass_bank,
  &describe_all_pass_bank,
  &all_pass_bank_equalp,
  0, 0,
  &all_pass_bank_length, 0,
  0, 0, 
  0, 0,
  0, 0,
  0, 0,
  &run_all_pass_bank,
  MUS_NOT_SPECIAL, 
  NULL, 0,
  0, 0, 0, 0,
  0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &all_pass_bank_reset,
  0, &allp_bank_copy
};


static mus_float_t all_pass_bank_3(mus_any *all_passes, mus_float_t inval)
{
  allp_bank *c = (allp_bank *)all_passes;
  mus_any **gs;
  gs = c->gens;
  return(mus_all_pass_unmodulated_noz(gs[2], mus_all_pass_unmodulated_noz(gs[1], mus_all_pass_unmodulated_noz(gs[0], inval))));
}

static mus_float_t all_pass_bank_4(mus_any *all_passes, mus_float_t inval)
{
  allp_bank *c = (allp_bank *)all_passes;
  mus_any **gs;
  gs = c->gens;
  return(mus_all_pass_unmodulated_noz(gs[3], mus_all_pass_unmodulated_noz(gs[2], mus_all_pass_unmodulated_noz(gs[1], mus_all_pass_unmodulated_noz(gs[0], inval)))));
}

static mus_float_t all_pass_bank_any(mus_any *all_passs, mus_float_t inval)
{
  int i;
  mus_float_t sum = inval;
  allp_bank *c = (allp_bank *)all_passs;
  for (i = 0; i < c->size; i++) 
    sum = mus_all_pass_unmodulated_noz(c->gens[i], sum);
  return(sum);
}


mus_any *mus_make_all_pass_bank(int size, mus_any **all_passs)
{
  allp_bank *gen;
  int i;

  gen = (allp_bank *)malloc(sizeof(allp_bank));
  gen->core = &ALL_PASS_BANK_CLASS;
  gen->size = size;

  gen->gens = (mus_any **)malloc(size * sizeof(mus_any *));
  for (i = 0; i < size; i++)
    gen->gens[i] = all_passs[i];

  if (size == 3)
    gen->apf = all_pass_bank_3;
  else
    {
      if (size == 4)
	gen->apf = all_pass_bank_4;
      else gen->apf = all_pass_bank_any;
    }

  return((mus_any *)gen);
}


bool mus_is_all_pass_bank(mus_any *ptr)
{
  return((ptr) && 
	 (ptr->core->type == MUS_ALL_PASS_BANK));
}


mus_float_t mus_all_pass_bank(mus_any *all_passes, mus_float_t inval)
{
  allp_bank *gen = (allp_bank *)all_passes;
  return((gen->apf)(all_passes, inval));
}




/* ---------------- moving-average ---------------- */

bool mus_is_moving_average(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_MOVING_AVERAGE));
}


mus_float_t mus_moving_average(mus_any *ptr, mus_float_t input)
{
  dly *gen = (dly *)ptr;
  mus_float_t output;
  output = mus_delay_unmodulated_noz(ptr, input);
  gen->xscl += (input - output);
  return(gen->xscl * gen->yscl); /* xscl=sum, yscl=1/n */
}


static mus_float_t run_mus_moving_average(mus_any *ptr, mus_float_t input, mus_float_t unused) {return(mus_moving_average(ptr, input));}


static void moving_average_reset(mus_any *ptr)
{
  dly *gen = (dly *)ptr;
  delay_reset(ptr);
  gen->xscl = 0.0;
}


static char *describe_moving_average(mus_any *ptr)
{
  char *str = NULL;
  dly *gen = (dly *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s %.3f, line[%u]:%s",
	       mus_name(ptr),
	       gen->xscl * gen->yscl, 
	       gen->size, 
	       str = float_array_to_string(gen->line, gen->size, gen->loc));
  if (str) free(str);
  return(describe_buffer);
}


static mus_any_class MOVING_AVERAGE_CLASS = {
  MUS_MOVING_AVERAGE,
  (char *)S_moving_average,
  &free_delay,
  &describe_moving_average,
  &delay_equalp,
  &delay_data,
  &delay_set_data,
  &delay_length,
  &delay_set_length,
  0, 0, 0, 0, /* freq phase */
  &delay_scaler,
  &delay_set_scaler,
  &delay_fb,
  &delay_set_fb,
  &run_mus_moving_average,
  MUS_DELAY_LINE,
  NULL, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &delay_loc,
  0, 0,
  0, 0, 0, 0,
  &moving_average_reset,
  0, &dly_copy
};


mus_any *mus_make_moving_average(int size, mus_float_t *line)
{
  dly *gen;
  gen = (dly *)mus_make_delay(size, line, size, MUS_INTERP_NONE);
  if (gen)
    {
      int i;
      gen->core = &MOVING_AVERAGE_CLASS;
      gen->xscl = 0.0;
      for (i = 0; i < size; i++) 
	gen->xscl += gen->line[i];
      gen->yscl = 1.0 / (mus_float_t)size;
      return((mus_any *)gen);
    }
  return(NULL);
}

mus_any *mus_make_moving_average_with_initial_sum(int size, mus_float_t *line, mus_float_t sum)
{
  dly *gen;
  gen = (dly *)mus_make_delay(size, line, size, MUS_INTERP_NONE);
  if (gen)
    {
      gen->core = &MOVING_AVERAGE_CLASS;
      gen->xscl = sum;
      gen->yscl = 1.0 / (mus_float_t)size;
      return((mus_any *)gen);
    }
  return(NULL);
}


/* -------- moving-max -------- */

bool mus_is_moving_max(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_MOVING_MAX));
}


mus_float_t mus_moving_max(mus_any *ptr, mus_float_t input)
{
  dly *gen = (dly *)ptr;
  mus_float_t output, abs_input;

  abs_input = fabs(input);
  output = mus_delay_unmodulated_noz(ptr, abs_input);
  if (abs_input >= gen->xscl)
    gen->xscl = abs_input;
  else
    {
      if (output >= gen->xscl)
	{
	  uint32_t i;
	  for (i = 0; i < gen->size; i++)
	    if (gen->line[i] > abs_input)
	      abs_input = gen->line[i];
	  gen->xscl = abs_input;
	}
    }
  return(gen->xscl);
}

static mus_float_t run_mus_moving_max(mus_any *ptr, mus_float_t input, mus_float_t unused) {return(mus_moving_max(ptr, input));}


static void moving_max_reset(mus_any *ptr)
{
  dly *gen = (dly *)ptr;
  delay_reset(ptr);
  gen->xscl = 0.0;
}


static char *describe_moving_max(mus_any *ptr)
{
  char *str = NULL;
  dly *gen = (dly *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s %.3f, line[%u]:%s",
	       mus_name(ptr),
	       gen->xscl, 
	       gen->size, 
	       str = float_array_to_string(gen->line, gen->size, gen->loc));
  if (str) free(str);
  return(describe_buffer);
}


static mus_any_class MOVING_MAX_CLASS = {
  MUS_MOVING_MAX,
  (char *)S_moving_max,
  &free_delay,
  &describe_moving_max,
  &delay_equalp,
  &delay_data,
  &delay_set_data,
  &delay_length,
  &delay_set_length,
  0, 0, 0, 0, /* freq phase */
  &delay_scaler,
  &delay_set_scaler,
  &delay_fb,
  &delay_set_fb,
  &run_mus_moving_max,
  MUS_DELAY_LINE,
  NULL, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &delay_loc,
  0, 0,
  0, 0, 0, 0,
  &moving_max_reset,
  0, &dly_copy
};


mus_any *mus_make_moving_max(int size, mus_float_t *line)
{
  dly *gen;
  gen = (dly *)mus_make_delay(size, line, size, MUS_INTERP_NONE);
  if (gen)
    {
      int i;
      gen->core = &MOVING_MAX_CLASS;
      gen->xscl = 0.0;
      for (i = 0; i < size; i++) 
	if (fabs(gen->line[i]) > gen->xscl)
	  gen->xscl = fabs(gen->line[i]);
      return((mus_any *)gen);
    }
  return(NULL);
}


/* -------- moving-norm -------- */

bool mus_is_moving_norm(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_MOVING_NORM));
}


mus_float_t mus_moving_norm(mus_any *ptr, mus_float_t input)
{
  dly *gen = (dly *)ptr;
  mus_float_t output, abs_input;

  abs_input = fabs(input);
  if (abs_input < 0.01) abs_input = 0.01; /* 0.01 sets the max norm output (~100) -- maybe a parameter to make-norm? */
  output = mus_moving_max(ptr, abs_input);
  gen->y1 = output + (gen->yscl * gen->y1);

  return(gen->norm / gen->y1);
}

static mus_float_t moving_norm_norm(mus_any *ptr)
{
  dly *gen = (dly *)ptr;
  return(gen->norm / (gen->size + 1.0));
}

static mus_float_t run_mus_moving_norm(mus_any *ptr, mus_float_t input, mus_float_t unused) {return(mus_moving_norm(ptr, input));}


static void moving_norm_reset(mus_any *ptr)
{
  dly *gen = (dly *)ptr;
  delay_reset(ptr);
  gen->xscl = 0.0;
  gen->y1 = 0.0;
}


static char *describe_moving_norm(mus_any *ptr)
{
  char *str = NULL;
  dly *gen = (dly *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s, max %.3f, y1 %.3f, weight %.3f, line[%u]:%s",
	   mus_name(ptr),
	   gen->xscl, gen->y1, gen->yscl,
	   gen->size, 
	   str = float_array_to_string(gen->line, gen->size, gen->loc));
  if (str) free(str);
  return(describe_buffer);
}


static mus_any_class MOVING_NORM_CLASS = {
  MUS_MOVING_NORM,
  (char *)S_moving_norm,
  &free_delay,
  &describe_moving_norm,
  &delay_equalp,
  &delay_data,
  &delay_set_data,
  &delay_length,
  &delay_set_length,
  0, 0, 0, 0, /* freq phase */
  &delay_scaler,
  &delay_set_scaler,
  &delay_fb,
  &delay_set_fb,
  &run_mus_moving_norm,
  MUS_DELAY_LINE,
  NULL, 0,
  &moving_norm_norm, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &delay_loc,
  0, 0,
  0, 0, 0, 0,
  &moving_norm_reset,
  0, &dly_copy
};


mus_any *mus_make_moving_norm(int size, mus_float_t *line, mus_float_t norm)
{
  dly *gen;
  gen = (dly *)mus_make_moving_max(size, line);
  if (gen)
    {
      gen->core = &MOVING_NORM_CLASS;
      
      gen->yscl = (mus_float_t)size / (size + 1.0); /* one-pole -b1 = -feedback so this is a lowpass filter */
      gen->norm = norm * (size + 1.0);
      gen->yn1 = 1.0 / size;
      gen->y1 = size + 1.0;
      
      return((mus_any *)gen);
    }
  return(NULL);
}
 




/* ---------------------------------------- filtered-comb ---------------------------------------- */

static void filtered_comb_reset(mus_any *ptr)
{
  dly *fc = (dly *)ptr;
  delay_reset(ptr);
  mus_reset(fc->filt);
}


static bool filtered_comb_equalp(mus_any *p1, mus_any *p2)
{
  return((delay_equalp(p1, p2)) &&
	 (mus_equalp(((dly *)p1)->filt, 
		     ((dly *)p2)->filt)));
}


static char *describe_filtered_comb(mus_any *ptr)
{
  char *comb_str, *filter_str, *res;
  int len;
  comb_str = describe_comb(ptr);
  filter_str = mus_describe(((dly *)ptr)->filt);
  len = strlen(comb_str) + strlen(filter_str) + 64;
  res = (char *)malloc(len * sizeof(char));
  if (filter_str)
    snprintf(res, len, "%s, filter: [%s]", comb_str, filter_str);
  else snprintf(res, len, "%s, filter: none?", comb_str);
  if (comb_str) free(comb_str);
  if (filter_str) free(filter_str);
  return(res);
}


bool mus_is_filtered_comb(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_FILTERED_COMB));
}


mus_float_t mus_filtered_comb(mus_any *ptr, mus_float_t input, mus_float_t pm)
{
  dly *fc = (dly *)ptr;
  if (fc->zdly)
    return(mus_delay(ptr,
		     input + (fc->yscl * 
			      fc->runf(fc->filt, 
				       mus_tap(ptr, pm), 
				       0.0)), 
		     pm)); 
  return(mus_delay_unmodulated(ptr,
			       input + (fc->yscl * 
					fc->runf(fc->filt, fc->line[fc->loc], 0.0))));
}


mus_float_t mus_filtered_comb_unmodulated(mus_any *ptr, mus_float_t input)
{
  dly *fc = (dly *)ptr;
  return(mus_delay_unmodulated(ptr,
			       input + (fc->yscl * 
					fc->runf(fc->filt, fc->line[fc->loc], 0.0))));
}


static mus_any_class FILTERED_COMB_CLASS = {
  MUS_FILTERED_COMB,
  (char *)S_filtered_comb,
  &free_delay,
  &describe_filtered_comb,
  &filtered_comb_equalp,
  &delay_data,
  &delay_set_data,
  &delay_length,
  &delay_set_length,
  0, 0, 0, 0, /* freq phase */
  &delay_scaler,
  &delay_set_scaler,
  &delay_fb,
  &delay_set_fb,
  &mus_filtered_comb,
  MUS_DELAY_LINE,
  NULL,
  &delay_interp_type,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &delay_loc,
  0, 0,
  0, 0, 0, 0,
  &filtered_comb_reset,
  0, &dly_copy
};


mus_any *mus_make_filtered_comb(mus_float_t scaler, int size, mus_float_t *line, int line_size, mus_interp_t type, mus_any *filt)
{
  dly *fc;
  fc = (dly *)mus_make_comb(scaler, size, line, line_size, type);
  if (fc)
    {
      fc->core = &FILTERED_COMB_CLASS;
      if (filt)
	fc->filt = filt;
      else 
	{
	  fc->filt = mus_make_one_zero(1.0, 0.0);
	  fc->filt_allocated = true;
	}
      fc->runf = mus_run_function(fc->filt);
      return((mus_any *)fc);
    }
  else return(NULL);
}


/* ---------------- filtered-comb-bank ---------------- */

typedef struct {
  mus_any_class *core;
  int size;
  mus_any **gens;
  mus_float_t (*cmbf)(mus_any *ptr, mus_float_t input);
} fltcmb_bank;


static void free_filtered_comb_bank(mus_any *ptr) 
{
  fltcmb_bank *f = (fltcmb_bank *)ptr;
  if (f->gens) {free(f->gens); f->gens = NULL;}
  free(ptr); 
}


static mus_any *fltcmb_bank_copy(mus_any *ptr)
{
  fltcmb_bank *g, *p;
  int i;

  p = (fltcmb_bank *)ptr;
  g = (fltcmb_bank *)malloc(sizeof(fltcmb_bank));
  memcpy((void *)g, (void *)ptr, sizeof(fltcmb_bank));
  g->gens = (mus_any **)malloc(p->size * sizeof(mus_any *));
  for (i = 0; i < p->size; i++)
    g->gens[i] = mus_copy(p->gens[i]);

  return((mus_any *)g);
}


static mus_float_t run_filtered_comb_bank(mus_any *ptr, mus_float_t input, mus_float_t unused) 
{
  return(mus_filtered_comb_bank(ptr, input));
}


static mus_long_t filtered_comb_bank_length(mus_any *ptr)
{
  return(((fltcmb_bank *)ptr)->size);
}


static void filtered_comb_bank_reset(mus_any *ptr)
{
  fltcmb_bank *f = (fltcmb_bank *)ptr;
  int i;
  for (i = 0; i < f->size; i++)
    mus_reset(f->gens[i]);
}


static bool filtered_comb_bank_equalp(mus_any *p1, mus_any *p2)
{
  fltcmb_bank *f1 = (fltcmb_bank *)p1;
  fltcmb_bank *f2 = (fltcmb_bank *)p2;
  int i, size;

  if (f1 == f2) return(true);
  if (f1->size != f2->size) return(false);
  size = f1->size;

  for (i = 0; i < size; i++)
    if (!filtered_comb_equalp(f1->gens[i], f2->gens[i]))
      return(false);

  return(true);
}


static char *describe_filtered_comb_bank(mus_any *ptr)
{
  fltcmb_bank *gen = (fltcmb_bank *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s size: %d",
	       mus_name(ptr),
	       gen->size);
  return(describe_buffer);
}

static mus_any_class FILTERED_COMB_BANK_CLASS = {
  MUS_FILTERED_COMB_BANK,
  (char *)S_filtered_comb_bank,
  &free_filtered_comb_bank,
  &describe_filtered_comb_bank,
  &filtered_comb_bank_equalp,
  0, 0,
  &filtered_comb_bank_length, 0,
  0, 0, 
  0, 0,
  0, 0,
  0, 0,
  &run_filtered_comb_bank,
  MUS_NOT_SPECIAL, 
  NULL, 0,
  0, 0, 0, 0,
  0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &filtered_comb_bank_reset,
  0, &fltcmb_bank_copy
};


static mus_float_t filtered_comb_one_zero(mus_any *ptr, mus_float_t input)
{
  dly *gen = (dly *)ptr;
  mus_float_t result;
  result = gen->line[gen->loc];
  gen->line[gen->loc] = input + mus_one_zero(gen->filt, result); /* gen->yscl folded into one_zero coeffs via smp_scl */
  gen->loc++;
  if (gen->loc >= gen->size) 
    gen->loc = 0;
  return(result);
}


static mus_float_t filtered_comb_bank_8(mus_any *combs, mus_float_t inval)
{
  fltcmb_bank *c = (fltcmb_bank *)combs;
  mus_any **gs;
  gs = c->gens;
  return(filtered_comb_one_zero(gs[0], inval) +
	 filtered_comb_one_zero(gs[1], inval) +
	 filtered_comb_one_zero(gs[2], inval) +
	 filtered_comb_one_zero(gs[3], inval) +
	 filtered_comb_one_zero(gs[4], inval) +
	 filtered_comb_one_zero(gs[5], inval) +
	 filtered_comb_one_zero(gs[6], inval) +
	 filtered_comb_one_zero(gs[7], inval));
}

static mus_float_t filtered_comb_bank_any(mus_any *filtered_combs, mus_float_t inval)
{
  int i;
  mus_float_t sum = 0.0;
  fltcmb_bank *c = (fltcmb_bank *)filtered_combs;
  for (i = 0; i < c->size; i++) 
    sum += mus_filtered_comb_unmodulated(c->gens[i], inval);
  return(sum);
}

static void smp_scl(mus_any *ptr, mus_float_t scl);

mus_any *mus_make_filtered_comb_bank(int size, mus_any **filtered_combs)
{
  fltcmb_bank *gen;
  int i;
  bool zdly = false, oz = true;

  gen = (fltcmb_bank *)malloc(sizeof(fltcmb_bank));
  gen->core = &FILTERED_COMB_BANK_CLASS;
  gen->size = size;

  gen->gens = (mus_any **)malloc(size * sizeof(mus_any *));
  for (i = 0; i < size; i++)
    {
      gen->gens[i] = filtered_combs[i];
      zdly = (zdly) || (((dly *)(filtered_combs[i]))->zdly);
      oz = (oz) && (mus_is_one_zero(((dly *)(filtered_combs[i]))->filt));
    }

  if ((size == 8) &&
      (oz) &&
      (!zdly))
    {
      gen->cmbf = filtered_comb_bank_8;
      for (i = 0; i < 8; i++)
	{
	  dly *d;
	  d = (dly *)gen->gens[i];
	  smp_scl(d->filt, d->yscl);
	}
    }
  else gen->cmbf = filtered_comb_bank_any;

  return((mus_any *)gen);
}


bool mus_is_filtered_comb_bank(mus_any *ptr)
{
  return((ptr) && 
	 (ptr->core->type == MUS_FILTERED_COMB_BANK));
}


mus_float_t mus_filtered_comb_bank(mus_any *filtered_combs, mus_float_t inval)
{
  fltcmb_bank *gen = (fltcmb_bank *)filtered_combs;
  return((gen->cmbf)(filtered_combs, inval));
}


mus_any *mus_bank_generator(mus_any *g, int i)
{
  if (mus_is_comb_bank(g))
    return(((cmb_bank *)g)->gens[i]);
  if (mus_is_all_pass_bank(g))
    return(((allp_bank *)g)->gens[i]);
  if (mus_is_filtered_comb_bank(g))
    return(((fltcmb_bank *)g)->gens[i]);
  return(NULL);
}




/* ---------------- sawtooth et al ---------------- */

typedef struct {
  mus_any_class *core;
  mus_float_t current_value;
  mus_float_t freq, phase, base, width;
} sw;


static void free_sw(mus_any *ptr) {free(ptr);}

static mus_any *sw_copy(mus_any *ptr)
{
  sw *g;
  g = (sw *)malloc(sizeof(sw));
  memcpy((void *)g, (void *)ptr, sizeof(sw));
  return((mus_any *)g);
}


mus_float_t mus_sawtooth_wave(mus_any *ptr, mus_float_t fm)
{
  sw *gen = (sw *)ptr;
  mus_float_t result;
  result = gen->current_value;
  gen->phase += (gen->freq + fm);
  if ((gen->phase >= TWO_PI) || (gen->phase < 0.0))
    {
      gen->phase = fmod(gen->phase, TWO_PI);
      if (gen->phase < 0.0) gen->phase += TWO_PI;
    }
  gen->current_value = gen->base * (gen->phase - M_PI);
  return(result);
}


static mus_float_t run_sawtooth_wave(mus_any *ptr, mus_float_t fm, mus_float_t unused) {return(mus_sawtooth_wave(ptr, fm));}

bool mus_is_sawtooth_wave(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_SAWTOOTH_WAVE));
}


static mus_float_t sw_freq(mus_any *ptr) {return(mus_radians_to_hz(((sw *)ptr)->freq));}
static mus_float_t sw_set_freq(mus_any *ptr, mus_float_t val) {((sw *)ptr)->freq = mus_hz_to_radians(val); return(val);}

static mus_float_t sw_increment(mus_any *ptr) {return(((sw *)ptr)->freq);}
static mus_float_t sw_set_increment(mus_any *ptr, mus_float_t val) {((sw *)ptr)->freq = val; return(val);}

static mus_float_t sw_phase(mus_any *ptr) {return(fmod(((sw *)ptr)->phase, TWO_PI));}
static mus_float_t sw_set_phase(mus_any *ptr, mus_float_t val) {((sw *)ptr)->phase = val; return(val);}

static mus_float_t sw_width(mus_any *ptr) {return((((sw *)ptr)->width) / ( 2 * M_PI));}
static mus_float_t sw_set_width(mus_any *ptr, mus_float_t val) {((sw *)ptr)->width = (2 * M_PI * val); return(val);}

static mus_float_t sawtooth_scaler(mus_any *ptr) {return(((sw *)ptr)->base * M_PI);}
static mus_float_t sawtooth_set_scaler(mus_any *ptr, mus_float_t val) {((sw *)ptr)->base = val / M_PI; return(val);}


static bool sw_equalp(mus_any *p1, mus_any *p2)
{
  sw *s1, *s2;
  s1 = (sw *)p1;
  s2 = (sw *)p2;
  return((p1 == p2) ||
	 ((s1) && (s2) &&
	  (s1->core->type == s2->core->type) &&
	  (s1->freq == s2->freq) &&
	  (s1->phase == s2->phase) &&
	  (s1->base == s2->base) &&
	  (s1->current_value == s2->current_value)));
}


static char *describe_sw(mus_any *ptr)
{
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s freq: %.3fHz, phase: %.3f, amp: %.3f",
	       mus_name(ptr),
	       mus_frequency(ptr),
	       mus_phase(ptr),
	       mus_scaler(ptr));
  return(describe_buffer);
}


static void sawtooth_reset(mus_any *ptr)
{
  sw *gen = (sw *)ptr;
  gen->phase = M_PI;
  gen->current_value = 0.0;
}


static mus_any_class SAWTOOTH_WAVE_CLASS = {
  MUS_SAWTOOTH_WAVE,
  (char *)S_sawtooth_wave,
  &free_sw,
  &describe_sw,
  &sw_equalp,
  0, 0, 0, 0,
  &sw_freq,
  &sw_set_freq,
  &sw_phase,
  &sw_set_phase,
  &sawtooth_scaler,
  &sawtooth_set_scaler,
  &sw_increment,
  &sw_set_increment,
  &run_sawtooth_wave,
  MUS_NOT_SPECIAL, 
  NULL, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &sawtooth_reset,
  0, &sw_copy
};


mus_any *mus_make_sawtooth_wave(mus_float_t freq, mus_float_t amp, mus_float_t phase) /* M_PI as initial phase, normally */
{
  sw *gen;
  gen = (sw *)malloc(sizeof(sw));
  gen->core = &SAWTOOTH_WAVE_CLASS;
  gen->freq = mus_hz_to_radians(freq);
  gen->base = (amp / M_PI);
  gen->phase = phase;
  gen->current_value = gen->base * (gen->phase - M_PI);
  return((mus_any *)gen);
}


mus_float_t mus_square_wave(mus_any *ptr, mus_float_t fm)
{
  sw *gen = (sw *)ptr;
  mus_float_t result;
  result = gen->current_value;
  gen->phase += (gen->freq + fm);
  if ((gen->phase >= TWO_PI) || (gen->phase < 0.0))
    {
      gen->phase = fmod(gen->phase, TWO_PI);
      if (gen->phase < 0.0) gen->phase += TWO_PI;
    }
  if (gen->phase < gen->width) 
    gen->current_value = gen->base; 
  else gen->current_value = 0.0;
  return(result);
}


bool mus_is_square_wave(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_SQUARE_WAVE));
}


static mus_float_t run_square_wave(mus_any *ptr, mus_float_t fm, mus_float_t unused) {return(mus_square_wave(ptr, fm));}

static mus_float_t square_wave_scaler(mus_any *ptr) {return(((sw *)ptr)->base);}
static mus_float_t square_wave_set_scaler(mus_any *ptr, mus_float_t val) {((sw *)ptr)->base = val; return(val);}


static void square_wave_reset(mus_any *ptr)
{
  sw *gen = (sw *)ptr;
  gen->phase = 0.0;
  gen->current_value = gen->base;
}


static mus_any_class SQUARE_WAVE_CLASS = {
  MUS_SQUARE_WAVE,
  (char *)S_square_wave,
  &free_sw,
  &describe_sw,
  &sw_equalp,
  0, 0, 0, 0,
  &sw_freq,
  &sw_set_freq,
  &sw_phase,
  &sw_set_phase,
  &square_wave_scaler,
  &square_wave_set_scaler,
  &sw_increment,
  &sw_set_increment,
  &run_square_wave,
  MUS_NOT_SPECIAL, 
  NULL, 0,
  0, 0, 
  &sw_width, &sw_set_width, 
  0, 0, 
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &square_wave_reset,
  0, &sw_copy
};


mus_any *mus_make_square_wave(mus_float_t freq, mus_float_t amp, mus_float_t phase)
{
  sw *gen;
  gen = (sw *)malloc(sizeof(sw));
  gen->core = &SQUARE_WAVE_CLASS;
  gen->freq = mus_hz_to_radians(freq);
  gen->base = amp;
  gen->phase = phase;
  gen->width = M_PI;
  if (gen->phase < gen->width) 
    gen->current_value = gen->base; 
  else gen->current_value = 0.0;
  return((mus_any *)gen);
}


mus_float_t mus_triangle_wave(mus_any *ptr, mus_float_t fm)
{
  sw *gen = (sw *)ptr;
  mus_float_t result;

  result = gen->current_value;
  gen->phase += (gen->freq + fm);
  if ((gen->phase >= TWO_PI) || (gen->phase < 0.0))
    {
      gen->phase = fmod(gen->phase, TWO_PI);
      if (gen->phase < 0.0) gen->phase += TWO_PI;
    }
  if (gen->phase < (M_PI / 2.0)) 
    gen->current_value = gen->base * gen->phase;
  else
    if (gen->phase < (M_PI * 1.5)) 
      gen->current_value = gen->base * (M_PI - gen->phase);
    else gen->current_value = gen->base * (gen->phase - TWO_PI);
  return(result);
}


mus_float_t mus_triangle_wave_unmodulated(mus_any *ptr)
{
  sw *gen = (sw *)ptr;
  mus_float_t result;

  result = gen->current_value;
  gen->phase += gen->freq;
 TRY_AGAIN:
  if (gen->phase < (M_PI / 2.0)) 
    gen->current_value = gen->base * gen->phase;
  else
    {
      if (gen->phase < (M_PI * 1.5)) 
	gen->current_value = gen->base * (M_PI - gen->phase);
      else 
	{
	  if (gen->phase < TWO_PI)
	    gen->current_value = gen->base * (gen->phase - TWO_PI);
	  else
	    {
	      gen->phase -= TWO_PI;
	      goto TRY_AGAIN;
	    }
	}
    }
  return(result);
}


bool mus_is_triangle_wave(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_TRIANGLE_WAVE));
}


static mus_float_t run_triangle_wave(mus_any *ptr, mus_float_t fm, mus_float_t unused) {return(mus_triangle_wave(ptr, fm));}

static mus_float_t triangle_wave_scaler(mus_any *ptr) {return(((sw *)ptr)->base * M_PI_2);}
static mus_float_t triangle_wave_set_scaler(mus_any *ptr, mus_float_t val) {((sw *)ptr)->base = (val * 2.0 / M_PI); return(val);}


static void triangle_wave_reset(mus_any *ptr)
{
  sw *gen = (sw *)ptr;
  gen->phase = 0.0;
  gen->current_value = 0.0;
}


static mus_any_class TRIANGLE_WAVE_CLASS = {
  MUS_TRIANGLE_WAVE,
  (char *)S_triangle_wave,
  &free_sw,
  &describe_sw,
  &sw_equalp,
  0, 0, 0, 0,
  &sw_freq,
  &sw_set_freq,
  &sw_phase,
  &sw_set_phase,
  &triangle_wave_scaler,
  &triangle_wave_set_scaler,
  &sw_increment,
  &sw_set_increment,
  &run_triangle_wave,
  MUS_NOT_SPECIAL, 
  NULL, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &triangle_wave_reset,
  0, &sw_copy
};


mus_any *mus_make_triangle_wave(mus_float_t freq, mus_float_t amp, mus_float_t phase)
{
  sw *gen;
  gen = (sw *)malloc(sizeof(sw));
  gen->core = &TRIANGLE_WAVE_CLASS;
  if (freq < 0.0) 
    {
      freq = -freq;
      phase += M_PI;
      if (phase > TWO_PI) phase -= TWO_PI;
    }
  gen->freq = mus_hz_to_radians(freq);
  gen->base = (2.0 * amp / M_PI);
  gen->phase = phase;
  if (gen->phase < M_PI_2) 
    gen->current_value = gen->base * gen->phase;
  else
    if (gen->phase < (M_PI * 1.5)) 
      gen->current_value = gen->base * (M_PI - gen->phase);
    else gen->current_value = gen->base * (gen->phase - TWO_PI);
  return((mus_any *)gen);
}


mus_float_t mus_pulse_train(mus_any *ptr, mus_float_t fm)
{
  sw *gen = (sw *)ptr;
  if ((gen->phase >= TWO_PI) || (gen->phase < 0.0))
    {
      gen->phase = fmod(gen->phase, TWO_PI);
      if (gen->phase < 0.0) gen->phase += TWO_PI;
      gen->current_value = gen->base;
    }
  else gen->current_value = 0.0;
  gen->phase += (gen->freq + fm);
  return(gen->current_value);
}


mus_float_t mus_pulse_train_unmodulated(mus_any *ptr)
{
  sw *gen = (sw *)ptr;
  /* here unfortunately, we might get any phase: (pulse-train p (+ (pulse-train p) -1.0))
   */
  if ((gen->phase >= TWO_PI) || (gen->phase < 0.0))
    {
      gen->phase = fmod(gen->phase, TWO_PI);
      if (gen->phase < 0.0) gen->phase += TWO_PI;
      gen->current_value = gen->base;
    }
  else gen->current_value = 0.0;
  gen->phase += gen->freq;
  return(gen->current_value);
}


bool mus_is_pulse_train(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_PULSE_TRAIN));
}


static mus_float_t run_pulse_train(mus_any *ptr, mus_float_t fm, mus_float_t unused) {return(mus_pulse_train(ptr, fm));}

static mus_float_t pulse_train_scaler(mus_any *ptr) {return(((sw *)ptr)->base);}
static mus_float_t pulse_train_set_scaler(mus_any *ptr, mus_float_t val) {((sw *)ptr)->base = val; return(val);}


static void pulse_train_reset(mus_any *ptr)
{
  sw *gen = (sw *)ptr;
  gen->phase = TWO_PI;
  gen->current_value = 0.0;
}


static mus_any_class PULSE_TRAIN_CLASS = {
  MUS_PULSE_TRAIN,
  (char *)S_pulse_train,
  &free_sw,
  &describe_sw,
  &sw_equalp,
  0, 0, 0, 0,
  &sw_freq,
  &sw_set_freq,
  &sw_phase,
  &sw_set_phase,
  &pulse_train_scaler,
  &pulse_train_set_scaler,
  &sw_increment,
  &sw_set_increment,
  &run_pulse_train,
  MUS_NOT_SPECIAL, 
  NULL, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &pulse_train_reset,
  0, &sw_copy
};


mus_any *mus_make_pulse_train(mus_float_t freq, mus_float_t amp, mus_float_t phase) /* TWO_PI initial phase, normally */
{
  sw *gen;
  gen = (sw *)malloc(sizeof(sw));
  gen->core = &PULSE_TRAIN_CLASS;
  if (freq < 0.0) freq = -freq;
  gen->freq = mus_hz_to_radians(freq);
  gen->base = amp;
  gen->phase = phase;
  gen->current_value = 0.0;
  return((mus_any *)gen);
}



/* ---------------- rand, rand_interp ---------------- */

typedef struct {
  mus_any_class *core;
  mus_float_t freq, phase, base, incr, norm;
  mus_float_t output;
  mus_float_t *distribution;
  int distribution_size;
  mus_float_t (*ran_unmod)(mus_any *ptr);
} noi;


/* rand taken from the ANSI C standard (essentially the same as the Cmix form used earlier) */

static unsigned long randx = 1;
#define INVERSE_MAX_RAND  0.0000610351563
#define INVERSE_MAX_RAND2 0.000030517579


void mus_set_rand_seed(unsigned long val) {randx = val;}

unsigned long mus_rand_seed(void) {return(randx);}


static mus_float_t next_random(void)
{
  randx = randx * 1103515245 + 12345;
  return((mus_float_t)((uint32_t)(randx >> 16) & 32767));
}


mus_float_t mus_random(mus_float_t amp) /* -amp to amp as mus_float_t */
{
  return(amp * (next_random() * INVERSE_MAX_RAND - 1.0));
}


mus_float_t mus_frandom(mus_float_t amp) /* 0.0 to amp as mus_float_t */
{
  return(amp * next_random() * INVERSE_MAX_RAND2);
}


int mus_irandom(int amp)
{
  return((int)(amp * next_random() * INVERSE_MAX_RAND2));
}


static mus_float_t random_any(noi *gen) /* -amp to amp possibly through distribution */
{
  if (gen->distribution)
    return(gen->base * mus_array_interp(gen->distribution, 
					next_random() * INVERSE_MAX_RAND2 * gen->distribution_size, 
					gen->distribution_size));
  return(gen->base * (next_random() * INVERSE_MAX_RAND - 1.0));
}


mus_float_t mus_rand(mus_any *ptr, mus_float_t fm)
{
  noi *gen = (noi *)ptr;
  if ((gen->phase >= TWO_PI) || (gen->phase < 0.0))
    {
      gen->phase = fmod(gen->phase, TWO_PI);
      if (gen->phase < 0.0) gen->phase += TWO_PI;
      gen->output = random_any(gen);
    }
  gen->phase += (gen->freq + fm);
  return(gen->output);
}


static mus_float_t zero_unmodulated(mus_any *ptr) {return(0.0);}

mus_float_t mus_rand_unmodulated(mus_any *ptr)
{
  noi *gen = (noi *)ptr;
  if (gen->phase >= TWO_PI)
    {
      gen->phase -= TWO_PI;
      gen->output = random_any(gen);
    }
  gen->phase += gen->freq;
  return(gen->output);
}


mus_float_t mus_rand_interp(mus_any *ptr, mus_float_t fm)
{
  /* fm can change the increment step during a ramp */
  noi *gen = (noi *)ptr;
  gen->output += gen->incr;
  if (gen->output > gen->base) 
    gen->output = gen->base;
  else 
    {
      if (gen->output < -gen->base)
	gen->output = -gen->base;
    }
  if ((gen->phase >= TWO_PI) || (gen->phase < 0.0))
    {
      double divisor;
      gen->phase = fmod(gen->phase, TWO_PI);
      if (gen->phase < 0.0) gen->phase += TWO_PI;
      gen->incr = random_any(gen) - gen->output;
      divisor = gen->freq + fm;
      if (divisor != 0.0)
	{
	  divisor = ceil(TWO_PI / divisor);
	  if (divisor != 0.0)
	    gen->incr /= divisor;
	}
    }
  gen->phase += (gen->freq + fm);
  return(gen->output);
}


mus_float_t mus_rand_interp_unmodulated(mus_any *ptr)
{
  return(((noi *)ptr)->ran_unmod(ptr));
}


mus_float_t (*mus_rand_interp_unmodulated_function(mus_any *g))(mus_any *gen);
mus_float_t (*mus_rand_interp_unmodulated_function(mus_any *g))(mus_any *gen)
{
  if (mus_is_rand_interp(g))
    return(((noi *)g)->ran_unmod);
  return(NULL);
}

static mus_float_t rand_interp_unmodulated_with_distribution(mus_any *ptr)
{
  noi *gen = (noi *)ptr;
  gen->output += gen->incr;
  if (gen->phase >= TWO_PI)
    {
      gen->phase -= TWO_PI;
      gen->incr = (random_any(gen) - gen->output) * gen->norm;
    }
  gen->phase += gen->freq;
  return(gen->output);
}


static mus_float_t rand_interp_unmodulated(mus_any *ptr)
{
  noi *gen = (noi *)ptr;
  gen->output += gen->incr;
  gen->phase += gen->freq;
  if (gen->phase >= TWO_PI)
    {
      gen->phase -= TWO_PI;
      gen->incr = (mus_random(gen->base) - gen->output) * gen->norm;
    }
  return(gen->output);
}


static mus_float_t run_rand(mus_any *ptr, mus_float_t fm, mus_float_t unused) {return(mus_rand(ptr, fm));}
static mus_float_t run_rand_interp(mus_any *ptr, mus_float_t fm, mus_float_t unused) {return(mus_rand_interp(ptr, fm));}


bool mus_is_rand(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_RAND));
}

bool mus_is_rand_interp(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_RAND_INTERP));
}


static void free_noi(mus_any *ptr) {free(ptr);}

static mus_any *noi_copy(mus_any *ptr)
{
  noi *g;
  g = (noi *)malloc(sizeof(noi));
  memcpy((void *)g, (void *)ptr, sizeof(noi));
  /* if ptr->distribution, it comes from elsewhere -- we don't touch it here,
   *   and in clm2xen, we merely wrap it.
   */
  return((mus_any *)g);
}

static mus_float_t noi_freq(mus_any *ptr) {return(mus_radians_to_hz(((noi *)ptr)->freq));}
static mus_float_t noi_set_freq(mus_any *ptr, mus_float_t val) 
{
  if (val < 0.0) val = -val; 
  ((noi *)ptr)->freq = mus_hz_to_radians(val); 
  return(val);
}

static mus_float_t interp_noi_set_freq(mus_any *ptr, mus_float_t val) 
{
  noi *gen = (noi *)ptr;
  if (val < 0.0) val = -val; 
  gen->freq = mus_hz_to_radians(val); 
  if (gen->freq != 0.0)
    gen->norm = 1.0 / (ceil(TWO_PI / gen->freq));
  return(val);
}

static mus_float_t noi_increment(mus_any *ptr) {return(((noi *)ptr)->freq);}
static mus_float_t noi_set_increment(mus_any *ptr, mus_float_t val) {((noi *)ptr)->freq = val; return(val);}

static mus_float_t noi_incr(mus_any *ptr) {return(((noi *)ptr)->incr);}
static mus_float_t noi_set_incr(mus_any *ptr, mus_float_t val) {((noi *)ptr)->incr = val; return(val);}

static mus_float_t noi_phase(mus_any *ptr) {return(fmod(((noi *)ptr)->phase, TWO_PI));}
static mus_float_t noi_set_phase(mus_any *ptr, mus_float_t val) {((noi *)ptr)->phase = val; return(val);}

static mus_float_t noi_scaler(mus_any *ptr) {return(((noi *)ptr)->base);}
static mus_float_t noi_set_scaler(mus_any *ptr, mus_float_t val) {((noi *)ptr)->base = val; return(val);} /* rand, not rand-interp */

static mus_float_t *noi_data(mus_any *ptr) {return(((noi *)ptr)->distribution);}
static mus_long_t noi_length(mus_any *ptr) {return(((noi *)ptr)->distribution_size);}


static mus_float_t randi_set_scaler(mus_any *ptr, mus_float_t val) 
{
  noi *gen = (noi *)ptr;
  if (val == 0.0)
    gen->ran_unmod = zero_unmodulated;
  else
    {
      if (gen->base == 0.0)
	{
	  if (gen->distribution)
	    gen->ran_unmod = rand_interp_unmodulated_with_distribution;
	  else gen->ran_unmod = rand_interp_unmodulated;
	}
    }
  gen->base = val; 
  return(val);
}

static void noi_reset(mus_any *ptr)
{
  noi *gen = (noi *)ptr;
  gen->phase = TWO_PI; /* 2*pi is the trigger, otherwise value after mus-reset is always 0.0, as Tito Latini noticed */
  gen->output = mus_is_rand_interp(ptr) ? random_any(gen) - gen->incr : 0.0;
}


static bool noi_equalp(mus_any *p1, mus_any *p2)
{
  noi *g1 = (noi *)p1;
  noi *g2 = (noi *)p2;
  return((p1 == p2) ||
	 ((g1) && (g2) &&
	  (g1->core->type == g2->core->type) &&
	  (g1->freq == g2->freq) &&
	  (g1->phase == g2->phase) &&
	  (g1->output == g2->output) &&
	  (g1->incr == g2->incr) &&
	  (g1->base == g2->base) &&
	  (g1->distribution_size == g2->distribution_size) &&
	  (g1->distribution == g2->distribution)));
}


static char *describe_noi(mus_any *ptr)
{
  noi *gen = (noi *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  if (mus_is_rand(ptr))
    snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s freq: %.3fHz, phase: %.3f, amp: %.3f%s",
		 mus_name(ptr),
		 mus_frequency(ptr),
		 mus_phase(ptr),
		 mus_scaler(ptr),
		 (gen->distribution) ? ", with distribution envelope" : "");
  else
    snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s freq: %.3fHz, phase: %.3f, amp: %.3f, incr: %.3f, curval: %.3f%s",
		 mus_name(ptr),
		 mus_frequency(ptr),
		 mus_phase(ptr),
		 mus_scaler(ptr),
		 gen->incr,
		 gen->output,
		 (gen->distribution) ? ", with distribution envelope" : "");
  return(describe_buffer);
}


static mus_any_class RAND_CLASS = {
  MUS_RAND,
  (char *)S_rand,
  &free_noi,
  &describe_noi,
  &noi_equalp,
  &noi_data, 0, 
  &noi_length, 0,
  &noi_freq,
  &noi_set_freq,
  &noi_phase,
  &noi_set_phase,
  &noi_scaler,
  &noi_set_scaler,
  &noi_increment, /* this is the phase increment, not the incr field */
  &noi_set_increment,
  &run_rand,
  MUS_NOT_SPECIAL, 
  NULL, 0,
  &noi_incr, &noi_set_incr, 
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &noi_reset,
  0, &noi_copy
};


static mus_any_class RAND_INTERP_CLASS = {
  MUS_RAND_INTERP,
  (char *)S_rand_interp,
  &free_noi,
  &describe_noi,
  &noi_equalp,
  &noi_data, 0, 
  &noi_length, 0,
  &noi_freq,
  &interp_noi_set_freq,
  &noi_phase,
  &noi_set_phase,
  &noi_scaler,
  &randi_set_scaler,
  &noi_increment, /* phase increment, not incr field */
  &noi_set_increment,
  &run_rand_interp,
  MUS_NOT_SPECIAL, 
  NULL, 0,
  &noi_incr, &noi_set_incr,  /* incr field == mus_offset method */
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &noi_reset,
  0, &noi_copy
};


mus_any *mus_make_rand(mus_float_t freq, mus_float_t base)
{
  noi *gen;
  gen = (noi *)calloc(1, sizeof(noi));
  gen->core = &RAND_CLASS;
  if (freq < 0.0) freq = -freq;
  gen->freq = mus_hz_to_radians(freq);
  gen->base = base;
  gen->incr = 0.0;
  gen->output = mus_random(base); /* this was always starting at 0.0 (changed 23-Dec-06) */
  return((mus_any *)gen);
}


mus_any *mus_make_rand_with_distribution(mus_float_t freq, mus_float_t base, mus_float_t *distribution, int distribution_size)
{
  noi *gen;
  gen = (noi *)calloc(1, sizeof(noi));
  gen->core = &RAND_CLASS;
  gen->distribution = distribution;
  gen->distribution_size = distribution_size;
  if (freq < 0.0) freq = -freq;
  gen->freq = mus_hz_to_radians(freq);
  gen->base = base;
  gen->incr = 0.0;
  gen->output = random_any(gen);
  return((mus_any *)gen);
}


mus_any *mus_make_rand_interp(mus_float_t freq, mus_float_t base)
{
  noi *gen;
  gen = (noi *)calloc(1, sizeof(noi));
  gen->core = &RAND_INTERP_CLASS;
  /* gen->distribution = NULL; */
  if (freq < 0.0) freq = -freq;
  gen->freq = mus_hz_to_radians(freq);
  gen->base = base;
  gen->output = mus_random(base);
  gen->incr = (mus_random(base) - gen->output) * freq / sampling_rate;
  gen->output -= gen->incr;
  if (gen->freq != 0.0)
    gen->norm = 1.0 / (ceil(TWO_PI / gen->freq));
  else gen->norm = 1.0;
  gen->ran_unmod = ((base == 0.0) ? zero_unmodulated : rand_interp_unmodulated);
  return((mus_any *)gen);
}


mus_any *mus_make_rand_interp_with_distribution(mus_float_t freq, mus_float_t base, mus_float_t *distribution, int distribution_size)
{
  noi *gen;
  gen = (noi *)calloc(1, sizeof(noi));
  gen->core = &RAND_INTERP_CLASS;
  gen->distribution = distribution;
  gen->distribution_size = distribution_size;
  if (freq < 0.0) freq = -freq;
  gen->freq = mus_hz_to_radians(freq);
  gen->base = base;
  gen->output = random_any(gen);
  gen->incr = (random_any(gen) - gen->output) * freq / sampling_rate;
  gen->output -= gen->incr;
  if (gen->freq != 0.0)
    gen->norm = 1.0 / (ceil(TWO_PI / gen->freq));
  else gen->norm = 1.0;
  gen->ran_unmod = ((base == 0.0) ? zero_unmodulated : rand_interp_unmodulated_with_distribution);
  return((mus_any *)gen);
}



/* ---------------- simple filters ---------------- */

typedef struct {
  mus_any_class *core;
  mus_float_t xs[3];
  mus_float_t ys[3];
  mus_float_t x1, x2, y1, y2;
} smpflt;


static void free_smpflt(mus_any *ptr) {free(ptr);}

static mus_any *smpflt_copy(mus_any *ptr)
{
  smpflt *g;
  g = (smpflt *)malloc(sizeof(smpflt));
  memcpy((void *)g, (void *)ptr, sizeof(smpflt));
  return((mus_any *)g);
}


static bool smpflt_equalp(mus_any *p1, mus_any *p2)
{
  smpflt *g1 = (smpflt *)p1;
  smpflt *g2 = (smpflt *)p2;
  return((p1 == p2) ||
	 ((g1->core->type == g2->core->type) &&
	  (g1->xs[0] == g2->xs[0]) &&
	  (g1->xs[1] == g2->xs[1]) &&
	  (g1->xs[2] == g2->xs[2]) &&
	  (g1->ys[1] == g2->ys[1]) &&
	  (g1->ys[2] == g2->ys[2]) &&
	  (g1->x1 == g2->x1) &&
	  (g1->x2 == g2->x2) &&
	  (g1->y1 == g2->y1) &&
	  (g1->y2 == g2->y2)));
}


static char *describe_smpflt(mus_any *ptr)
{
  smpflt *gen = (smpflt *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  switch (gen->core->type)
    {
    case MUS_ONE_ZERO: 
      snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s a0: %.3f, a1: %.3f, x1: %.3f", 
		   mus_name(ptr),
		   gen->xs[0], gen->xs[1], gen->x1); 
      break;

    case MUS_ONE_POLE: 
      snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s a0: %.3f, b1: %.3f, y1: %.3f", 
		   mus_name(ptr),
		   gen->xs[0], gen->ys[1], gen->y1); 
      break;

    case MUS_TWO_ZERO: 
      snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s a0: %.3f, a1: %.3f, a2: %.3f, x1: %.3f, x2: %.3f",
		   mus_name(ptr),
		   gen->xs[0], gen->xs[1], gen->xs[2], gen->x1, gen->x2); 
      break;

    case MUS_TWO_POLE: 
      snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s a0: %.3f, b1: %.3f, b2: %.3f, y1: %.3f, y2: %.3f",
		   mus_name(ptr),
		   gen->xs[0], gen->ys[1], gen->ys[2], gen->y1, gen->y2); 
      break;
    }
  return(describe_buffer);
}


mus_float_t mus_one_zero(mus_any *ptr, mus_float_t input)
{
  smpflt *gen = (smpflt *)ptr;
  mus_float_t result;
  result = (gen->xs[0] * input) + (gen->xs[1] * gen->x1);
  gen->x1 = input;
  return(result);
}


static mus_float_t run_one_zero(mus_any *ptr, mus_float_t input, mus_float_t unused) {return(mus_one_zero(ptr, input));}

static mus_long_t one_length(mus_any *ptr) {return(1);}
static mus_long_t two_length(mus_any *ptr) {return(2);}

static mus_float_t smp_xcoeff(mus_any *ptr, int index) {return(((smpflt *)ptr)->xs[index]);}
static mus_float_t smp_set_xcoeff(mus_any *ptr, int index, mus_float_t val) {((smpflt *)ptr)->xs[index] = val; return(val);}

static mus_float_t smp_ycoeff(mus_any *ptr, int index) {return(((smpflt *)ptr)->ys[index]);}
static mus_float_t smp_set_ycoeff(mus_any *ptr, int index, mus_float_t val) {((smpflt *)ptr)->ys[index] = val; return(val);}

static mus_float_t *smp_xcoeffs(mus_any *ptr) {return(((smpflt *)ptr)->xs);}
static mus_float_t *smp_ycoeffs(mus_any *ptr) {return(((smpflt *)ptr)->ys);}

static void smp_scl(mus_any *ptr, mus_float_t scl) {smpflt *g = (smpflt *)ptr; g->xs[0] *= scl; g->xs[1] *= scl;}

static void smpflt_reset(mus_any *ptr)
{
  smpflt *gen = (smpflt *)ptr;
  gen->x1 = 0.0;
  gen->x2 = 0.0;
  gen->y1 = 0.0;
  gen->y2 = 0.0;
}


static mus_any_class ONE_ZERO_CLASS = {
  MUS_ONE_ZERO,
  (char *)S_one_zero,
  &free_smpflt,
  &describe_smpflt,
  &smpflt_equalp,
  0, 0,
  &one_length, 0,
  0, 0, 0, 0,
  0, 0,
  0, 0,
  &run_one_zero,
  MUS_SIMPLE_FILTER, 
  NULL, 0,
  0, 0, 
  0, 0,
  &smp_xcoeff, &smp_set_xcoeff,
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 
  &smp_xcoeffs, &smp_ycoeffs,
  &smpflt_reset,
  0, &smpflt_copy
};


mus_any *mus_make_one_zero(mus_float_t a0, mus_float_t a1)
{
  smpflt *gen;
  gen = (smpflt *)calloc(1, sizeof(smpflt));
  gen->core = &ONE_ZERO_CLASS;
  gen->xs[0] = a0;
  gen->xs[1] = a1;
  return((mus_any *)gen);
}


bool mus_is_one_zero(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_ONE_ZERO));
}


mus_float_t mus_one_pole(mus_any *ptr, mus_float_t input)
{
  smpflt *gen = (smpflt *)ptr;
  gen->y1 = (gen->xs[0] * input) - (gen->ys[1] * gen->y1);
  return(gen->y1);
}

/* incrementer: (make-one-pole 1.0 -1.0) */

static mus_float_t run_one_pole(mus_any *ptr, mus_float_t input, mus_float_t unused) {return(mus_one_pole(ptr, input));}


static mus_any_class ONE_POLE_CLASS = {
  MUS_ONE_POLE,
  (char *)S_one_pole,
  &free_smpflt,
  &describe_smpflt,
  &smpflt_equalp,
  0, 0,
  &one_length, 0,
  0, 0, 0, 0,
  0, 0, 0, 0,
  &run_one_pole,
  MUS_SIMPLE_FILTER, 
  NULL, 0,
  0, 0, 0, 0, 
  &smp_xcoeff, &smp_set_xcoeff, 
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  &smp_ycoeff, &smp_set_ycoeff, 
  &smp_xcoeffs, &smp_ycoeffs,
  &smpflt_reset,
  0, &smpflt_copy
};


mus_any *mus_make_one_pole(mus_float_t a0, mus_float_t b1)
{
  smpflt *gen;
  gen = (smpflt *)calloc(1, sizeof(smpflt));
  gen->core = &ONE_POLE_CLASS;
  gen->xs[0] = a0;
  gen->ys[1] = b1;
  return((mus_any *)gen);
}


bool mus_is_one_pole(mus_any *ptr) 
{
  return((ptr) &&
	 (ptr->core->type == MUS_ONE_POLE));
}


mus_float_t mus_two_zero(mus_any *ptr, mus_float_t input)
{
  smpflt *gen = (smpflt *)ptr;
  mus_float_t result;
  result = (gen->xs[0] * input) + (gen->xs[1] * gen->x1) + (gen->xs[2] * gen->x2);
  gen->x2 = gen->x1;
  gen->x1 = input;
  return(result);
}


static mus_float_t run_two_zero(mus_any *ptr, mus_float_t input, mus_float_t unused) {return(mus_two_zero(ptr, input));}


static mus_float_t two_zero_radius(mus_any *ptr) 
{
  smpflt *gen = (smpflt *)ptr; 
  return(sqrt(gen->xs[2]));
}


static mus_float_t two_zero_set_radius(mus_any *ptr, mus_float_t new_radius)
{
  smpflt *gen = (smpflt *)ptr; 
  gen->xs[1] = -2.0 * new_radius * cos(mus_hz_to_radians(mus_frequency(ptr)));
  gen->xs[2] = new_radius * new_radius;
  return(new_radius);
}


static mus_float_t two_zero_frequency(mus_any *ptr)
{
  smpflt *gen = (smpflt *)ptr;
  if (two_zero_radius(ptr) == 0.0) return(0.0); /* or srate/2 */
  return(mus_radians_to_hz(acos(gen->xs[1] / (-2.0 * two_zero_radius(ptr)))));
}


static mus_float_t two_zero_set_frequency(mus_any *ptr, mus_float_t new_freq)
{
  smpflt *gen = (smpflt *)ptr; 
  gen->xs[1] = -2.0 * mus_scaler(ptr) * cos(mus_hz_to_radians(new_freq));
  return(new_freq);
}


static mus_any_class TWO_ZERO_CLASS = {
  MUS_TWO_ZERO,
  (char *)S_two_zero,
  &free_smpflt,
  &describe_smpflt,
  &smpflt_equalp,
  0, 0,
  &two_length, 0,
  &two_zero_frequency, &two_zero_set_frequency, 
  0, 0,
  &two_zero_radius, &two_zero_set_radius, 
  0, 0,
  &run_two_zero,
  MUS_SIMPLE_FILTER, 
  NULL, 0,
  0, 0, 0, 0,
  &smp_xcoeff, &smp_set_xcoeff,
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0,
  &smp_xcoeffs, &smp_ycoeffs,
  &smpflt_reset,
  0, &smpflt_copy
};


mus_any *mus_make_two_zero(mus_float_t a0, mus_float_t a1, mus_float_t a2)
{
  smpflt *gen;
  gen = (smpflt *)calloc(1, sizeof(smpflt));
  gen->core = &TWO_ZERO_CLASS;
  gen->xs[0] = a0;
  gen->xs[1] = a1;
  gen->xs[2] = a2;
  return((mus_any *)gen);
}


bool mus_is_two_zero(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_TWO_ZERO));
}


mus_any *mus_make_two_zero_from_frequency_and_radius(mus_float_t frequency, mus_float_t radius)
{
  return(mus_make_two_zero(1.0, -2.0 * radius * cos(mus_hz_to_radians(frequency)), radius * radius));
}


mus_float_t mus_two_pole(mus_any *ptr, mus_float_t input)
{
  smpflt *gen = (smpflt *)ptr;
  mus_float_t result;
  result = (gen->xs[0] * input) - (gen->ys[1] * gen->y1) - (gen->ys[2] * gen->y2);
  gen->y2 = gen->y1;
  gen->y1 = result;
  return(result);
}


static mus_float_t run_two_pole(mus_any *ptr, mus_float_t input, mus_float_t unused) {return(mus_two_pole(ptr, input));}


static mus_float_t two_pole_radius(mus_any *ptr) 
{
  smpflt *gen = (smpflt *)ptr; 
  return(sqrt(gen->ys[2]));
}


static mus_float_t two_pole_set_radius(mus_any *ptr, mus_float_t new_radius)
{
  smpflt *gen = (smpflt *)ptr; 
  gen->ys[1] = -2.0 * new_radius * cos(mus_hz_to_radians(mus_frequency(ptr)));
  gen->ys[2] = new_radius * new_radius;
  return(new_radius);
}


static mus_float_t two_pole_frequency(mus_any *ptr)
{
  smpflt *gen = (smpflt *)ptr;
  return(mus_radians_to_hz(acos(gen->ys[1] / (-2.0 * two_pole_radius(ptr)))));
}


static mus_float_t two_pole_set_frequency(mus_any *ptr, mus_float_t new_freq)
{
  smpflt *gen = (smpflt *)ptr; 
  gen->ys[1] = -2.0 * mus_scaler(ptr) * cos(mus_hz_to_radians(new_freq));
  return(new_freq);
}


static mus_any_class TWO_POLE_CLASS = {
  MUS_TWO_POLE,
  (char *)S_two_pole,
  &free_smpflt,
  &describe_smpflt,
  &smpflt_equalp,
  0, 0,
  &two_length, 0,
  &two_pole_frequency, &two_pole_set_frequency, 
  0, 0,
  &two_pole_radius, &two_pole_set_radius, 
  0, 0,
  &run_two_pole,
  MUS_SIMPLE_FILTER, 
  NULL, 0,
  0, 0, 0, 0,
  &smp_xcoeff, &smp_set_xcoeff, 
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  &smp_ycoeff, &smp_set_ycoeff, 
  &smp_xcoeffs, &smp_ycoeffs,
  &smpflt_reset,
  0, &smpflt_copy
};


mus_any *mus_make_two_pole(mus_float_t a0, mus_float_t b1, mus_float_t b2)
{
  smpflt *gen;
  gen = (smpflt *)calloc(1, sizeof(smpflt));
  gen->core = &TWO_POLE_CLASS;
  gen->xs[0] = a0;
  gen->ys[1] = b1;
  gen->ys[2] = b2;
  return((mus_any *)gen);
}


bool mus_is_two_pole(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_TWO_POLE));
}


mus_any *mus_make_two_pole_from_frequency_and_radius(mus_float_t frequency, mus_float_t radius)
{
  return(mus_make_two_pole(1.0, -2.0 * radius * cos(mus_hz_to_radians(frequency)), radius * radius));
}



/* ---------------- formant ---------------- */

typedef struct {
  mus_any_class *core;
  mus_float_t frequency, radius;
  mus_float_t x1, x2, y1, y2;
  mus_float_t rr, gain, fdbk;
} frm;


static void free_frm(mus_any *ptr) {free(ptr);}


static mus_any *frm_copy(mus_any *ptr)
{
  frm *g;
  g = (frm *)malloc(sizeof(frm));
  memcpy((void *)g, (void *)ptr, sizeof(frm));
  return((mus_any *)g);
}


bool mus_is_formant(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_FORMANT));
}


static void frm_reset(mus_any *ptr)
{
  frm *gen = (frm *)ptr;
  gen->x1 = 0.0;
  gen->x2 = 0.0;
  gen->y1 = 0.0;
  gen->y2 = 0.0;
}


static bool frm_equalp(mus_any *p1, mus_any *p2)
{
  frm *g1 = (frm *)p1;
  frm *g2 = (frm *)p2;
  return((p1 == p2) ||
	 ((g1->core->type == g2->core->type) &&
	  (g1->radius == g2->radius) &&
	  (g1->frequency == g2->frequency) &&
	  (g1->x1 == g2->x1) &&
	  (g1->x2 == g2->x2) &&
	  (g1->y1 == g2->y1) &&
	  (g1->y2 == g2->y2)));
}


static char *describe_formant(mus_any *ptr)
{
  frm *gen = (frm *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s frequency: %.3f, radius: %.3f",
	       mus_name(ptr),
	       mus_radians_to_hz(gen->frequency),
	       gen->radius);
  return(describe_buffer);
}


mus_float_t mus_formant(mus_any *ptr, mus_float_t input) 
{
  frm *gen = (frm *)ptr;
  mus_float_t x0, y0;
  x0 = gen->gain * input;
  y0 = x0 - gen->x2 + (gen->fdbk * gen->y1) - (gen->rr * gen->y2);
  gen->y2 = gen->y1;
  gen->y1 = y0;
  gen->x2 = gen->x1;
  gen->x1 = x0;
  return(y0);
}


static mus_float_t run_formant(mus_any *ptr, mus_float_t input, mus_float_t unused) {return(mus_formant(ptr, input));}


static void mus_set_formant_radius_and_frequency_in_radians(mus_any *ptr, mus_float_t radius, mus_float_t freq_in_radians)
{
  frm *gen = (frm *)ptr;
  gen->radius = radius;
  gen->frequency = freq_in_radians;
  gen->rr = radius * radius;
  gen->gain = (1.0 - gen->rr) * 0.5; 
  gen->fdbk = 2.0 * radius * cos(freq_in_radians);
}


void mus_set_formant_radius_and_frequency(mus_any *ptr, mus_float_t radius, mus_float_t freq_in_hz)
{
  mus_set_formant_radius_and_frequency_in_radians(ptr, radius, mus_hz_to_radians(freq_in_hz));
}


static mus_float_t formant_frequency(mus_any *ptr) {return(mus_radians_to_hz(((frm *)ptr)->frequency));}

mus_float_t mus_set_formant_frequency(mus_any *ptr, mus_float_t freq_in_hz)
{
  frm *gen = (frm *)ptr;
  mus_float_t fw;
  fw = mus_hz_to_radians(freq_in_hz);
  gen->frequency = fw;
  gen->fdbk = 2.0 * gen->radius * cos(fw);
  return(freq_in_hz);
}


mus_float_t mus_formant_with_frequency(mus_any *ptr, mus_float_t input, mus_float_t freq_in_radians)
{
  frm *gen = (frm *)ptr;
  if (gen->frequency != freq_in_radians)
    {
      gen->frequency = freq_in_radians;
      gen->fdbk = 2.0 * gen->radius * cos(freq_in_radians);
    }
  return(mus_formant(ptr, input));
}


static mus_float_t formant_radius(mus_any *ptr) {return(((frm *)ptr)->radius);}

static mus_float_t formant_set_radius(mus_any *ptr, mus_float_t val) 
{
  mus_set_formant_radius_and_frequency_in_radians(ptr, val, ((frm *)ptr)->frequency);
  return(val);
}


static mus_any_class FORMANT_CLASS = {
  MUS_FORMANT,
  (char *)S_formant,
  &free_frm,
  &describe_formant,
  &frm_equalp,
  0, 0,
  &two_length, 0,
  &formant_frequency, &mus_set_formant_frequency,
  0, 0,
  &formant_radius, &formant_set_radius,
  0, 0,
  &run_formant,
  MUS_SIMPLE_FILTER, 
  NULL, 0,
  0, 0, 0, 0,
  0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &frm_reset,
  0, &frm_copy
};


mus_any *mus_make_formant(mus_float_t frequency, mus_float_t radius)
{
  frm *gen;
  gen = (frm *)calloc(1, sizeof(frm));
  gen->core = &FORMANT_CLASS;
  mus_set_formant_radius_and_frequency((mus_any *)gen, radius, frequency);
  return((mus_any *)gen);
}


/* ---------------- formant-bank ---------------- */

typedef struct {
  mus_any_class *core;
  int size, mctr;
  mus_float_t *x0, *x1, *x2, *y0, *y1, *y2, *amps, *rr, *fdbk, *gain;
  mus_float_t c1, c2;
  mus_float_t (*one_input)(mus_any *fbank, mus_float_t inval);
  mus_float_t (*many_inputs)(mus_any *fbank, mus_float_t *inval);
} frm_bank;


static void free_formant_bank(mus_any *ptr) 
{
  frm_bank *f = (frm_bank *)ptr;
  if (f->x0) {free(f->x0); f->x0 = NULL;}
  if (f->x1) {free(f->x1); f->x1 = NULL;}
  if (f->x2) {free(f->x2); f->x2 = NULL;}
  if (f->y0) {free(f->y0); f->y0 = NULL;}
  if (f->y1) {free(f->y1); f->y1 = NULL;}
  if (f->y2) {free(f->y2); f->y2 = NULL;}
  if (f->rr) {free(f->rr); f->rr = NULL;}
  if (f->fdbk) {free(f->fdbk); f->fdbk = NULL;}
  if (f->gain) {free(f->gain); f->gain = NULL;}
  free(ptr); 
}

static mus_any *frm_bank_copy(mus_any *ptr)
{
  frm_bank *g, *p;
  int bytes;

  p = (frm_bank *)ptr;
  g = (frm_bank *)malloc(sizeof(frm_bank));
  memcpy((void *)g, (void *)ptr, sizeof(frm_bank));
  bytes = g->size * sizeof(mus_float_t);

  g->x0 = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->x0, p->x0, g->size);
  g->x1 = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->x1, p->x1, g->size);
  g->x2 = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->x2, p->x2, g->size);
  g->y0 = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->y0, p->y0, g->size);
  g->y1 = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->y1, p->y1, g->size);
  g->y2 = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->y2, p->y2, g->size);

  g->rr = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->rr, p->rr, g->size);
  g->fdbk = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->fdbk, p->fdbk, g->size);
  g->gain = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->gain, p->gain, g->size);

  return((mus_any *)g);
}

static mus_float_t run_formant_bank(mus_any *ptr, mus_float_t input, mus_float_t unused) 
{
  return(mus_formant_bank(ptr, input));
}


static mus_long_t formant_bank_length(mus_any *ptr)
{
  return(((frm_bank *)ptr)->size);
}


static void formant_bank_reset(mus_any *ptr)
{
  frm_bank *f = (frm_bank *)ptr;
  mus_clear_floats((f->x0), f->size);
  mus_clear_floats((f->x1), f->size);
  mus_clear_floats((f->x2), f->size);
  mus_clear_floats((f->y0), f->size);
  mus_clear_floats((f->y1), f->size);
  mus_clear_floats((f->y2), f->size);
}


static bool formant_bank_equalp(mus_any *p1, mus_any *p2)
{
  frm_bank *f1 = (frm_bank *)p1;
  frm_bank *f2 = (frm_bank *)p2;
#if 0
  int i, size;
#endif
  if (f1 == f2) return(true);
  if (f1->size != f2->size) return(false);
#if 0
  size = f1->size;
  for (i = 0; i < size; i++)
    if (!frm_equalp(f1->gens[i], f2->gens[i]))
      return(false);
#endif  
  /* now check the locals... */
  return(true);
}


static char *describe_formant_bank(mus_any *ptr)
{
  frm_bank *gen = (frm_bank *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s size: %d", mus_name(ptr), gen->size);
  return(describe_buffer);
}


mus_float_t mus_formant_bank(mus_any *fbank, mus_float_t inval)
{
  frm_bank *bank = (frm_bank *)fbank;
  return(bank->one_input(fbank, inval));
}

mus_float_t mus_formant_bank_with_inputs(mus_any *fbank, mus_float_t *inval)
{
  frm_bank *bank = (frm_bank *)fbank;
  return(bank->many_inputs(fbank, inval));
}


static mus_float_t fb_one_with_amps(mus_any *fbank, mus_float_t inval)
{
  frm_bank *bank = (frm_bank *)fbank;
  int i, size4;
  mus_float_t sum = 0.0;
  mus_float_t *x0, *x1, *x2, *y0, *y1, *y2, *amps, *rr, *fdbk, *gain;

  x0 = bank->x0;
  x1 = bank->x1;
  x2 = bank->x2;
  y0 = bank->y0;
  y1 = bank->y1;
  y2 = bank->y2;
  rr = bank->rr;
  fdbk = bank->fdbk;
  gain = bank->gain;
  amps = bank->amps;
  size4 = bank->size - 4;

  i = 0;
  while (i <= size4)
    {
      x0[i] = gain[i] * inval;
      y0[i] = x0[i] - x2[i] + (fdbk[i] * y1[i]) - (rr[i] * y2[i]);
      sum += amps[i] * y0[i];
      i++;
      
      x0[i] = gain[i] * inval;
      y0[i] = x0[i] - x2[i] + (fdbk[i] * y1[i]) - (rr[i] * y2[i]);
      sum += amps[i] * y0[i];
      i++;
      
      x0[i] = gain[i] * inval;
      y0[i] = x0[i] - x2[i] + (fdbk[i] * y1[i]) - (rr[i] * y2[i]);
      sum += amps[i] * y0[i];
      i++;
      
      x0[i] = gain[i] * inval;
      y0[i] = x0[i] - x2[i] + (fdbk[i] * y1[i]) - (rr[i] * y2[i]);
      sum += amps[i] * y0[i];
      i++;
    }
  for (; i < bank->size; i++)
    {
      x0[i] = gain[i] * inval;
      y0[i] = x0[i] - x2[i] + (fdbk[i] * y1[i]) - (rr[i] * y2[i]);
      sum += amps[i] * y0[i];
    }

  bank->x2 = x1;
  bank->x1 = x0;
  bank->x0 = x2;

  bank->y2 = y1;
  bank->y1 = y0;
  bank->y0 = y2;

  return(sum);
}

static mus_float_t fb_one_without_amps(mus_any *fbank, mus_float_t inval)
{
  frm_bank *bank = (frm_bank *)fbank;
  int i;
  mus_float_t sum = 0.0;
  mus_float_t *x0, *x1, *x2, *y0, *y1, *y2, *rr, *fdbk, *gain;

  x0 = bank->x0;
  x1 = bank->x1;
  x2 = bank->x2;
  y0 = bank->y0;
  y1 = bank->y1;
  y2 = bank->y2;
  rr = bank->rr;
  fdbk = bank->fdbk;
  gain = bank->gain;

  for (i = 0; i < bank->size; i++)
    {
      x0[i] = gain[i] * inval;
      y0[i] = x0[i] - x2[i] + (fdbk[i] * y1[i]) - (rr[i] * y2[i]);
      sum += y0[i];
    }

  bank->x2 = x1;
  bank->x1 = x0;
  bank->x0 = x2;

  bank->y2 = y1;
  bank->y1 = y0;
  bank->y0 = y2;

  return(sum);
}

static mus_float_t fb_many_with_amps(mus_any *fbank, mus_float_t *inval)
{
  frm_bank *bank = (frm_bank *)fbank;
  int i;
  mus_float_t sum = 0.0;
  mus_float_t *x0, *x1, *x2, *y0, *y1, *y2, *amps, *rr, *fdbk, *gain;

  x0 = bank->x0;
  x1 = bank->x1;
  x2 = bank->x2;
  y0 = bank->y0;
  y1 = bank->y1;
  y2 = bank->y2;
  rr = bank->rr;
  fdbk = bank->fdbk;
  gain = bank->gain;
  amps = bank->amps;

  for (i = 0; i < bank->size; i++)
    {
      x0[i] = gain[i] * inval[i];
      y0[i] = x0[i] - x2[i] + (fdbk[i] * y1[i]) - (rr[i] * y2[i]);
      sum += amps[i] * y0[i];
    }

  bank->x2 = x1;
  bank->x1 = x0;
  bank->x0 = x2;

  bank->y2 = y1;
  bank->y1 = y0;
  bank->y0 = y2;

  return(sum);
}

static mus_float_t fb_many_without_amps(mus_any *fbank, mus_float_t *inval)
{
  frm_bank *bank = (frm_bank *)fbank;
  int i;
  mus_float_t sum = 0.0;
  mus_float_t *x0, *x1, *x2, *y0, *y1, *y2, *rr, *fdbk, *gain;

  x0 = bank->x0;
  x1 = bank->x1;
  x2 = bank->x2;
  y0 = bank->y0;
  y1 = bank->y1;
  y2 = bank->y2;
  rr = bank->rr;
  fdbk = bank->fdbk;
  gain = bank->gain;

  for (i = 0; i < bank->size; i++)
    {
      x0[i] = gain[i] * inval[i];
      y0[i] = x0[i] - x2[i] + (fdbk[i] * y1[i]) - (rr[i] * y2[i]);
      sum += y0[i];
    }

  bank->x2 = x1;
  bank->x1 = x0;
  bank->x0 = x2;

  bank->y2 = y1;
  bank->y1 = y0;
  bank->y0 = y2;

  return(sum);
}

static mus_float_t fb_one_with_amps_c1_c2(mus_any *fbank, mus_float_t inval)
{
  frm_bank *bank = (frm_bank *)fbank;
  int i, size4;
  mus_float_t sum = 0.0, rr, gain;
  mus_float_t *x0, *x1, *x2, *y0, *y1, *y2, *amps, *fdbk;

  x0 = bank->x0;
  x1 = bank->x1;
  x2 = bank->x2;
  y0 = bank->y0;
  y1 = bank->y1;
  y2 = bank->y2;
  fdbk = bank->fdbk;
  amps = bank->amps;
  size4 = bank->size - 4;

  bank->mctr++;

  rr = bank->c1;
  gain = (bank->c2 * inval);
  x0[0] = gain;

  if (bank->mctr < 3)
    {
      i = 0;
      while (i <= size4)
	{
	  y0[i] = gain - x2[i] + (fdbk[i] * y1[i]) - (rr * y2[i]);
	  sum += amps[i] * y0[i];
	  i++;
	  /* in isolation this looks like the x0[i]-x2[i] business could be handled outside the loop
	   *   by a single float, but formant-bank can be called in the same do-loop both with and
	   *   without multiple inputs, so fb_one has to be completely compatible sample-by-sample
	   *   with fb_many.  Since we can't predict here when we'll need bank->x2, we can't collapse
	   *   this calculation. 
	   *
	   * If we know we've had 2 fb_one calls just before this one, then x2[i] are all the same,
	   *   x0[i] will all be the same in this loop, so x0[i] - x2[i] can be collapsed, but
	   *   we still need to set x0[0]=gain: enter mctr.
	   *
	   * So in the current case, we can save x0[0]=gain -> x1 -> x2, then in fm_many
	   *   mctr=1 -- x2 is ok, x1[0] needs to be propagated
	   *   mctr>1 -- x2 and x1 need propagation
	   * On the other side, if mctr>=3, then x2[i] was not set, so don't access it.
	   */
	  
	  y0[i] = gain - x2[i] + (fdbk[i] * y1[i]) - (rr * y2[i]);
	  sum += amps[i] * y0[i];
	  i++;
	  
	  y0[i] = gain - x2[i] + (fdbk[i] * y1[i]) - (rr * y2[i]);
	  sum += amps[i] * y0[i];
	  i++;
	  
	  y0[i] = gain - x2[i] + (fdbk[i] * y1[i]) - (rr * y2[i]);
	  sum += amps[i] * y0[i];
	  i++;
	}
      for (; i < bank->size; i++)
	{
	  y0[i] = gain - x2[i] + (fdbk[i] * y1[i]) - (rr * y2[i]);
	  sum += amps[i] * y0[i];
	}
    }
  else
    {
      mus_float_t g2;
      g2 = gain - x2[0];
      i = 0;
      while (i <= size4)
	{
	  y0[i] = g2 + (fdbk[i] * y1[i]) - (rr * y2[i]);
	  sum += amps[i] * y0[i];
	  i++;
	  
	  y0[i] = g2 + (fdbk[i] * y1[i]) - (rr * y2[i]);
	  sum += amps[i] * y0[i];
	  i++;
	  
	  y0[i] = g2 + (fdbk[i] * y1[i]) - (rr * y2[i]);
	  sum += amps[i] * y0[i];
	  i++;
	  
	  y0[i] = g2 + (fdbk[i] * y1[i]) - (rr * y2[i]);
	  sum += amps[i] * y0[i];
	  i++;
	}
      for (; i < bank->size; i++)
	{
	  y0[i] = g2 + (fdbk[i] * y1[i]) - (rr * y2[i]);
	  sum += amps[i] * y0[i];
	}
    }

  bank->x2 = x1;
  bank->x1 = x0;
  bank->x0 = x2;

  bank->y2 = y1;
  bank->y1 = y0;
  bank->y0 = y2;

  return(sum);
}

static mus_float_t fb_many_with_amps_c1_c2(mus_any *fbank, mus_float_t *inval)
{
  frm_bank *bank = (frm_bank *)fbank;
  int i, size4;
  mus_float_t sum = 0.0, rr, gain;
  mus_float_t *x0, *x1, *x2, *y0, *y1, *y2, *amps, *fdbk;

  x0 = bank->x0;
  x1 = bank->x1;
  x2 = bank->x2;
  y0 = bank->y0;
  y1 = bank->y1;
  y2 = bank->y2;
  fdbk = bank->fdbk;
  amps = bank->amps;
  size4 = bank->size - 4;

  if (bank->mctr > 0)
    {
      if (bank->mctr == 1)
	{
	  for (i = 1; i < bank->size; i++) x1[i] = x1[0];
	}
      else
	{
	  for (i = 1; i < bank->size; i++) {x1[i] = x1[0]; x2[i] = x2[0];}
	}
      bank->mctr = 0;
    }
  rr = bank->c1;
  gain = bank->c2;

  i = 0;
  while (i <= size4)
    {
      x0[i] = gain * inval[i];
      y0[i] = x0[i] - x2[i] + (fdbk[i] * y1[i]) - (rr * y2[i]);
      sum += amps[i] * y0[i];
      i++;
      
      x0[i] = gain * inval[i];
      y0[i] = x0[i] - x2[i] + (fdbk[i] * y1[i]) - (rr * y2[i]);
      sum += amps[i] * y0[i];
      i++;
      
      x0[i] = gain * inval[i];
      y0[i] = x0[i] - x2[i] + (fdbk[i] * y1[i]) - (rr * y2[i]);
      sum += amps[i] * y0[i];
      i++;
      
      x0[i] = gain * inval[i];
      y0[i] = x0[i] - x2[i] + (fdbk[i] * y1[i]) - (rr * y2[i]);
      sum += amps[i] * y0[i];
      i++;
    }
  for (; i < bank->size; i++)
    {
      x0[i] = gain * inval[i];
      y0[i] = x0[i] - x2[i] + (fdbk[i] * y1[i]) - (rr * y2[i]);
      sum += amps[i] * y0[i];
    }

  bank->x2 = x1;
  bank->x1 = x0;
  bank->x0 = x2;

  bank->y2 = y1;
  bank->y1 = y0;
  bank->y0 = y2;

  return(sum);
}


static mus_float_t fb_one_without_amps_c1_c2(mus_any *fbank, mus_float_t inval)
{
  frm_bank *bank = (frm_bank *)fbank;
  int i, size4;
  mus_float_t sum = 0.0, rr, gain;
  mus_float_t *x0, *x1, *x2, *y0, *y1, *y2, *fdbk;

  x0 = bank->x0;
  x1 = bank->x1;
  x2 = bank->x2;
  y0 = bank->y0;
  y1 = bank->y1;
  y2 = bank->y2;
  fdbk = bank->fdbk;
  size4 = bank->size - 4;

  bank->mctr++;

  rr = bank->c1;
  gain = (bank->c2 * inval);
  x0[0] = gain;

  if (bank->mctr < 3)
    {
      i = 0;
      while (i <= size4)
	{
	  y0[i] = gain - x2[i] + (fdbk[i] * y1[i]) - (rr * y2[i]);
	  sum += y0[i];
	  i++;
	  
	  y0[i] = gain - x2[i] + (fdbk[i] * y1[i]) - (rr * y2[i]);
	  sum += y0[i];
	  i++;
	  
	  y0[i] = gain - x2[i] + (fdbk[i] * y1[i]) - (rr * y2[i]);
	  sum += y0[i];
	  i++;
	  
	  y0[i] = gain - x2[i] + (fdbk[i] * y1[i]) - (rr * y2[i]);
	  sum += y0[i];
	  i++;
	}
      for (; i < bank->size; i++)
	{
	  y0[i] = gain - x2[i] + (fdbk[i] * y1[i]) - (rr * y2[i]);
	  sum += y0[i];
	}
    }
  else
    {
      mus_float_t g2;
      g2 = gain - x2[0];
      i = 0;
      while (i <= size4)
	{
	  y0[i] = g2 + (fdbk[i] * y1[i]) - (rr * y2[i]);
	  sum += y0[i];
	  i++;
	  
	  y0[i] = g2 + (fdbk[i] * y1[i]) - (rr * y2[i]);
	  sum += y0[i];
	  i++;
	  
	  y0[i] = g2 + (fdbk[i] * y1[i]) - (rr * y2[i]);
	  sum += y0[i];
	  i++;
	  
	  y0[i] = g2 + (fdbk[i] * y1[i]) - (rr * y2[i]);
	  sum += y0[i];
	  i++;
	}
      for (; i < bank->size; i++)
	{
	  y0[i] = g2 + (fdbk[i] * y1[i]) - (rr * y2[i]);
	  sum += y0[i];
	}
    }

  bank->x2 = x1;
  bank->x1 = x0;
  bank->x0 = x2;

  bank->y2 = y1;
  bank->y1 = y0;
  bank->y0 = y2;

  return(sum);
}


static mus_float_t fb_many_without_amps_c1_c2(mus_any *fbank, mus_float_t *inval)
{
  frm_bank *bank = (frm_bank *)fbank;
  int i, size4;
  mus_float_t sum = 0.0, rr, gain;
  mus_float_t *x0, *x1, *x2, *y0, *y1, *y2, *fdbk;

  x0 = bank->x0;
  x1 = bank->x1;
  x2 = bank->x2;
  y0 = bank->y0;
  y1 = bank->y1;
  y2 = bank->y2;
  fdbk = bank->fdbk;
  size4 = bank->size - 4;

  if (bank->mctr > 0)
    {
      if (bank->mctr == 1)
	{
	  for (i = 1; i < bank->size; i++) x1[i] = x1[0];
	}
      else
	{
	  for (i = 1; i < bank->size; i++) {x1[i] = x1[0]; x2[i] = x2[0];}
	}
      bank->mctr = 0;
    }

  rr = bank->c1;
  gain = bank->c2;

  i = 0;
  while (i <= size4)
    {
      x0[i] = gain * inval[i];
      y0[i] = x0[i] - x2[i] + (fdbk[i] * y1[i]) - (rr * y2[i]);
      sum += y0[i];
      i++;
      
      x0[i] = gain * inval[i];
      y0[i] = x0[i] - x2[i] + (fdbk[i] * y1[i]) - (rr * y2[i]);
      sum += y0[i];
      i++;
      
      x0[i] = gain * inval[i];
      y0[i] = x0[i] - x2[i] + (fdbk[i] * y1[i]) - (rr * y2[i]);
      sum += y0[i];
      i++;
      
      x0[i] = gain * inval[i];
      y0[i] = x0[i] - x2[i] + (fdbk[i] * y1[i]) - (rr * y2[i]);
      sum += y0[i];
      i++;
    }
  for (; i < bank->size; i++)
    {
      x0[i] = gain * inval[i];
      y0[i] = x0[i] - x2[i] + (fdbk[i] * y1[i]) - (rr * y2[i]);
      sum += y0[i];
    }

  bank->x2 = x1;
  bank->x1 = x0;
  bank->x0 = x2;

  bank->y2 = y1;
  bank->y1 = y0;
  bank->y0 = y2;

  return(sum);
}


static mus_any_class FORMANT_BANK_CLASS = {
  MUS_FORMANT_BANK,
  (char *)S_formant_bank,
  &free_formant_bank,
  &describe_formant_bank,
  &formant_bank_equalp,
  0, 0,
  &formant_bank_length, 0,
  0, 0, 
  0, 0,
  0, 0,
  0, 0,
  &run_formant_bank,
  MUS_NOT_SPECIAL, 
  NULL, 0,
  0, 0, 0, 0,
  0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &formant_bank_reset,
  0, &frm_bank_copy
};


mus_any *mus_make_formant_bank(int size, mus_any **formants, mus_float_t *amps)
{
  frm_bank *gen;
  int i;

  gen = (frm_bank *)malloc(sizeof(frm_bank));
  gen->core = &FORMANT_BANK_CLASS;
  gen->size = size;
  gen->mctr = 0;

  gen->x0 = (mus_float_t *)calloc(size, sizeof(mus_float_t));
  gen->x1 = (mus_float_t *)calloc(size, sizeof(mus_float_t));
  gen->x2 = (mus_float_t *)calloc(size, sizeof(mus_float_t));
  gen->y0 = (mus_float_t *)calloc(size, sizeof(mus_float_t));
  gen->y1 = (mus_float_t *)calloc(size, sizeof(mus_float_t));
  gen->y2 = (mus_float_t *)calloc(size, sizeof(mus_float_t));
  gen->amps = amps;

  gen->rr = (mus_float_t *)malloc(size * sizeof(mus_float_t));
  gen->fdbk = (mus_float_t *)malloc(size * sizeof(mus_float_t));
  gen->gain = (mus_float_t *)malloc(size * sizeof(mus_float_t));

  if (amps)
    {
      gen->one_input = fb_one_with_amps;
      gen->many_inputs = fb_many_with_amps;
    }
  else
    {
      gen->one_input = fb_one_without_amps;
      gen->many_inputs = fb_many_without_amps;
    }
  
  for (i = 0; i < size; i++)
    {
      frm *g;
      g = (frm *)formants[i];
      gen->rr[i] = g->rr;
      gen->fdbk[i] = g->fdbk;
      gen->gain[i] = g->gain;
      /* one case: 1.0 val 0.0 throughout
       * also c1 x c2
       */
    }
  gen->c1 = gen->rr[0];
  gen->c2 = gen->gain[0];
  for (i = 1; i < size; i++)
    if ((gen->rr[i] != gen->c1) ||
	(gen->gain[i] != gen->c2))
      return((mus_any *)gen);

  if (amps)
    {
      gen->one_input = fb_one_with_amps_c1_c2;
      gen->many_inputs = fb_many_with_amps_c1_c2;
    }
  else
    {
      gen->one_input = fb_one_without_amps_c1_c2;
      gen->many_inputs = fb_many_without_amps_c1_c2;
    }

  return((mus_any *)gen);
}

bool mus_is_formant_bank(mus_any *ptr)
{
  return((ptr) && 
	 (ptr->core->type == MUS_FORMANT_BANK));
}



/* ---------------- firmant ---------------- */

static char *describe_firmant(mus_any *ptr)
{
  frm *gen = (frm *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s frequency: %.3f, radius: %.3f",
	       mus_name(ptr),
	       mus_radians_to_hz(gen->frequency),
	       gen->radius);
  return(describe_buffer);
}

static mus_float_t firmant_frequency(mus_any *ptr) {return(mus_radians_to_hz(((frm *)ptr)->frequency));}

static mus_float_t firmant_set_frequency(mus_any *ptr, mus_float_t freq_in_hz)
{
  frm *gen = (frm *)ptr;
  mus_float_t fw;
  fw = mus_hz_to_radians(freq_in_hz);
  gen->frequency = fw;
  gen->fdbk = 2.0 * sin(gen->frequency * 0.5);
  return(freq_in_hz);
}

static mus_float_t firmant_radius(mus_any *ptr) {return(((frm *)ptr)->radius);}

static mus_float_t firmant_set_radius(mus_any *ptr, mus_float_t radius)
{
  frm *gen = (frm *)ptr;
  gen->radius = radius;
  gen->gain = 1.0 - radius * radius;
  return(radius);
}


bool mus_is_firmant(mus_any *ptr)
{
  return((ptr) && 
	 (ptr->core->type == MUS_FIRMANT));
}


mus_float_t mus_firmant(mus_any *ptr, mus_float_t input)
{
  frm *gen = (frm *)ptr;
  mus_float_t xn1, yn1;
  xn1 = gen->gain * input + gen->radius * (gen->x1         - gen->fdbk * gen->y1);
  yn1 =                     gen->radius * (gen->fdbk * xn1 + gen->y1);
  gen->x1 = xn1;
  gen->y1 = yn1;
  return(yn1);
}


mus_float_t mus_firmant_with_frequency(mus_any *ptr, mus_float_t input, mus_float_t freq_in_radians)
{
  frm *gen = (frm *)ptr;
  gen->frequency = freq_in_radians;
  gen->fdbk = 2.0 * sin(gen->frequency * 0.5);
  return(mus_firmant(ptr, input));
}


static mus_float_t run_firmant(mus_any *ptr, mus_float_t input, mus_float_t unused) {return(mus_firmant(ptr, input));}


static mus_any_class FIRMANT_CLASS = {
  MUS_FIRMANT,
  (char *)S_firmant,
  &free_frm,
  &describe_firmant,
  &frm_equalp,
  0, 0,
  &two_length, 0,
  &firmant_frequency, &firmant_set_frequency,
  0, 0,
  &firmant_radius, &firmant_set_radius,
  0, 0,
  &run_firmant,
  MUS_SIMPLE_FILTER, 
  NULL, 0,
  0, 0, 0, 0,
  0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &frm_reset,
  0, &frm_copy
};


mus_any *mus_make_firmant(mus_float_t frequency, mus_float_t radius)
{
  frm *gen;
  gen = (frm *)calloc(1, sizeof(frm));
  gen->core = &FIRMANT_CLASS;
  gen->frequency = mus_hz_to_radians(frequency);
  gen->radius = radius;
  gen->fdbk = 2.0 * sin(gen->frequency * 0.5);
  gen->gain = 1.0 - radius * radius;
  return((mus_any *)gen);
}




/* ---------------- filter ---------------- */

typedef struct {
  mus_any_class *core;
  int order, allocated_size, loc;
  bool state_allocated;
  mus_float_t *x, *y, *state;
  mus_float_t (*filtw)(mus_any *ptr, mus_float_t fm);
} flt;


mus_float_t mus_filter(mus_any *ptr, mus_float_t input)
{
  return((((flt *)ptr)->filtw)(ptr, input));
}


static mus_float_t filter_eight(mus_any *ptr, mus_float_t input)
{
  /* oddly enough, this separated form is faster than the interleaved version below, or is valgrind confused?
   */
  flt *gen = (flt *)ptr;
  mus_float_t xout;
  mus_float_t *state, *ts, *ts1, *y, *x;

  x = (mus_float_t *)(gen->x);
  y = (mus_float_t *)(gen->y + 1); /* assume y[0] = 1.0 I think */
  state = (mus_float_t *)(gen->state + gen->loc);
  ts = (mus_float_t *)(state + gen->order - 1);
  ts1 = (mus_float_t *)(state + gen->order);

  gen->loc++;
  if (gen->loc == gen->order)
    gen->loc = 0;

  input -= ((*ts--) * (*y++));
  input -= ((*ts--) * (*y++));
  input -= ((*ts--) * (*y++));
  input -= ((*ts--) * (*y++));
  input -= ((*ts--) * (*y++));
  input -= ((*ts--) * (*y++));
  input -= ((*ts--) * (*y++));
  input -= ((*ts) * (*y));

  state[0] = input;
  state[gen->order] = input;

  xout = (*ts1--) * (*x++);
  xout += (*ts1--) * (*x++);
  xout += (*ts1--) * (*x++);
  xout += (*ts1--) * (*x++);
  xout += (*ts1--) * (*x++);
  xout += (*ts1--) * (*x++);
  xout += (*ts1--) * (*x++);
  xout += (*ts1--) * (*x++);
  return(xout + ((*ts1) * (*x)));

  /*
   *    flt *gen = (flt *)ptr;
   *    mus_float_t xout;
   *    mus_float_t *state, *ts, *y, *x;
   *    
   *    x = (mus_float_t *)(gen->x + 1);
   *    y = (mus_float_t *)(gen->y + 1);
   *    state = (mus_float_t *)(gen->state + gen->loc);
   *    ts = (mus_float_t *)(state + gen->order - 1);
   *    
   *    gen->loc++;
   *    if (gen->loc == gen->order)
   *    gen->loc = 0;
   *    
   *    xout = (*ts) * (*x++);
   *    input -= ((*ts--) * (*y++));
   *    xout += (*ts) * (*x++);
   *    input -= ((*ts--) * (*y++));
   *    xout += (*ts) * (*x++);
   *    input -= ((*ts--) * (*y++));
   *    xout += (*ts) * (*x++);
   *    input -= ((*ts--) * (*y++));
   *    xout += (*ts) * (*x++);
   *    input -= ((*ts--) * (*y++));
   *    xout += (*ts) * (*x++);
   *    input -= ((*ts--) * (*y++));
   *    xout += (*ts) * (*x++);
   *    input -= ((*ts--) * (*y++));
   *    xout += (*ts) * (*x);
   *    input -= ((*ts--) * (*y));
   *    
   *    state[0] = input;
   *    state[gen->order] = input;
   *    return(xout + ((*ts) * gen->x[0]));
   */
}


static mus_float_t filter_four(mus_any *ptr, mus_float_t input)
{
  flt *gen = (flt *)ptr;
  mus_float_t xout;
  mus_float_t *state, *ts, *ts1, *y, *x;

  x = (mus_float_t *)(gen->x);
  y = (mus_float_t *)(gen->y + 1);
  state = (mus_float_t *)(gen->state + gen->loc);
  ts = (mus_float_t *)(state + gen->order - 1);
  ts1 = (mus_float_t *)(state + gen->order);

  gen->loc++;
  if (gen->loc == gen->order)
    gen->loc = 0;

  input -= ((*ts--) * (*y++));
  input -= ((*ts--) * (*y++));
  input -= ((*ts--) * (*y++));
  input -= ((*ts) * (*y));

  state[0] = input;
  state[gen->order] = input;

  xout = (*ts1--) * (*x++);
  xout += (*ts1--) * (*x++);
  xout += (*ts1--) * (*x++);
  xout += (*ts1--) * (*x++);
  return(xout + ((*ts1) * (*x)));

  /*
   *    flt *gen = (flt *)ptr;
   *    mus_float_t xout;
   *    mus_float_t *state, *ts, *y, *x;
   *    
   *    x = (mus_float_t *)(gen->x + 1);
   *    y = (mus_float_t *)(gen->y + 1);
   *    state = (mus_float_t *)(gen->state + gen->loc);
   *    ts = (mus_float_t *)(state + gen->order - 1);
   *    
   *    gen->loc++;
   *    if (gen->loc == gen->order)
   *    gen->loc = 0;
   *    
   *    xout = (*ts) * (*x++);
   *    input -= ((*ts--) * (*y++));
   *    xout += (*ts) * (*x++);
   *    input -= ((*ts--) * (*y++));
   *    xout += (*ts) * (*x++);
   *    input -= ((*ts--) * (*y++));
   *    xout += (*ts) * (*x++);
   *    input -= ((*ts--) * (*y++));
   *    
   *    state[0] = input;
   *    state[gen->order] = input;
   *    return(xout + ((*ts) * gen->x[0]));
   */
}


static mus_float_t filter_two(mus_any *ptr, mus_float_t input)
{
  /* here the mus_float_t-delay form is not faster, but use it for consistency */
  flt *gen = (flt *)ptr;
  mus_float_t *state, *ts, *y, *x;

  x = gen->x;
  y = gen->y;
  state = (mus_float_t *)(gen->state + gen->loc);
  ts = (mus_float_t *)(state + gen->order - 2);

  gen->loc++;
  if (gen->loc == gen->order)
    gen->loc = 0;

  state[0] = input - ((ts[1] * y[1]) + (ts[0] * y[2]));
  state[gen->order] = state[0];

  return((ts[0] * x[2]) + (ts[1] * x[1]) + (ts[2] * x[0]));
}


static mus_float_t filter_lt_10(mus_any *ptr, mus_float_t input)
{
  flt *gen = (flt *)ptr;
  mus_float_t xout = 0.0;
  mus_float_t *state, *state1, *ts, *y, *x;

  x = (mus_float_t *)(gen->x);
  y = (mus_float_t *)(gen->y + 1); /* assume y[0] = 1.0 I think */
  state = (mus_float_t *)(gen->state + gen->loc);
  state1 = (mus_float_t *)(state + 1);
  ts = (mus_float_t *)(state + gen->order - 1);

  while (ts > state1)
    input -= ((*ts--) * (*y++));
  input -= ((*ts) * (*y));

  state[0] = input;
  state[gen->order] = input;

  ts = (mus_float_t *)(state + gen->order);

  while (ts > state1)
    xout += (*ts--) * (*x++);

  gen->loc++;
  if (gen->loc == gen->order)
    gen->loc = 0;

  return(xout + ((*ts) * (*x)));
}


static mus_float_t filter_ge_10(mus_any *ptr, mus_float_t input)
{
  flt *gen = (flt *)ptr;
  mus_float_t xout = 0.0;
  mus_float_t *state, *state1, *state11, *ts, *y, *x;

  x = (mus_float_t *)(gen->x);
  y = (mus_float_t *)(gen->y + 1); /* assume y[0] = 1.0 I think */
  state = (mus_float_t *)(gen->state + gen->loc);
  state1 = (mus_float_t *)(state + 1);
  state11 = (mus_float_t *)(state + 11);
  ts = (mus_float_t *)(state + gen->order - 1);

  while (ts >= state11)
    {
      input -= ((*ts--) * (*y++));
      input -= ((*ts--) * (*y++));
      input -= ((*ts--) * (*y++));
      input -= ((*ts--) * (*y++));
      input -= ((*ts--) * (*y++));
      input -= ((*ts--) * (*y++));
      input -= ((*ts--) * (*y++));
      input -= ((*ts--) * (*y++));
      input -= ((*ts--) * (*y++));
      input -= ((*ts--) * (*y++));
    }
  while (ts > state1)
    input -= ((*ts--) * (*y++));
  input -= ((*ts) * (*y));

  state[0] = input;
  state[gen->order] = input;

  ts = (mus_float_t *)(state + gen->order);
  while (ts >= state11)
    {
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
    }
  while (ts > state1)
    xout += (*ts--) * (*x++);

  gen->loc++;
  if (gen->loc == gen->order)
    gen->loc = 0;

  return(xout + ((*ts) * (*x)));
}


mus_float_t mus_fir_filter(mus_any *ptr, mus_float_t input)
{
  return((((flt *)ptr)->filtw)(ptr, input));
}

static inline mus_float_t fir_n(mus_any *ptr, mus_float_t input)
{
  mus_float_t xout = 0.0;
  flt *gen = (flt *)ptr;
  mus_float_t *state, *ts, *x, *end;

  x = (mus_float_t *)(gen->x);
  state = (mus_float_t *)(gen->state + gen->loc);
  ts = (mus_float_t *)(state + gen->order);

  (*state) = input;
  (*ts) = input;
  state++;
  end = (mus_float_t *)(state + 4);

  while (ts > end)
    {
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
    }
  while (ts > state)
    xout += (*ts--) * (*x++);

  gen->loc++;
  if (gen->loc == gen->order)
    gen->loc = 0;

  return(xout + ((*ts) * (*x)));
}
 

static inline mus_float_t fir_3(mus_any *ptr, mus_float_t input)
{
  mus_float_t xout;
  flt *gen = (flt *)ptr;
  mus_float_t *state, *ts, *x;

  x = (mus_float_t *)(gen->x);
  state = (mus_float_t *)(gen->state + gen->loc);
  ts = (mus_float_t *)(state + 4); /* gen->order == 4 in this case */

  /* gen->loc = (gen->loc == 3) ? 0 : (gen->loc + 1); */
  gen->loc = (gen->loc + 1) & 0x3;
  (*state) = input;
  (*ts) = input;

  xout = (*ts--) * (*x++);
  xout += (*ts--) * (*x++);
  xout += (*ts--) * (*x++);
  return(xout + ((*ts) * (*x)));
}

static inline mus_float_t fir_4(mus_any *ptr, mus_float_t input)
{
  mus_float_t xout;
  flt *gen = (flt *)ptr;
  mus_float_t *state, *ts, *x;

  x = (mus_float_t *)(gen->x);
  state = (mus_float_t *)(gen->state + gen->loc);
  ts = (mus_float_t *)(state + 5);

  gen->loc = (gen->loc == 4) ? 0 : (gen->loc + 1);
  (*state) = input;
  (*ts) = input;

  xout = (*ts--) * (*x++);
  xout += (*ts--) * (*x++);
  xout += (*ts--) * (*x++);
  xout += (*ts--) * (*x++);
  return(xout + ((*ts) * (*x)));
}
 
static inline mus_float_t fir_9(mus_any *ptr, mus_float_t input)
{
  mus_float_t xout;
  flt *gen = (flt *)ptr;
  mus_float_t *state, *ts, *x;

  x = (mus_float_t *)(gen->x);
  state = (mus_float_t *)(gen->state + gen->loc);
  ts = (mus_float_t *)(state + 10);

  gen->loc = (gen->loc == 9) ? 0 : (gen->loc + 1);
  (*state) = input;
  (*ts) = input;

  xout = (*ts--) * (*x++);
  xout += (*ts--) * (*x++);
  xout += (*ts--) * (*x++);
  xout += (*ts--) * (*x++);
  xout += (*ts--) * (*x++);
  xout += (*ts--) * (*x++);
  xout += (*ts--) * (*x++);
  xout += (*ts--) * (*x++);
  xout += (*ts--) * (*x++);
  return(xout + ((*ts) * (*x)));
}


static mus_float_t fir_ge_20(mus_any *ptr, mus_float_t input)
{
  mus_float_t xout = 0.0;
  flt *gen = (flt *)ptr;
  mus_float_t *state, *ts, *x, *end;

  x = (mus_float_t *)(gen->x);
  state = (mus_float_t *)(gen->state + gen->loc);
  ts = (mus_float_t *)(state + gen->order);
  end = (mus_float_t *)(state + 20);

  (*state) = input;
  (*ts) = input;
  state++;

  while (ts >= end)
    {
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
      xout += (*ts--) * (*x++);
    }
  while (ts > state)
    xout += (*ts--) * (*x++);

  gen->loc++;
  if (gen->loc == gen->order)
    gen->loc = 0;

  return((ts == state) ? (xout + ((*ts) * (*x))) : xout); 
}


mus_float_t mus_iir_filter(mus_any *ptr, mus_float_t input)
{
  return((((flt *)ptr)->filtw)(ptr, input));
}


static mus_float_t iir_n(mus_any *ptr, mus_float_t input)
{
  flt *gen = (flt *)ptr;
  mus_float_t *state, *ts, *y;

  y = (mus_float_t *)(gen->y + 1); /* assume y[0] = 1.0 I think */
  state = (mus_float_t *)(gen->state + gen->loc);
  ts = (mus_float_t *)(state + gen->order - 1);

  while (ts > state)
    input -= ((*ts--) * (*y++));

  gen->loc++;
  if (gen->loc == gen->order)
    gen->loc = 0;

  state[0] = input;
  state[gen->order] = input;

  return(input);
}


static mus_float_t run_filter(mus_any *ptr, mus_float_t input, mus_float_t unused) 
{
  return((((flt *)ptr)->filtw)(ptr, input));
}

bool mus_is_filter(mus_any *ptr) 
{
  return((ptr) && 
	 ((ptr->core->type == MUS_FILTER) || 
	  (ptr->core->type == MUS_FIR_FILTER) ||
	  (ptr->core->type == MUS_IIR_FILTER)));
}


bool mus_is_fir_filter(mus_any *ptr) 
{
  return((ptr) &&
	 (ptr->core->type == MUS_FIR_FILTER));
}

bool mus_is_iir_filter(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_IIR_FILTER));
}


static mus_float_t *filter_data(mus_any *ptr) {return(((flt *)ptr)->state);}
static mus_long_t filter_length(mus_any *ptr) {return(((flt *)ptr)->order);}

static mus_float_t *filter_xcoeffs(mus_any *ptr) {return(((flt *)ptr)->x);}
static mus_float_t *filter_ycoeffs(mus_any *ptr) {return(((flt *)ptr)->y);}


mus_float_t *mus_filter_set_xcoeffs(mus_any *ptr, mus_float_t *new_data)
{
  /* needed by Snd if filter order increased during play */
  flt *gen = (flt *)ptr;
  mus_float_t *old_data;
  old_data = gen->x;
  gen->x = new_data;
  return(old_data);
}


mus_float_t *mus_filter_set_ycoeffs(mus_any *ptr, mus_float_t *new_data)
{
  flt *gen = (flt *)ptr;
  mus_float_t *old_data;
  old_data = gen->y;
  gen->y = new_data;
  return(old_data);
}


static mus_long_t filter_set_length(mus_any *ptr, mus_long_t val) 
{
  /* just resets order if order < allocated size */
  flt *gen = (flt *)ptr;
  if ((val > 0) && (val <= gen->allocated_size))
    gen->order = (int)val;
  return((mus_long_t)(gen->order));
}


static void set_filter_function(flt *gen);

int mus_filter_set_order(mus_any *ptr, int order)
{
  /* resets order and fixes state array if needed (coeffs arrays should be handled separately by set_x|ycoeffs above) */
  /*   returns either old order or -1 if state array can't be reallocated */
  flt *gen = (flt *)ptr;
  int old_order;
  if ((order > gen->allocated_size) &&
      (!(gen->state_allocated)))
    return(-1);
  old_order = gen->order;
  gen->order = order;
  if (order > gen->allocated_size)
    {
      int i;
      gen->allocated_size = order;
      gen->state = (mus_float_t *)realloc(gen->state, order * 2 * sizeof(mus_float_t));
      for (i = old_order; i < order; i++)
	{
	  gen->state[i] = 0.0;         /* try to minimize click */
	  gen->state[i + order] = 0.0; /* just a guess */
	}
    }
  set_filter_function(gen);
  return(old_order);
}


static mus_float_t filter_xcoeff(mus_any *ptr, int index) 
{
  flt *gen = (flt *)ptr;
  if (!(gen->x)) return((mus_float_t)mus_error(MUS_NO_XCOEFFS, S_mus_xcoeff ": no xcoeffs"));
  if ((index >= 0) && (index < gen->order))
    return(gen->x[index]);
  return((mus_float_t)mus_error(MUS_ARG_OUT_OF_RANGE, S_mus_xcoeff ": invalid index %d, order = %d?", index, gen->order));
}


static mus_float_t filter_set_xcoeff(mus_any *ptr, int index, mus_float_t val) 
{
  flt *gen = (flt *)ptr;
  if (!(gen->x)) return((mus_float_t)mus_error(MUS_NO_XCOEFFS, S_set S_mus_xcoeff ": no xcoeffs"));
  if ((index >= 0) && (index < gen->order))
    {
      gen->x[index] = val;
      return(val);
    }
  return((mus_float_t)mus_error(MUS_ARG_OUT_OF_RANGE, S_set S_mus_xcoeff ": invalid index %d, order = %d?", index, gen->order));
}


static mus_float_t filter_ycoeff(mus_any *ptr, int index) 
{
  flt *gen = (flt *)ptr;
  if (!(gen->y)) return((mus_float_t)mus_error(MUS_NO_YCOEFFS, S_mus_ycoeff ": no ycoeffs"));
  if ((index >= 0) && (index < gen->order))
    return(gen->y[index]);
  return((mus_float_t)mus_error(MUS_ARG_OUT_OF_RANGE, S_mus_ycoeff ": invalid index %d, order = %d?", index, gen->order));
}


static mus_float_t filter_set_ycoeff(mus_any *ptr, int index, mus_float_t val) 
{
  flt *gen = (flt *)ptr;
  if (!(gen->y)) return((mus_float_t)mus_error(MUS_NO_YCOEFFS, S_set S_mus_ycoeff ": no ycoeffs"));
  if ((index >= 0) && (index < gen->order))
    {
      gen->y[index] = val;
      return(val);
    }
  return((mus_float_t)mus_error(MUS_ARG_OUT_OF_RANGE, S_set S_mus_ycoeff ": invalid index %d, order = %d?", index, gen->order));
}


static void free_filter(mus_any *ptr)
{
  flt *gen = (flt *)ptr;
  if ((gen->state) && (gen->state_allocated)) free(gen->state);
  free(gen);
}

static mus_any *flt_copy(mus_any *ptr)
{
  flt *g, *p;

  p = (flt *)ptr;
  g = (flt *)malloc(sizeof(flt));
  memcpy((void *)g, (void *)ptr, sizeof(flt));

  /* we have to make a new state array -- otherwise the original and copy step on each other */
  g->state_allocated = true;
  g->state = (mus_float_t *)malloc(p->order * 2 * sizeof(mus_float_t));
  mus_copy_floats(g->state, p->state, p->order * 2);
  return((mus_any *)g);
}


static bool filter_equalp(mus_any *p1, mus_any *p2) 
{
  flt *f1, *f2;
  f1 = (flt *)p1;
  f2 = (flt *)p2;
  if (p1 == p2) return(true);
  return(((p1->core)->type == (p2->core)->type) &&
	 ((mus_is_filter(p1)) || (mus_is_fir_filter(p1)) || (mus_is_iir_filter(p1))) &&
	 (f1->order == f2->order) &&
	 ((!(f1->x)) || (!(f2->x)) || (clm_arrays_are_equal(f1->x, f2->x, f1->order))) &&
	 ((!(f1->y)) || (!(f2->y)) || (clm_arrays_are_equal(f1->y, f2->y, f1->order))) &&
	 (clm_arrays_are_equal(f1->state, f2->state, f1->order)));
}


static char *describe_filter(mus_any *ptr)
{
  flt *gen = (flt *)ptr;
  char *xstr, *ystr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  xstr = float_array_to_string(gen->x, gen->order, 0);
  ystr = float_array_to_string(gen->y, gen->order, 0);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s order: %d, xs: %s, ys: %s", 
	       mus_name(ptr),
	       gen->order,
	       xstr, ystr);
  if (xstr) free(xstr);
  if (ystr) free(ystr);
  return(describe_buffer);
}


static char *describe_fir_filter(mus_any *ptr)
{
  flt *gen = (flt *)ptr;
  char *xstr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  xstr = float_array_to_string(gen->x, gen->order, 0);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s order: %d, xs: %s", 
	       mus_name(ptr),
	       gen->order,
	       xstr);
  if (xstr) free(xstr);
  return(describe_buffer);
}


static char *describe_iir_filter(mus_any *ptr)
{
  flt *gen = (flt *)ptr;
  char *ystr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  ystr = float_array_to_string(gen->y, gen->order, 0);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s order: %d, ys: %s", 
	       mus_name(ptr),
	       gen->order,
	       ystr);
  if (ystr) free(ystr);
  return(describe_buffer);
}


static void filter_reset(mus_any *ptr)
{
  flt *gen = (flt *)ptr;
  mus_clear_floats(gen->state, gen->allocated_size * 2);
}


static mus_any_class FILTER_CLASS = {
  MUS_FILTER,
  (char *)S_filter,
  &free_filter,
  &describe_filter,
  &filter_equalp,
  &filter_data, 0,
  &filter_length,
  &filter_set_length,
  0, 0, 0, 0,
  0, 0,
  0, 0,
  &run_filter,
  MUS_FULL_FILTER, 
  NULL, 0,
  0, 0, 0, 0, 
  &filter_xcoeff, &filter_set_xcoeff, 
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  &filter_ycoeff, &filter_set_ycoeff, 
  &filter_xcoeffs, &filter_ycoeffs, 
  &filter_reset,
  0, &flt_copy
};


static mus_any_class FIR_FILTER_CLASS = {
  MUS_FIR_FILTER,
  (char *)S_fir_filter,
  &free_filter,
  &describe_fir_filter,
  &filter_equalp,
  &filter_data, 0,
  &filter_length,
  &filter_set_length,
  0, 0, 0, 0,
  0, 0,
  0, 0,
  &run_filter,
  MUS_FULL_FILTER, 
  NULL, 0,
  0, 0, 0, 0, 
  &filter_xcoeff, &filter_set_xcoeff, 
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 
  &filter_xcoeffs, 0,
  &filter_reset,
  0, &flt_copy
};


static mus_any_class IIR_FILTER_CLASS = {
  MUS_IIR_FILTER,
  (char *)S_iir_filter,
  &free_filter,
  &describe_iir_filter,
  &filter_equalp,
  &filter_data, 0,
  &filter_length,
  &filter_set_length,
  0, 0, 0, 0,
  0, 0,
  0, 0,
  &run_filter,
  MUS_FULL_FILTER, 
  NULL, 0,
  0, 0, 0, 0, 
  0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  &filter_ycoeff, &filter_set_ycoeff, 
  0, &filter_ycoeffs,
  &filter_reset,
  0, &flt_copy
};


static void set_filter_function(flt *gen)
{
  /* choose the run-time function based on the current filter order and type */
  int order;
  order = gen->order - 1;
  if (gen->core == &FILTER_CLASS)
    {
      if (order == 2)
	gen->filtw = filter_two;
      else
	{
	  if (order == 8)
	    gen->filtw = filter_eight;
	  else
	    {
	      if (order == 4)
		gen->filtw = filter_four;
	      else 
		{
		  if (order >= 10)
		    gen->filtw = filter_ge_10;
		  else gen->filtw = filter_lt_10;
		}}}
    }
  else
    {
      if (gen->core == &FIR_FILTER_CLASS)
	{
	  if (order >= 20)
	    gen->filtw = fir_ge_20;
	  else 
	    {
	      if (order == 3)
		gen->filtw = fir_3;
	      else
		{
		  if (order == 4)
		    gen->filtw = fir_4;
		  else
		    {
		      if (order == 9)
			gen->filtw = fir_9;
		      else gen->filtw = fir_n;
		      }}}}
      else gen->filtw = iir_n;
    }
}


static mus_any *make_filter(mus_any_class *cls, const char *name, int order, mus_float_t *xcoeffs, mus_float_t *ycoeffs, mus_float_t *state) 
{
  /* if state is null, it is allocated locally, otherwise it's size should be at least 2 * order.
   */
  if (order <= 0)
    mus_error(MUS_ARG_OUT_OF_RANGE, S_make_filter ": %s order = %d?", name, order);
  else
    {
      flt *gen;
      gen = (flt *)malloc(sizeof(flt));
      if (state)
	{
	  gen->state = state;
	  gen->state_allocated = false;
	}
      else 
	{
	  gen->state = (mus_float_t *)calloc(order * 2, sizeof(mus_float_t));
	  gen->state_allocated = true;
	}
      gen->loc = 0;

      if (cls == &FILTER_CLASS)
	{
	  if (!ycoeffs)
	    cls = &FIR_FILTER_CLASS;
	  else 
	    {
	      if (!xcoeffs)
		cls = &IIR_FILTER_CLASS;
	    }
	}
      gen->core = cls;
      gen->order = order;
      gen->allocated_size = order;
      gen->x = xcoeffs;
      gen->y = ycoeffs;
      gen->filtw = NULL;
      set_filter_function(gen);
      return((mus_any *)gen);
    }
  return(NULL);
}


mus_any *mus_make_filter(int order, mus_float_t *xcoeffs, mus_float_t *ycoeffs, mus_float_t *state)
{
  return(make_filter(&FILTER_CLASS, S_make_filter, order, xcoeffs, ycoeffs, state));
}


mus_any *mus_make_fir_filter(int order, mus_float_t *xcoeffs, mus_float_t *state)
{
  return(make_filter(&FIR_FILTER_CLASS, S_make_fir_filter, order, xcoeffs, NULL, state));
}


mus_any *mus_make_iir_filter(int order, mus_float_t *ycoeffs, mus_float_t *state)
{
  return(make_filter(&IIR_FILTER_CLASS, S_make_iir_filter, order, NULL, ycoeffs, state));
}


mus_float_t *mus_make_fir_coeffs(int order, mus_float_t *envl, mus_float_t *aa)
{
  /* envl = evenly sampled freq response, has order samples */
  int n, i, j, jj;
  mus_float_t scl;
  mus_float_t *a;

  n = order;
  if (n <= 0) return(aa);
  if (aa) 
    a = aa;
  else a = (mus_float_t *)calloc(order + 1, sizeof(mus_float_t));
  if (!a) return(NULL);
  if (!(is_power_of_2(order)))
    {
      int m;
      mus_float_t am, q, xt0, x;
      m = (n + 1) / 2;
      am = 0.5 * (n + 1) - 1.0;
      scl = 2.0 / (mus_float_t)n;
      q = TWO_PI / (mus_float_t)n;
      xt0 = envl[0] * 0.5;
      for (j = 0, jj = n - 1; j < m; j++, jj--)
	{
	  mus_float_t xt, qj;
#if HAVE_SINCOS
	  double s1, c1, s2, c2, qj1;
	  xt = xt0;
	  qj = q * (am - j);
	  sincos(qj, &s1, &c1);
	  qj1 = qj * 2.0;
	  for (i = 1, x = qj; i < m; i += 2, x += qj1)
	    {
	      sincos(x, &s2, &c2);
	      xt += (envl[i] * c2);
	      if (i < (m - 1))
		xt += (envl[i + 1] * (c1 * c2 - s1 * s2));
	    }
#else
	  xt = xt0;
	  qj = q * (am - j);
	  for (i = 1, x = qj; i < m; i++, x += qj)
	    xt += (envl[i] * cos(x));
#endif
	  a[j] = xt * scl;
	  a[jj] = a[j];
	}
    }
  else /* use fft if it's easy to match -- there must be a way to handle non-power-of-2 orders here 
	*   stretch envl to a power of 2, fft, subsample?
	*/
    {
      mus_float_t *rl, *im;
      mus_long_t fsize; 
      int lim;
      mus_float_t offset;

      fsize = 2 * order; /* checked power of 2 above */
      rl = (mus_float_t *)calloc(fsize, sizeof(mus_float_t));
      im = (mus_float_t *)calloc(fsize, sizeof(mus_float_t));
      lim = order / 2;
      mus_copy_floats(rl, envl, lim);

      mus_fft(rl, im, fsize, 1);

      scl = 4.0 / fsize;
      offset = -2.0 * envl[0] / fsize;
      for (i = 0; i < fsize; i++) 
	rl[i] = rl[i] * scl + offset;
      for (i = 1, j = lim - 1, jj = lim; i < order; i += 2, j--, jj++) 
	{
	  a[j] = rl[i]; 
	  a[jj] = rl[i];
	}
      free(rl);
      free(im);
    }

  return(a);
}



/* ---------------- one-pole-all-pass ---------------- */

typedef struct {
  mus_any_class *core;
  int size;
  mus_float_t coeff;
  mus_float_t *x, *y;
  mus_float_t (*f)(mus_any *ptr, mus_float_t input);
} onepall;


static void free_onepall(mus_any *ptr) 
{
  onepall *f = (onepall *)ptr;
  if (f->x) {free(f->x); f->x = NULL;}
  if (f->y) {free(f->y); f->y = NULL;}
  free(ptr); 
}

static mus_any *onepall_copy(mus_any *ptr)
{
  onepall *g, *p;
  int bytes;

  p = (onepall *)ptr;
  g = (onepall *)malloc(sizeof(onepall));
  memcpy((void *)g, (void *)ptr, sizeof(onepall));

  bytes = g->size * sizeof(mus_float_t);
  g->x = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->x, p->x, g->size);
  g->y = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->y, p->y, g->size);

  return((mus_any *)g);
}


static mus_float_t run_onepall(mus_any *ptr, mus_float_t input, mus_float_t unused) 
{
  return((((onepall *)ptr)->f)(ptr, input));
}


static mus_long_t onepall_length(mus_any *ptr) {return(((onepall *)ptr)->size);}
static mus_float_t onepall_scaler(mus_any *ptr) {return(((onepall *)ptr)->coeff);}

static void onepall_reset(mus_any *ptr)
{
  onepall *f = (onepall *)ptr;
  mus_clear_floats(f->x, f->size);
  mus_clear_floats(f->y, f->size);
}


static bool onepall_equalp(mus_any *p1, mus_any *p2)
{
  onepall *f1 = (onepall *)p1;
  onepall *f2 = (onepall *)p2;

  if (f1 == f2) return(true);
  if (f1->size != f2->size) return(false);
  if (f1->coeff != f2->coeff) return(false);

  return((mus_arrays_are_equal(f1->x, f2->x, float_equal_fudge_factor, f1->size)) &&
	 (mus_arrays_are_equal(f1->y, f2->y, float_equal_fudge_factor, f1->size)));
}


static char *describe_onepall(mus_any *ptr)
{
  onepall *gen = (onepall *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s size: %d, coeff: %f",
	       mus_name(ptr),
	       gen->size,
	       gen->coeff);
  return(describe_buffer);
}


mus_float_t mus_one_pole_all_pass(mus_any *ptr, mus_float_t input)
{
  return((((onepall *)ptr)->f)(ptr, input));
}

static mus_float_t one_pole_all_pass_n(mus_any *f, mus_float_t input)
{
  onepall *p = (onepall *)f;
  int i;
  mus_float_t coeff, y0;
  mus_float_t *x, *y;

  x = p->x;
  y = p->y;
  coeff = p->coeff;

  y0 = input;
  for (i = 0; i < p->size; i++)
    {
      y[i] = x[i] + (coeff * (y0 - y[i]));
      x[i] = y0;
      y0 = y[i];
    }
  return(y0);
}

static mus_float_t one_pole_all_pass_8(mus_any *f, mus_float_t input)
{
  onepall *p = (onepall *)f;
  mus_float_t coeff;
  mus_float_t *x, *y;

  x = p->x;
  y = p->y;
  coeff = p->coeff;

  y[0] = x[0] + (coeff * (input - y[0])); x[0] = input;
  y[1] = x[1] + (coeff * (y[0] - y[1])); x[1] = y[0]; 
  y[2] = x[2] + (coeff * (y[1] - y[2])); x[2] = y[1]; 
  y[3] = x[3] + (coeff * (y[2] - y[3])); x[3] = y[2]; 
  y[4] = x[4] + (coeff * (y[3] - y[4])); x[4] = y[3]; 
  y[5] = x[5] + (coeff * (y[4] - y[5])); x[5] = y[4]; 
  y[6] = x[6] + (coeff * (y[5] - y[6])); x[6] = y[5]; 
  y[7] = x[7] + (coeff * (y[6] - y[7])); x[7] = y[6];

  return(y[7]);
}

static mus_float_t one_pole_all_pass_1(mus_any *f, mus_float_t input)
{
  onepall *p = (onepall *)f;

  p->y[0] = p->x[0] + (p->coeff * (input - p->y[0]));
  p->x[0] = input;
  return(p->y[0]);
}


static mus_any_class ONE_POLE_ALL_PASS_CLASS = {
  MUS_ONE_POLE_ALL_PASS,
  (char *)S_one_pole_all_pass,
  &free_onepall,
  &describe_onepall,
  &onepall_equalp,
  0, 0,
  &onepall_length, 0,
  0, 0, 
  0, 0,
  &onepall_scaler, 0,
  0, 0,
  &run_onepall,
  MUS_NOT_SPECIAL, 
  NULL, 0,
  0, 0, 0, 0,
  0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &onepall_reset,
  0, &onepall_copy
};


mus_any *mus_make_one_pole_all_pass(int size, mus_float_t coeff)
{
  onepall *gen;

  gen = (onepall *)malloc(sizeof(onepall));
  gen->core = &ONE_POLE_ALL_PASS_CLASS;
  gen->size = size;

  gen->x = (mus_float_t *)calloc(size, sizeof(mus_float_t));
  gen->y = (mus_float_t *)calloc(size, sizeof(mus_float_t));
  gen->coeff = coeff;

  if (size == 1)
    gen->f = one_pole_all_pass_1;
  else
    {
      if (size == 8)
	gen->f = one_pole_all_pass_8;
      else gen->f = one_pole_all_pass_n;
    }

  return((mus_any *)gen);
}


bool mus_is_one_pole_all_pass(mus_any *ptr)
{
  return((ptr) && 
	 (ptr->core->type == MUS_ONE_POLE_ALL_PASS));
}
  


/* ---------------- env ---------------- */

typedef enum {MUS_ENV_LINEAR, MUS_ENV_EXPONENTIAL, MUS_ENV_STEP} mus_env_t;

typedef struct {
  mus_any_class *core;
  mus_float_t rate, current_value, base, offset, scaler, power, init_y, init_power, original_scaler, original_offset;
  mus_long_t loc, end;
  mus_env_t style;
  int index, size;
  mus_float_t *original_data;
  mus_float_t *rates;
  mus_long_t *locs;
  mus_float_t (*env_func)(mus_any *g);
  void *next;
  void (*free_env)(mus_any *ptr);
} seg;

/* I used to use exp directly, but:

    (define (texp1 start end num)
      (let* ((ls (log start))
	     (le (log end))
	     (cf (exp (/ (- le ls) (1- num))))
	     (max-diff 0.0)
	     (xstart start))
        (do ((i 0 (+ i 1)))
	    ((= i num) 
	     max-diff)
          (let ((val1 (* start (exp (* (/ i (1- num)) (- le ls)))))
	        (val2 xstart))
	    (set! xstart (* xstart cf))
	    (set! max-diff (max max-diff (abs (- val1 val2))))))))
      
    returns:

    :(texp1 1.0 3.0 1000000)
    2.65991229042584e-10
    :(texp1 1.0 10.0 100000000)
    2.24604939091932e-8
    :(texp1 10.0 1000.0 100000000)
    4.11786902532185e-6
    :(texp1 1.0 1.1 100000000)
    1.28246036013024e-9
    :(texp1 10.0 1000.0 1000000000)
    4.39423240550241e-5

    so the repeated multiply version is more than accurate enough
*/


bool mus_is_env(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_ENV));
}


mus_float_t mus_env(mus_any *ptr)
{
  seg *gen = (seg *)ptr;
  return((*(gen->env_func))(ptr));
}


mus_float_t (*mus_env_function(mus_any *g))(mus_any *gen)
{
  if (mus_is_env(g))
    return(((seg *)g)->env_func);
  return(NULL);
}


static mus_float_t mus_env_step(mus_any *ptr)
{
  seg *gen = (seg *)ptr;
  mus_float_t val;
  val = gen->current_value;
  if (gen->loc == 0)
    {
      gen->index++;
      gen->loc = gen->locs[gen->index] - gen->locs[gen->index - 1];
      gen->rate = gen->rates[gen->index];
      gen->current_value = gen->rate; 
    }
  gen->loc--;
  return(val);
}


static mus_float_t mus_env_line(mus_any *ptr)
{
  seg *gen = (seg *)ptr;
  return(gen->current_value);
}


static mus_float_t mus_env_linear(mus_any *ptr)
{
  seg *gen = (seg *)ptr;
  mus_float_t val;

  val = gen->current_value;
  if (gen->loc == 0)
    {
      /* we can save about 10% total env time by checking here that we're on the last segment,
       *   and setting gen->env_func to a version of mus_env_linear that does not watch gen->loc.
       * In any case, this code is strange -- change anything and it is 20% slower, sez callgrind.
       */
      gen->index++;
      gen->loc = gen->locs[gen->index] - gen->locs[gen->index - 1];
      gen->rate = gen->rates[gen->index];
    }
  gen->current_value += gen->rate; 
  gen->loc--;
  return(val);
}


static mus_float_t mus_env_exponential(mus_any *ptr)
{
  seg *gen = (seg *)ptr;
  mus_float_t val;
  val = gen->current_value;
  if (gen->loc == 0)
    {
      gen->index++;
      gen->loc = gen->locs[gen->index] - gen->locs[gen->index - 1];
      gen->rate = gen->rates[gen->index];
    }
  gen->power *= gen->rate;
  gen->current_value = gen->offset + (gen->scaler * gen->power);
  gen->loc--;
  return(val);
}


static mus_float_t run_env(mus_any *ptr, mus_float_t unused1, mus_float_t unused2) 
{
  return(mus_env(ptr));
}


static void canonicalize_env(seg *e, const mus_float_t *data, int pts, mus_long_t dur, mus_float_t scaler)
{ 
  int i, j, pts2;
  mus_float_t xscl, cur_loc, x1, y1, xdur;
  mus_long_t samps, pre_loc;

  /* pts > 1 if we get here, so the loop below is always exercised */

  pts2 = pts * 2;
  xdur = data[pts2 - 2] - data[0];
  if (xdur > 0.0)
    xscl = (mus_float_t)(dur - 1) / xdur;
  else xscl = 1.0;
  e->locs[pts - 2] = e->end;

  x1 = data[0];
  y1 = data[1];
  pre_loc = 0;

  for (j = 0, i = 2, cur_loc = 0.0; i < pts2; i += 2, j++)
    {
      mus_float_t cur_dx, x0, y0;
      x0 = x1;
      x1 = data[i];
      y0 = y1;
      y1 = data[i + 1];

      cur_dx = xscl * (x1 - x0);
      if (cur_dx < 1.0)
	cur_loc += 1.0;
      else cur_loc += cur_dx;

      switch (e->style)
	{
	case MUS_ENV_LINEAR:
	  e->locs[j] = (mus_long_t)(cur_loc + 0.5);
	  samps = e->locs[j] - pre_loc;
	  pre_loc = e->locs[j];

	  if (samps == 0)
	    e->rates[j] = 0.0;
	  else e->rates[j] = scaler * (y1 - y0) / (mus_float_t)samps;
	  break;

	case MUS_ENV_EXPONENTIAL:
	  e->locs[j] = (mus_long_t)(cur_loc + 0.5);
	  samps = e->locs[j] - pre_loc;
	  pre_loc = e->locs[j];

	  if (samps == 0)
	    e->rates[j] = 1.0;
	  else e->rates[j] = exp((y1 - y0) / (mus_float_t)samps);
	  break;

	case MUS_ENV_STEP:
	  e->locs[j] = (mus_long_t)cur_loc;               /* this is the change boundary (confusing...) */  
	  e->rates[j] = e->offset + (scaler * y0);
	  break;
	}
    }

  e->locs[pts - 1] = 1000000000;
  e->locs[pts] = 1000000000; /* guard cell at end to make bounds check simpler */
}


static mus_float_t *fixup_exp_env(seg *e, const mus_float_t *data, int pts, mus_float_t offset, mus_float_t scaler, mus_float_t base)
{
  mus_float_t min_y, max_y, val = 0.0, tmp = 0.0, b1;
  int len, i;
  bool flat;
  mus_float_t *result = NULL;

  if ((base <= 0.0) || 
      (base == 1.0)) 
    return(NULL);

  min_y = offset + scaler * data[1];
  max_y = min_y;
  len = pts * 2;

  /* fill "result" with x and (offset+scaler*y) */

  result = (mus_float_t *)malloc(len * sizeof(mus_float_t));
  result[0] = data[0];
  result[1] = min_y;

  for (i = 2; i < len; i += 2)
    {
      tmp = offset + scaler * data[i + 1];
      result[i] = data[i];
      result[i + 1] = tmp;
      if (tmp < min_y) min_y = tmp;
      if (tmp > max_y) max_y = tmp;
    }

  b1 = base - 1.0;
  flat = (min_y == max_y);
  if (!flat) 
    val = 1.0 / (max_y - min_y);

  /* now logify result */
  for (i = 1; i < len; i += 2)
    {
      if (flat) 
	tmp = 1.0;
      else tmp = val * (result[i] - min_y);
      result[i] = log(1.0 + (tmp * b1));
    }

  e->scaler = (max_y - min_y) / b1;
  e->offset = min_y;
  return(result);
}


static bool env_equalp(mus_any *p1, mus_any *p2)
{
  seg *e1 = (seg *)p1;
  seg *e2 = (seg *)p2;
  if (p1 == p2) return(true);
  return((e1) && (e2) &&
	 (e1->core->type == e2->core->type) &&
	 (e1->loc == e2->loc) &&
	 (e1->end == e2->end) &&
	 (e1->style == e2->style) &&
	 (e1->index == e2->index) &&
	 (e1->size == e2->size) &&

	 (e1->rate == e2->rate) &&
	 (e1->base == e2->base) &&
	 (e1->power == e2->power) &&
	 (e1->current_value == e2->current_value) &&
	 (e1->scaler == e2->scaler) &&
	 (e1->offset == e2->offset) &&
	 (e1->init_y == e2->init_y) &&
	 (e1->init_power == e2->init_power) &&
	 (clm_arrays_are_equal(e1->original_data, e2->original_data, e1->size * 2)));
}


static char *describe_env(mus_any *ptr)
{
  char *str = NULL;
  seg *e = (seg *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s %s, pass: %" print_mus_long " (dur: %" print_mus_long "), index: %d, scaler: %.4f, offset: %.4f, data: %s",
	   mus_name(ptr),
	   ((e->style == MUS_ENV_LINEAR) ? "linear" : ((e->style == MUS_ENV_EXPONENTIAL) ? "exponential" : "step")),
	   (e->locs) ? (e->locs[e->index] - e->loc) : -1,
	   e->end + 1, 
	   e->index,
	   e->original_scaler, 
	   e->original_offset,
	   str = float_array_to_string(e->original_data, e->size * 2, 0));
  if (str) free(str);
  return(describe_buffer);
}


static seg *e2_free_list = NULL, *e3_free_list = NULL, *e4_free_list = NULL;

static void free_env_gen(mus_any *pt) 
{
  seg *ptr = (seg *)pt;
  (*(ptr->free_env))(pt);
}

static void fe2(mus_any *pt) 
{
  seg *ptr = (seg *)pt;
  ptr->next = e2_free_list;
  e2_free_list = ptr;
}

static void fe3(mus_any *pt) 
{
  seg *ptr = (seg *)pt;
  ptr->next = e3_free_list;
  e3_free_list = ptr;
}

static void fe4(mus_any *pt) 
{
  seg *ptr = (seg *)pt;
  ptr->next = e4_free_list;
  e4_free_list = ptr;
}

static void ferest(mus_any *pt) 
{
  seg *ptr = (seg *)pt;
  if (ptr->locs) {free(ptr->locs); ptr->locs = NULL;}
  if (ptr->rates) {free(ptr->rates); ptr->rates = NULL;}
  free(ptr); 
}

static mus_any *seg_copy(mus_any *ptr)
{
  seg *e = NULL, *p;
  p = (seg *)ptr;

  switch (p->size) /* "npts" */
    {
    case 1:
      e = (seg *)malloc(sizeof(seg));
      memcpy((void *)e, (void *)ptr, sizeof(seg));
      return((mus_any *)e);

    case 2: if (e2_free_list) {e = e2_free_list; e2_free_list = (seg *)(e->next);} break;
    case 3: if (e3_free_list) {e = e3_free_list; e3_free_list = (seg *)(e->next);} break;
    case 4: if (e4_free_list) {e = e4_free_list; e4_free_list = (seg *)(e->next);} break;
    default: break;
    }

  if (!e)
    {
      e = (seg *)malloc(sizeof(seg));
      memcpy((void *)e, (void *)ptr, sizeof(seg));
      if (p->rates)
	{
	  int bytes;
	  e->rates = (mus_float_t *)malloc(p->size * sizeof(mus_float_t));
	  mus_copy_floats(e->rates, p->rates, p->size);

	  bytes = (p->size + 1) * sizeof(mus_long_t);
	  e->locs = (mus_long_t *)malloc(bytes);
	  memcpy((void *)(e->locs), (void *)(p->locs), bytes);
	}
    }
  else
    {
      mus_float_t *r;
      mus_long_t *l;
      int bytes;

      bytes = p->size * sizeof(mus_float_t);
      r = e->rates;
      mus_copy_floats(r, p->rates, p->size);

      bytes = (p->size + 1) * sizeof(mus_long_t);
      l = e->locs;
      memcpy((void *)l, (void *)(p->locs), bytes);

      memcpy((void *)e, (void *)ptr, sizeof(seg));
      e->rates = r;
      e->locs = l;
    }
  return((mus_any *)e);
}

static mus_float_t *env_data(mus_any *ptr) {return(((seg *)ptr)->original_data);}    /* mus-data */

static mus_float_t env_scaler(mus_any *ptr) {return(((seg *)ptr)->original_scaler);} /* "mus_float_t" for mus-scaler */

static mus_float_t env_offset(mus_any *ptr) {return(((seg *)ptr)->original_offset);}

int mus_env_breakpoints(mus_any *ptr) {return(((seg *)ptr)->size);}

static mus_long_t env_length(mus_any *ptr) {return((((seg *)ptr)->end + 1));}        /* this needs to match the :length arg to make-env (changed to +1, 20-Feb-08) */

static mus_float_t env_current_value(mus_any *ptr) {return(((seg *)ptr)->current_value);}

mus_long_t *mus_env_passes(mus_any *gen) {return(((seg *)gen)->locs);}

mus_float_t *mus_env_rates(mus_any *gen) {return(((seg *)gen)->rates);}

static int env_position(mus_any *ptr) {return(((seg *)ptr)->index);}

mus_float_t mus_env_offset(mus_any *gen) {return(((seg *)gen)->offset);}

mus_float_t mus_env_scaler(mus_any *gen) {return(((seg *)gen)->scaler);}

mus_float_t mus_env_initial_power(mus_any *gen) {return(((seg *)gen)->init_power);}

static void env_set_location(mus_any *ptr, mus_long_t val);

static mus_long_t seg_set_pass(mus_any *ptr, mus_long_t val) {env_set_location(ptr, val); return(val);}

static mus_long_t seg_pass(mus_any *ptr) 
{
  seg *gen = (seg *)ptr;
  return(gen->locs[gen->index] - gen->loc);
}


static mus_float_t env_increment(mus_any *rd)
{
  if (((seg *)rd)->style == MUS_ENV_STEP)
    return(0.0);
  return(((seg *)rd)->base);
}


static void env_reset(mus_any *ptr)
{
  seg *gen = (seg *)ptr;
  gen->current_value = gen->init_y;
  gen->index = 0;
  gen->loc = gen->locs[0];
  gen->rate = gen->rates[0];
  gen->power = gen->init_power;
}


static void rebuild_env(seg *e, mus_float_t scl, mus_float_t off, mus_long_t end)
{
  seg *new_e;

  new_e = (seg *)mus_make_env(e->original_data, e->size, scl, off, e->base, 0.0, end, NULL);
  if (e->locs) free(e->locs);
  if (e->rates) free(e->rates);
  e->locs = new_e->locs;
  e->rates = new_e->rates;

  e->init_y = new_e->init_y;
  e->init_power = new_e->init_power;
  env_reset((mus_any *)e);

  free(new_e);
}


static mus_float_t env_set_scaler(mus_any *ptr, mus_float_t val)
{
  seg *e;
  e = (seg *)ptr;
  rebuild_env(e, val, e->original_offset, e->end);
  e->original_scaler = val;
  return(val);
}


static mus_float_t env_set_offset(mus_any *ptr, mus_float_t val)
{
  seg *e;
  e = (seg *)ptr;
  rebuild_env(e, e->original_scaler, val, e->end);
  e->original_offset = val;
  return(val);
}


static mus_long_t env_set_length(mus_any *ptr, mus_long_t val)
{
  seg *e;
  e = (seg *)ptr;
  rebuild_env(e, e->original_scaler, e->original_offset, val - 1);
  e->end = val - 1;
  return(val);
}


static mus_any_class ENV_CLASS = {
  MUS_ENV,
  (char *)S_env,
  &free_env_gen,
  &describe_env,
  &env_equalp,
  &env_data, /* mus-data -> original breakpoints */
  0,
  &env_length, &env_set_length,
  0, 0, 
  &env_current_value, 0, /* mus-phase?? -- used in snd-sig.c, but this needs a better access point */
  &env_scaler, &env_set_scaler,
  &env_increment,
  0,
  &run_env,
  MUS_NOT_SPECIAL, 
  NULL,
  &env_position,
  &env_offset, &env_set_offset,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 
  &seg_pass, &seg_set_pass,
  0,
  0, 0, 0, 0,
  &env_reset,
  0, &seg_copy
};


mus_any *mus_make_env(mus_float_t *brkpts, int npts, mus_float_t scaler, mus_float_t offset, mus_float_t base, mus_float_t duration, mus_long_t end, mus_float_t *ignored)
{
  /* brkpts are not freed by the new env gen when it is freed, but should be protected during its existence */
  int i;
  mus_long_t dur_in_samples;
  mus_float_t *edata;
  seg *e = NULL;
  void (*fe_release)(mus_any *ptr);

  for (i = 2; i < npts * 2; i += 2)
    if (brkpts[i - 2] >= brkpts[i])
      {
	char *temp = NULL;
	mus_error(MUS_BAD_ENVELOPE, S_make_env ": env at breakpoint %d: x axis value: %f <= previous x value: %f (env: %s)", 
		  i / 2, brkpts[i], brkpts[i - 2], 
		  temp = float_array_to_string(brkpts, npts * 2, 0)); /* minor memleak here */
	if (temp) free(temp);
	return(NULL);
      }

  switch (npts)
    {
    case 1:
      e = (seg *)calloc(1, sizeof(seg));
      e->core = &ENV_CLASS;
      e->current_value = offset + scaler * brkpts[1];
      e->env_func = mus_env_line;
      e->original_data = brkpts;
      e->free_env = ferest;
      return((mus_any *)e);

    case 2:
      if (e2_free_list)
	{
	  e = e2_free_list;
	  e2_free_list = (seg *)(e->next);
	}
      fe_release = fe2;
      break;

    case 3:
      if (e3_free_list)
	{
	  e = e3_free_list;
	  e3_free_list = (seg *)(e->next);
	}
      fe_release = fe3;
      break;
      
    case 4:
      if (e4_free_list)
	{
	  e = e4_free_list;
	  e4_free_list = (seg *)(e->next);
	}
      fe_release = fe4;
      break;
      
    default:
      fe_release = ferest;
      break;
    }
  
  if (!e)
    {
      e = (seg *)malloc(sizeof(seg));
      e->core = &ENV_CLASS;
      e->size = npts;
      e->rates = (mus_float_t *)malloc(npts * sizeof(mus_float_t));
      e->locs = (mus_long_t *)malloc((npts + 1) * sizeof(mus_long_t));
    }

  e->free_env = fe_release;
  e->original_data = brkpts;

  if (duration != 0.0)
    dur_in_samples = (mus_long_t)(duration * sampling_rate);
  else dur_in_samples = (end + 1);

  e->init_y = offset + scaler * brkpts[1];
  e->current_value = e->init_y;
  e->rate = 0.0;
  e->offset = offset;
  e->scaler = scaler;
  e->original_offset = offset;
  e->original_scaler = scaler;
  e->base = base;
  e->end = (dur_in_samples - 1);
  e->loc = 0;
  e->index = 0;

  if (base == 1.0)
    {
      e->style = MUS_ENV_LINEAR;
      if ((npts == 2) &&
	  (brkpts[1] == brkpts[3]))
	e->env_func = mus_env_line;
      else e->env_func = mus_env_linear;
      e->power = 0.0;
      e->init_power = 0.0;
      canonicalize_env(e, brkpts, npts, dur_in_samples, scaler);
      e->rates[npts - 1] = 0.0;
    }
  else
    {
      if (base == 0.0)
	{
	  e->style = MUS_ENV_STEP;
	  e->env_func = mus_env_step;
	  e->power = 0.0;
	  e->init_power = 0.0;
	  canonicalize_env(e, brkpts, npts, dur_in_samples, scaler);
	  e->rates[npts - 1] = e->offset + (scaler * brkpts[npts * 2 - 1]); /* stick at last value, which in this case is the value (not an increment) */
	}
      else
	{
	  e->style = MUS_ENV_EXPONENTIAL;
	  e->env_func = mus_env_exponential;
	  edata = fixup_exp_env(e, brkpts, npts, offset, scaler, base);
	  if (!edata)
	    {
	      free(e);
	      return(NULL);
	    }
	  canonicalize_env(e, edata, npts, dur_in_samples, 1.0);
	  e->rates[npts - 1] = 1.0;
	  e->power = exp(edata[1]);
	  e->init_power = e->power;
	  e->offset -= e->scaler;
	  free(edata);
	}
    }

  e->rate = e->rates[0];
  e->loc = e->locs[0];
  return((mus_any *)e);
}

/* one way to make an impulse: (make-env '(0 1 1 0) :length 1 :base 0.0)
 * a counter: (make-env '(0 0 1 1) :length 21 :scaler 20) -- length = 1+scaler
 */ 


static void env_set_location(mus_any *ptr, mus_long_t val)
{
  seg *gen = (seg *)ptr;
  mus_long_t ctr = 0, loc;

  loc = gen->locs[gen->index] - gen->loc;
  if (loc == val) return;

  if (loc > val)
    mus_reset(ptr);
  else ctr = loc;

  while ((gen->index < (gen->size - 1)) && /* this was gen->size */
	 (ctr < val))
    {
      mus_long_t samps;
      if (val > gen->locs[gen->index])
	samps = gen->locs[gen->index] - ctr;
      else samps = val - ctr;

      switch (gen->style)
	{
	case MUS_ENV_LINEAR: 
	  gen->current_value += (samps * gen->rate);
	  break;

	case MUS_ENV_STEP: 
	  gen->current_value = gen->rate; 
	  break;

	case MUS_ENV_EXPONENTIAL: 
	  gen->power *= exp(samps * log(gen->rate));
	  gen->current_value = gen->offset + (gen->scaler * gen->power);
	  break;
	}

      ctr += samps;
      if (ctr < val)
	{
	  gen->index++;
	  if (gen->index < gen->size)
	    gen->rate = gen->rates[gen->index];
	}
    }
  gen->loc = gen->locs[gen->index] - ctr;
}


mus_float_t mus_env_interp(mus_float_t x, mus_any *ptr)
{
  /* the accuracy depends on the duration here -- more samples = more accurate */
  seg *gen = (seg *)ptr;
  env_set_location(ptr, (mus_long_t)((x * (gen->end + 1)) / (gen->original_data[gen->size * 2 - 2])));
  return(gen->current_value);
}


mus_float_t mus_env_any(mus_any *e, mus_float_t (*connect_points)(mus_float_t val))
{
  /* "env_any" is supposed to mimic "out-any" */
  seg *gen = (seg *)e;
  mus_float_t *pts;
  int pt, size;
  mus_float_t y0, y1, new_val, val;
  mus_float_t scaler, offset;

  scaler = gen->original_scaler;
  offset = gen->original_offset;
  size = gen->size;

  if (size <= 1)
    return(offset + scaler * connect_points(0.0));
    
  pts = gen->original_data;
  pt = gen->index;
  if (pt >= (size - 1)) pt = size - 2;
  if (pts[pt * 2 + 1] <= pts[pt * 2 + 3])
    {
      y0 = pts[pt * 2 + 1];
      y1 = pts[pt * 2 + 3];
    }
  else
    {
      y1 = pts[pt * 2 + 1];
      y0 = pts[pt * 2 + 3];
    }

  val = (mus_env(e) - offset) / scaler;
  new_val = connect_points( (val - y0) / (y1 - y0));
  return(offset + scaler * (y0 + new_val * (y1 - y0)));
}


/* ---------------- pulsed-env ---------------- */

typedef struct {
  mus_any_class *core;
  mus_any *e, *p;
  bool gens_allocated;
} plenv;


static void free_pulsed_env(mus_any *ptr) 
{
  plenv *g;
  g = (plenv *)ptr;
  if (g->gens_allocated)
    {
      mus_free(g->e);
      mus_free(g->p);
    }
  free(ptr); 
}

static mus_any *plenv_copy(mus_any *ptr)
{
  plenv *g, *p;
  p = (plenv *)ptr;
  g = (plenv *)malloc(sizeof(plenv));
  memcpy((void *)g, (void *)ptr, sizeof(plenv));
  g->gens_allocated = true;
  g->e = mus_copy(p->e);
  g->p = mus_copy(p->p);
  return((mus_any *)g);
}


static mus_float_t run_pulsed_env(mus_any *ptr, mus_float_t input, mus_float_t unused) 
{
  return(mus_pulsed_env(ptr, input));
}


static void pulsed_env_reset(mus_any *ptr)
{
  plenv *pl = (plenv *)ptr;
  mus_reset(pl->e);
  mus_reset(pl->p);
}


static bool pulsed_env_equalp(mus_any *p1, mus_any *p2)
{
  plenv *f1 = (plenv *)p1;
  plenv *f2 = (plenv *)p2;

  if (f1 == f2) return(true);
  return((env_equalp(f1->e, f2->e)) &&
	 (sw_equalp(f1->p, f2->p)));
}


static char *describe_pulsed_env(mus_any *ptr)
{
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s",
	       mus_name(ptr));
  return(describe_buffer);
}

static mus_any_class PULSED_ENV_CLASS = {
  MUS_PULSED_ENV,
  (char *)S_pulsed_env,
  &free_pulsed_env,
  &describe_pulsed_env,
  &pulsed_env_equalp,
  0, 0,
  0, 0,
  0, 0, 
  0, 0,
  0, 0,
  0, 0,
  &run_pulsed_env,
  MUS_NOT_SPECIAL, 
  NULL, 0,
  0, 0, 0, 0,
  0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &pulsed_env_reset,
  0, &plenv_copy
};


mus_any *mus_make_pulsed_env(mus_any *e, mus_any *p)
{
  plenv *gen;

  gen = (plenv *)malloc(sizeof(plenv));
  gen->core = &PULSED_ENV_CLASS;
  gen->e = e;
  gen->p = p;
  gen->gens_allocated = false;
  return((mus_any *)gen);
}

bool mus_is_pulsed_env(mus_any *ptr)
{
  return((ptr) && 
	 (ptr->core->type == MUS_PULSED_ENV));
}

mus_float_t mus_pulsed_env(mus_any *g, mus_float_t inval)
{
  plenv *pl = (plenv *)g;
  mus_float_t pt_val;
  pt_val = mus_pulse_train(pl->p, inval);
  if (pt_val > 0.1)
    mus_reset(pl->e);
  return(mus_env(pl->e));
}


mus_float_t mus_pulsed_env_unmodulated(mus_any *g)
{
  plenv *pl = (plenv *)g;
  mus_float_t pt_val;
  pt_val = mus_pulse_train_unmodulated(pl->p);
  if (pt_val > 0.1)
    mus_reset(pl->e);
  return(mus_env(pl->e));
}




/* ---------------- input/output ---------------- */

static mus_float_t mus_read_sample(mus_any *fd, mus_long_t frample, int chan) 
{
  if ((check_gen(fd, "mus-read-sample")) &&
      ((fd->core)->read_sample))
    return(((*(fd->core)->read_sample))(fd, frample, chan));
  return((mus_float_t)mus_error(MUS_NO_SAMPLE_INPUT, 
			  ":can't find %s's sample input function", 
			  mus_name(fd)));
}


bool mus_is_input(mus_any *gen) 
{
  return((gen) && 
	 (gen->core->extended_type == MUS_INPUT));
}


bool mus_is_output(mus_any *gen) 
{
  return((gen) && 
	 (gen->core->extended_type == MUS_OUTPUT));
}



/* ---------------- file->sample ---------------- */

typedef struct {
  mus_any_class *core;
  int chan;
  int dir;
  mus_long_t loc;
  char *file_name;
  int chans;
  mus_float_t **ibufs, **saved_data;
  mus_float_t *sbuf;
  mus_long_t data_start, data_end, file_end;
  mus_long_t file_buffer_size;
  mus_float_t (*reader)(mus_any *ptr);
} rdin;


static char *describe_file_to_sample(mus_any *ptr)
{
  rdin *gen = (rdin *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s \"%s\"", 
	       mus_name(ptr),
	       gen->file_name);
  return(describe_buffer);
}


static bool rdin_equalp(mus_any *p1, mus_any *p2) 
{
  rdin *r1 = (rdin *)p1;
  rdin *r2 = (rdin *)p2;
  return((p1 == p2) ||
	 ((r1) && (r2) &&
	  (r1->core->type == r2->core->type) &&
	  (r1->chan == r2->chan) &&
	  (r1->loc == r2->loc) &&
	  (r1->dir == r2->dir) &&
	  (r1->file_name) &&
	  (r2->file_name) &&
	  (strcmp(r1->file_name, r2->file_name) == 0)));
}


static void free_file_to_sample(mus_any *p) 
{
  rdin *ptr = (rdin *)p;
  if (ptr->core->end) ((*ptr->core->end))(p);
  free(ptr->file_name);
  free(ptr);
}

static mus_long_t make_ibufs(rdin *gen)
{
  int i;
  mus_long_t len;
  len = gen->file_end + 1;
  if (len > gen->file_buffer_size) 
    len = gen->file_buffer_size;
	      
  gen->ibufs = (mus_float_t **)malloc(gen->chans * sizeof(mus_float_t *));
  for (i = 0; i < gen->chans; i++)
    gen->ibufs[i] = (mus_float_t *)malloc(len * sizeof(mus_float_t));

  return(len);
}

static mus_any *rdin_copy(mus_any *ptr)
{
  rdin *g, *p;
  p = (rdin *)ptr;
  g = (rdin *)malloc(sizeof(rdin));
  memcpy((void *)g, (void *)ptr, sizeof(rdin));
  g->file_name = mus_strdup(p->file_name);
  if (p->ibufs)
    {
      int i;
      mus_long_t len;
      len = make_ibufs(g);
      for (i = 0; i < g->chans; i++)
	mus_copy_floats(g->ibufs[i], p->ibufs[i], len);
    }
  return((mus_any *)g);
}

static mus_long_t file_to_sample_length(mus_any *ptr) {return((((rdin *)ptr)->file_end));}

static int file_to_sample_channels(mus_any *ptr) {return((int)(((rdin *)ptr)->chans));}

static mus_float_t file_to_sample_increment(mus_any *rd) {return((mus_float_t)(((rdin *)rd)->dir));}
static mus_float_t file_to_sample_set_increment(mus_any *rd, mus_float_t val) {((rdin *)rd)->dir = (int)val; return(val);}

static char *file_to_sample_file_name(mus_any *ptr) {return(((rdin *)ptr)->file_name);}

static void no_reset(mus_any *ptr) {}

static mus_float_t mus_in_any_from_file(mus_any *ptr, mus_long_t samp, int chan)
{
  /* check in-core buffer bounds,
   * if needed read new buffer (taking into account dir)
   * return mus_float_t at samp (frample) 
   */
  rdin *gen = (rdin *)ptr;

  if (chan >= gen->chans)
    return(0.0);

  if ((samp <= gen->data_end) &&
      (samp >= gen->data_start))
    return((mus_float_t)(gen->ibufs[chan][samp - gen->data_start]));

  if ((samp >= 0) &&
      (samp < gen->file_end))
    {
      /* got to read it from the file */
      int fd;
      mus_long_t newloc;
      /* read in first buffer start either at samp (dir > 0) or samp-bufsize (dir < 0) */
      
      if (samp >= gen->data_start) /* gen dir is irrelevant here (see grev in clm23.scm) */
	newloc = samp; 
      else newloc = (mus_long_t)(samp - (gen->file_buffer_size * .75));
      /* The .75 in the backwards read is trying to avoid reading the full buffer on 
       * nearly every sample when we're oscillating around the
       * nominal buffer start/end (in src driven by an oscil for example)
       */
      if (newloc < 0) newloc = 0;
      gen->data_start = newloc;
      gen->data_end = newloc + gen->file_buffer_size - 1;
      fd = mus_sound_open_input(gen->file_name);
      if (fd == -1)
	return((mus_float_t)mus_error(MUS_CANT_OPEN_FILE, 
				      "open(%s) -> %s", 
				      gen->file_name, STRERROR(errno)));
      else
	{ 
	  if (!gen->ibufs) 
	    make_ibufs(gen);
	  mus_file_seek_frample(fd, gen->data_start);

	  if ((gen->data_start + gen->file_buffer_size) >= gen->file_end)
	    mus_file_read_chans(fd, gen->data_start, gen->file_end - gen->data_start, gen->chans, gen->ibufs, gen->ibufs);
	  else mus_file_read_chans(fd, gen->data_start, gen->file_buffer_size, gen->chans, gen->ibufs, gen->ibufs);
	  
	  /* we have to check file_end here because chunked files can have trailing chunks containing
	   *   comments or whatever.  io.c (mus_file_read_*) merely calls read, and translates bytes --
	   *   if it gets fewer than requested, it zeros from the point where the incoming file data stopped,
	   *   but that can be far beyond the actual end of the sample data!  It is at this level that
	   *   we know how much data is actually supposed to be in the file. 
	   *
	   * Also, file_end is the number of framples, so we should not read samp # file_end (see above).
	   */
	  
	  mus_sound_close_input(fd);
	  if (gen->data_end > gen->file_end) gen->data_end = gen->file_end;
	}
      
      return((mus_float_t)(gen->ibufs[chan][samp - gen->data_start]));
    }
  
  return(0.0);
}


static mus_float_t run_file_to_sample(mus_any *ptr, mus_float_t arg1, mus_float_t arg2) 
{
  /* mus_read_sample here? */
  return(mus_in_any_from_file(ptr, (int)arg1, (int)arg2));
} 


static int file_to_sample_end(mus_any *ptr)
{
  rdin *gen = (rdin *)ptr;
  if (gen)
    {
      if (gen->ibufs)
	{
	  int i;
	  for (i = 0; i < gen->chans; i++)
	    if (gen->ibufs[i]) 
	      free(gen->ibufs[i]);
	  free(gen->ibufs);
	  gen->ibufs = NULL;
	  gen->sbuf = NULL;
	}
    }
  return(0);
}


static mus_any_class FILE_TO_SAMPLE_CLASS = {
  MUS_FILE_TO_SAMPLE,
  (char *)S_file_to_sample,
  &free_file_to_sample,
  &describe_file_to_sample,
  &rdin_equalp,
  0, 0, 
  &file_to_sample_length, 0,
  0, 0, 0, 0,
  0, 0,
  &file_to_sample_increment, 
  &file_to_sample_set_increment,
  &run_file_to_sample,
  MUS_INPUT,
  NULL,
  &file_to_sample_channels,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  &mus_in_any_from_file,
  0,
  &file_to_sample_file_name,
  &file_to_sample_end,
  0, /* location */
  0, /* set_location */
  0, /* channel */
  0, 0, 0, 0,
  &no_reset,
  0, &rdin_copy
};


bool mus_is_file_to_sample(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_FILE_TO_SAMPLE));
}


mus_any *mus_make_file_to_sample_with_buffer_size(const char *filename, mus_long_t buffer_size)
{
  rdin *gen;

  if (!filename)
    mus_error(MUS_NO_FILE_NAME_PROVIDED, S_make_file_to_sample " requires a file name");
  else
    {
      gen = (rdin *)calloc(1, sizeof(rdin));
      gen->core = &FILE_TO_SAMPLE_CLASS;

      gen->file_name = (char *)malloc((strlen(filename) + 1) * sizeof(char));
      strcpy(gen->file_name, filename);
      gen->data_end = -1; /* force initial read */

      gen->chans = mus_sound_chans(gen->file_name);
      if (gen->chans <= 0) 
	mus_error(MUS_NO_CHANNELS, S_make_file_to_sample ": %s chans: %d", filename, gen->chans);

      gen->file_end = mus_sound_framples(gen->file_name);
      if (gen->file_end < 0) 
	mus_error(MUS_NO_LENGTH, S_make_file_to_sample ": %s framples: %" print_mus_long, filename, gen->file_end);

      if (buffer_size < gen->file_end)
	gen->file_buffer_size = buffer_size;
      else gen->file_buffer_size = gen->file_end;

      return((mus_any *)gen);
    }
  return(NULL);
}


mus_any *mus_make_file_to_sample(const char *filename)
{
  return(mus_make_file_to_sample_with_buffer_size(filename, clm_file_buffer_size));
}


mus_float_t mus_file_to_sample(mus_any *ptr, mus_long_t samp, int chan)
{
  rdin *gen = (rdin *)ptr;

  if (chan >= gen->chans)
    return(0.0);

  /* redundant in a sense, but saves the call overhead of mus_in_any_from_file */
  if ((samp <= gen->data_end) &&
      (samp >= gen->data_start))
    return((mus_float_t)(gen->ibufs[chan][samp - gen->data_start]));

  return(mus_in_any_from_file(ptr, samp, chan));
}



/* ---------------- readin ---------------- */

/* readin reads only the desired channel and increments the location by the direction
 *   it inherits from and specializes the file_to_sample class 
 */

static char *describe_readin(mus_any *ptr)
{
  rdin *gen = (rdin *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s %s[chan %d], loc: %" print_mus_long ", dir: %d", 
	       mus_name(ptr),
	       gen->file_name, gen->chan, gen->loc, gen->dir);
  return(describe_buffer);
}


static void free_readin(mus_any *p) 
{
  rdin *ptr = (rdin *)p;
  if (ptr->core->end) ((*ptr->core->end))(p);
  free(ptr->file_name);
  free(ptr);
}

static mus_float_t run_readin(mus_any *ptr, mus_float_t unused1, mus_float_t unused2) {return(((rdin *)ptr)->reader(ptr));}
static mus_float_t readin_to_sample(mus_any *ptr, mus_long_t samp, int chan) {return(((rdin *)ptr)->reader(ptr));}

static mus_float_t rd_increment(mus_any *ptr) {return((mus_float_t)(((rdin *)ptr)->dir));}
static mus_float_t rd_set_increment(mus_any *ptr, mus_float_t val) {((rdin *)ptr)->dir = (int)val; return(val);}

static mus_long_t rd_location(mus_any *rd) {return(((rdin *)rd)->loc);}
static mus_long_t rd_set_location(mus_any *rd, mus_long_t loc) {((rdin *)rd)->loc = loc; return(loc);}

static int rd_channel(mus_any *rd) {return(((rdin *)rd)->chan);}

static mus_any_class READIN_CLASS = {
  MUS_READIN,
  (char *)S_readin,
  &free_readin,
  &describe_readin,
  &rdin_equalp,
  0, 0, 
  &file_to_sample_length, 0,
  0, 0, 0, 0,
  &fallback_scaler, 0,
  &rd_increment,
  &rd_set_increment,
  &run_readin,
  MUS_INPUT,
  NULL,
  &file_to_sample_channels,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  &readin_to_sample,
  0,
  &file_to_sample_file_name,
  &file_to_sample_end,
  &rd_location,
  &rd_set_location,
  &rd_channel,
  0, 0, 0, 0,
  &no_reset,
  0, &rdin_copy
};


bool mus_is_readin(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_READIN));
}


mus_float_t mus_readin(mus_any *ptr)
{
  return(((rdin *)ptr)->reader(ptr));
}


static mus_float_t safe_readin(mus_any *ptr)
{
  mus_float_t res;
  rdin *rd = (rdin *)ptr;

  if ((rd->loc < rd->file_end) &&
      (rd->loc >= 0))
    res = rd->sbuf[rd->loc];
  else res = 0.0;
  rd->loc += rd->dir;
  return(res);
}


static mus_float_t readin(mus_any *ptr)
{
  mus_float_t res;
  rdin *rd = (rdin *)ptr;

  if ((rd->loc <= rd->data_end) &&
      (rd->loc >= rd->data_start))
    res = rd->sbuf[rd->loc - rd->data_start];
  else 
    {
      if ((rd->loc < 0) || (rd->loc >= rd->file_end))
	res = 0.0;
      else res = mus_in_any_from_file(ptr, rd->loc, rd->chan);
    }

  rd->loc += rd->dir;
  return(res);
}


mus_any *mus_make_readin_with_buffer_size(const char *filename, int chan, mus_long_t start, int direction, mus_long_t buffer_size)
{
  rdin *gen;
  if (chan >= mus_sound_chans(filename))
    mus_error(MUS_NO_SUCH_CHANNEL, S_make_readin ": %s, chan: %d, but chans: %d", filename, chan, mus_sound_chans(filename));
  
  gen = (rdin *)mus_make_file_to_sample(filename);
  if (gen)
    {
      gen->core = &READIN_CLASS;
      gen->loc = start;
      gen->dir = direction;
      gen->chan = chan;
      /* the saved data option does not save us anything in file_to_sample above */
      gen->saved_data = mus_sound_saved_data(filename);
      if (!gen->saved_data)
	{
	  char *str;
	  str = mus_expand_filename(filename);
	  if (str)
	    {
	      gen->saved_data = mus_sound_saved_data(str);
	      free(str);
	    }
	}
      if (gen->saved_data)
	{
	  gen->file_buffer_size = gen->file_end;
	  gen->sbuf = gen->saved_data[chan];
	  gen->reader = safe_readin;
	  gen->data_start = 0;
	  gen->data_end = gen->file_end;
	}
      else
	{
	  gen->ibufs = (mus_float_t **)calloc(gen->chans, sizeof(mus_float_t *));
	  if (buffer_size > gen->file_end)
	    {
	      gen->file_buffer_size = gen->file_end;
	      gen->reader = safe_readin;
	      gen->ibufs[chan] = (mus_float_t *)malloc(gen->file_buffer_size * sizeof(mus_float_t));
	      mus_in_any_from_file((mus_any *)gen, 0, chan);
	    }
	  else
	    {
	      gen->file_buffer_size = buffer_size;
	      gen->reader = readin;
	      gen->ibufs[chan] = (mus_float_t *)malloc(gen->file_buffer_size * sizeof(mus_float_t));
	    }
	  gen->sbuf = gen->ibufs[chan];
	}
      return((mus_any *)gen);
    }
  return(NULL);
}

/* it would be easy to extend readin to read from a float-vector by using the saved_data and safe_readin
 *   business above -- just need mus_make_readin_from_float_vector or something.
 */


/* ---------------- in-any ---------------- */

mus_float_t mus_in_any(mus_long_t samp, int chan, mus_any *IO)
{
  if (IO) return(mus_read_sample(IO, samp, chan));
  return(0.0);
}


bool mus_in_any_is_safe(mus_any *ptr)
{
  rdin *gen = (rdin *)ptr;
  return((gen) && 
	 ((gen->core->read_sample == mus_in_any_from_file) ||
	  (gen->core->read_sample == readin_to_sample)));
}



/* ---------------- file->frample ---------------- */

/* also built on file->sample */

static char *describe_file_to_frample(mus_any *ptr)
{
  rdin *gen = (rdin *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s \"%s\"", 
	       mus_name(ptr),
	       gen->file_name);
  return(describe_buffer);
}


static mus_float_t run_file_to_frample(mus_any *ptr, mus_float_t arg1, mus_float_t arg2) 
{
  mus_error(MUS_NO_RUN, "no run method for file->frample"); 
  return(0.0);
}


static mus_any_class FILE_TO_FRAMPLE_CLASS = {
  MUS_FILE_TO_FRAMPLE,
  (char *)S_file_to_frample,
  &free_file_to_sample,
  &describe_file_to_frample,
  &rdin_equalp,
  0, 0, 
  &file_to_sample_length, 0,
  0, 0, 0, 0,
  &fallback_scaler, 0,
  &file_to_sample_increment,     /* allow backward reads */ 
  &file_to_sample_set_increment, 
  &run_file_to_frample,
  MUS_INPUT,
  NULL,
  &file_to_sample_channels,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  &mus_in_any_from_file,
  0,
  &file_to_sample_file_name,
  &file_to_sample_end,
  0, /* location */
  0, /* set_location */
  0, /* channel */
  0, 0, 0, 0,
  &no_reset,
  0, &rdin_copy
};


mus_any *mus_make_file_to_frample_with_buffer_size(const char *filename, mus_long_t buffer_size)
{
  rdin *gen;
  gen = (rdin *)mus_make_file_to_sample_with_buffer_size(filename, buffer_size);
  if (gen) 
    {
      gen->core = &FILE_TO_FRAMPLE_CLASS;
      return((mus_any *)gen);
    }
  return(NULL);
}


mus_any *mus_make_file_to_frample(const char *filename)
{
  return(mus_make_file_to_frample_with_buffer_size(filename, clm_file_buffer_size));
}


bool mus_is_file_to_frample(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_FILE_TO_FRAMPLE));
}


mus_float_t *mus_file_to_frample(mus_any *ptr, mus_long_t samp, mus_float_t *f)
{
  rdin *gen = (rdin *)ptr;
  int i;

  if ((samp <= gen->data_end) &&
      (samp >= gen->data_start))
    {
      mus_long_t pos;
      pos = samp - gen->data_start;
      f[0] = gen->ibufs[0][pos];
      for (i = 1; i < gen->chans; i++) 
	f[i] = gen->ibufs[i][pos];
    }
  else
    {
      if ((samp < 0) ||
	  (samp >= gen->file_end))
	{
	  for (i = 0; i < gen->chans; i++) 
	    f[i] = 0.0;
	}
      else
	{
	  f[0] = mus_in_any_from_file(ptr, samp, 0);
	  for (i = 1; i < gen->chans; i++) 
	    f[i] = mus_in_any_from_file(ptr, samp, i);
	}
    }
  return(f);
}



/* ---------------- sample->file ---------------- */

/* in all output functions, the assumption is that we're adding to whatever already exists */
/* also, the "end" methods need to flush the output buffer */

/* rdout struct is in clm.h */

static char *describe_sample_to_file(mus_any *ptr)
{
  rdout *gen = (rdout *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s \"%s\"", 
	       mus_name(ptr),
	       gen->file_name);
  return(describe_buffer);
}


static bool sample_to_file_equalp(mus_any *p1, mus_any *p2) {return(p1 == p2);}


static void free_sample_to_file(mus_any *p) 
{
  rdout *ptr = (rdout *)p;
  if (ptr->core->end) ((*ptr->core->end))(p);
  free(ptr->file_name);
  free(ptr);
}

static mus_any *rdout_copy(mus_any *ptr)
{
  rdout *g, *p;
  p = (rdout *)ptr;
  g = (rdout *)malloc(sizeof(rdout));
  memcpy((void *)g, (void *)ptr, sizeof(rdout));
  g->file_name = mus_strdup(p->file_name);
  if (p->obufs)
    {
      int i;
      g->obufs = (mus_float_t **)malloc(g->chans * sizeof(mus_float_t *));
      for (i = 0; i < g->chans; i++)
	{
	  g->obufs[i] = (mus_float_t *)malloc(clm_file_buffer_size * sizeof(mus_float_t));
	  mus_copy_floats(g->obufs[i], p->obufs[i], clm_file_buffer_size);
	}
      g->obuf0 = g->obufs[0];
      if (g->chans > 1)
	g->obuf1 = g->obufs[1];
      else g->obuf1 = NULL;
    }
  return((mus_any *)g);
}

static int sample_to_file_channels(mus_any *ptr) {return((int)(((rdout *)ptr)->chans));}
static mus_long_t sample_to_file_samp_type(mus_any *ptr) {return((int)(((rdout *)ptr)->output_sample_type));}
static int sample_to_file_head_type(mus_any *ptr) {return((int)(((rdout *)ptr)->output_header_type));}

static mus_long_t bufferlen(mus_any *ptr) {return(clm_file_buffer_size);}

static mus_long_t set_bufferlen(mus_any *ptr, mus_long_t len) {clm_file_buffer_size = len; return(len);} 

static char *sample_to_file_file_name(mus_any *ptr) {return(((rdout *)ptr)->file_name);}

static int sample_to_file_end(mus_any *ptr);

static mus_float_t run_sample_to_file(mus_any *ptr, mus_float_t arg1, mus_float_t arg2) {mus_error(MUS_NO_RUN, "no run method for sample->file"); return(0.0);}

static mus_any_class SAMPLE_TO_FILE_CLASS = {
  MUS_SAMPLE_TO_FILE,
  (char *)S_sample_to_file,
  &free_sample_to_file,
  &describe_sample_to_file,
  &sample_to_file_equalp,
  0, 0, 
  &bufferlen, &set_bufferlen, /* does this have any effect on the current gen? */
  0, 0, 0, 0,
  &fallback_scaler, 0,
  0, 0,
  &run_sample_to_file,
  MUS_OUTPUT,
  NULL,
  &sample_to_file_channels,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0,
  &mus_out_any_to_file,
  &sample_to_file_file_name,
  &sample_to_file_end,
  &sample_to_file_samp_type, 0, 
  &sample_to_file_head_type,
  0, 0, 0, 0,
  &no_reset,
  0, &rdout_copy
};


static int *sample_type_zero = NULL;

int mus_sample_type_zero(mus_sample_t samp_type)
{
  return(sample_type_zero[samp_type]);
}


static void flush_buffers(rdout *gen)
{
  int fd;

  if ((!gen->obufs) || 
      (mus_file_probe(gen->file_name) == 0) ||
      (gen->chans == 0))
    return; /* can happen if output abandoned, then later mus_free called via GC sweep */

  fd = mus_sound_open_input(gen->file_name);
  if (fd == -1)
    {
      /* no output yet, so open the output file and write the current samples (no need to add to existing samples in this case) */
      fd = mus_sound_open_output(gen->file_name, 
				 (int)sampling_rate, 
				 gen->chans, 
				 gen->output_sample_type,
				 gen->output_header_type, 
				 NULL);
      if (fd == -1)
	mus_error(MUS_CANT_OPEN_FILE, 
		  "open(%s) -> %s", 
		  gen->file_name, STRERROR(errno));
      else
	{
	  mus_file_write(fd, 0, gen->out_end, gen->chans, gen->obufs);
	  mus_sound_close_output(fd, (gen->out_end + 1) * gen->chans * mus_bytes_per_sample(mus_sound_sample_type(gen->file_name))); 
	}
    }
  else
    {
      /* get existing samples, add new output, write back to output */
      mus_float_t **addbufs = NULL;
      int i;
      mus_sample_t sample_type;
      mus_long_t current_file_framples, framples_to_add;
      
      sample_type = mus_sound_sample_type(gen->file_name);
      current_file_framples = mus_sound_framples(gen->file_name);
      /* this is often 0 (brand-new file) */
      
      if (current_file_framples > gen->data_start)
	{
	  bool allocation_failed = false;
	  addbufs = (mus_float_t **)calloc(gen->chans, sizeof(mus_float_t *));
	  
	  for (i = 0; i < gen->chans; i++) 
	    {
	      /* clm_file_buffer_size may be too large, but it's very hard to tell that
	       *   in advance.  In Linux, malloc returns a non-null pointer even when
	       *   there's no memory available, so you have to touch the memory to force
	       *   the OS to deal with it, then the next allocation returns null.
	       */
	      addbufs[i] = (mus_float_t *)malloc(clm_file_buffer_size * sizeof(mus_float_t));
	      if (addbufs[i])
		addbufs[i][0] = 0.0;
	      else
		{
		  allocation_failed = true;
		  break;
		}
	    }
	  
	  if (allocation_failed)
	    {
	      mus_long_t old_file_buffer_size = 0;
	      
	      /* first clean up the mess we made */
	      for (i = 0; i < gen->chans; i++) 
		if (addbufs[i])
		  {
		    free(addbufs[i]);
		    addbufs[i] = NULL;
		  }
	      free(addbufs);
	      
	      /* it would take a lot of screwing around to find the biggest clm_file_buffer_size we could handle,
	       *   and it might fail on the next call (if more chans), so we'll throw an error.  We could get
	       *   say 1024 samps per chan, then run through a loop outputting the current buffer, but geez...
	       */
	      /* but... if we hit this in with-sound, mus_error calls (eventually) s7_error which sees the
	       *   dynamic-wind and tries to call mus-close, which tries to flush the buffers and we have
	       *   an infinite loop.  So, we need to clean up right now.
	       */
	      mus_sound_close_input(fd);
	      old_file_buffer_size = clm_file_buffer_size;
	      clm_file_buffer_size = MUS_DEFAULT_FILE_BUFFER_SIZE;
	      mus_error(MUS_MEMORY_ALLOCATION_FAILED, S_mus_file_buffer_size " (%" print_mus_long ") is too large: we can't allocate the output buffers!", old_file_buffer_size);
	      return;
	    }
	}
      
      framples_to_add = gen->out_end - gen->data_start;
      
      /* if the caller reset clm_file_buffer_size during a run, framples_to_add might be greater than the assumed buffer size,
       *   so we need to complain and fix up the limits.  In CLM, the size is set in sound.lisp, begin-with-sound.
       *   In Snd via mus_set_file_buffer_size in clm2xen.c.  The initial default is set in mus_initialize
       *   called in CLM by clm-initialize-links via in cmus.c, and in Snd in clm2xen.c when the module is setup.
       */
      if (framples_to_add >= clm_file_buffer_size) 
	{
	  mus_print("clm-file-buffer-size changed? %" print_mus_long " <= %" print_mus_long " (start: %" print_mus_long ", end: %" print_mus_long ", %" print_mus_long ")",
		    clm_file_buffer_size, framples_to_add, gen->data_start, gen->data_end, gen->out_end);
	  
	  framples_to_add = clm_file_buffer_size - 1;
	  /* this means we drop samples -- the other choice (short of throwing an error) would
	   *   be to read/allocate the bigger size.
	   */
	}
      if (addbufs)
	{
	  mus_file_seek_frample(fd, gen->data_start);
	  mus_file_read(fd, gen->data_start, framples_to_add + 1, gen->chans, addbufs);
	}
      mus_sound_close_input(fd); /* close previous mus_sound_open_input */

      fd = mus_sound_reopen_output(gen->file_name, gen->chans, sample_type,
				   mus_sound_header_type(gen->file_name),
				   mus_sound_data_location(gen->file_name));
      
      if ((current_file_framples < gen->data_start) &&
	  (sample_type_zero[sample_type] != 0))
	{
	  /* we're about to create a gap in the output file.  mus_file_seek_frample calls lseek which (man lseek):
	   *
           *    "The lseek function allows the file offset to be set beyond the  end  of
           *    the existing end-of-file of the file (but this does not change the size
           *    of the file).  If data is later written at this point, subsequent reads
           *    of  the  data  in the gap return bytes of zeros (until data is actually
           *    written into the gap)."
	   *
           * but 0 bytes in a file are not interpreted as sound samples of 0 in several sample types.
	   *  for example, mus-mulaw 0 => -.98, whereas sound sample 0 is a byte of 255.
	   *  see the table at the end of this file (sample_type_zero) for the other cases.
	   *
	   * So, we need to write explicit sample-type 0 values in those cases where machine 0's
	   *  won't be sample type 0.  sample_type_zero[type] != 0 signals we have such a
	   *  case, and returns the nominal zero value.  For unsigned shorts, we also need to
	   *  take endianess into account.
	   */
	  
	  mus_long_t filler, current_samps, bytes, bps;
	  unsigned char *zeros;
	  #define MAX_ZERO_SAMPLES 65536

	  bps = mus_bytes_per_sample(sample_type);
	  filler = gen->data_start - current_file_framples; 
	  mus_file_seek_frample(fd, current_file_framples);

	  if (filler > MAX_ZERO_SAMPLES)
	    bytes = MAX_ZERO_SAMPLES * bps * gen->chans;
	  else bytes = filler * bps * gen->chans;

	  zeros = (unsigned char *)malloc(bytes);
	  if (bps == 1)
	    memset((void *)zeros, sample_type_zero[sample_type], bytes);
	  else /* it has to be a short */
	    {
	      int df, i, b1, b2;
	      df = sample_type_zero[sample_type];
	      b1 = df >> 8;
	      b2 = df & 0xff;
	      for (i = 0; i < bytes; i += 2)
		{
		  zeros[i] = b2;
		  zeros[i + 1] = b1;
		}
	    }
	  /* (with-sound (:sample-type mus-ulshort) (fm-violin 10 1 440 .1)) */
	  while (filler > 0)
	    {
	      ssize_t wbytes;
	      if (filler > MAX_ZERO_SAMPLES)
		current_samps = MAX_ZERO_SAMPLES;
	      else 
		{
		  current_samps = filler;
		  bytes = current_samps * bps * gen->chans;
		}
	      wbytes = write(fd, zeros, bytes);
	      if (wbytes != bytes) fprintf(stderr, "%s[%d]: write trouble\n", __func__, __LINE__);
	      filler -= current_samps;
	    }
	  free(zeros);
	}
      
      if (addbufs)
	{
	  int j;
	  /* fill/write output buffers with current data added to saved data */
	  for (j = 0; j < gen->chans; j++)
	    {
	      mus_float_t *adder, *vals;
	      adder = addbufs[j];
	      vals = gen->obufs[j];
	      mus_add_floats(adder, vals, framples_to_add + 1);
	    }
	  
	  mus_file_seek_frample(fd, gen->data_start);
	  mus_file_write(fd, 0, framples_to_add, gen->chans, addbufs);
	  for (i = 0; i < gen->chans; i++) 
	    free(addbufs[i]); 
	  free(addbufs);
	}
      else
	{
	  /* output currently empty, so just flush out the gen->obufs */
	  mus_file_seek_frample(fd, gen->data_start);
	  mus_file_write(fd, 0, framples_to_add, gen->chans, gen->obufs);
	}
      
      if (current_file_framples <= gen->out_end) 
	current_file_framples = gen->out_end + 1;
      mus_sound_close_output(fd, current_file_framples * gen->chans * mus_bytes_per_sample(sample_type));
    }
}


mus_any *mus_sample_to_file_add(mus_any *out1, mus_any *out2)
{
  mus_long_t min_framples;
  rdout *dest = (rdout *)out1;
  rdout *in_coming = (rdout *)out2;
  int chn, min_chans;

  min_chans = dest->chans;
  if (in_coming->chans < min_chans) min_chans = in_coming->chans;
  min_framples = in_coming->out_end;

  for (chn = 0; chn < min_chans; chn++)
    {
      mus_long_t i;
      for (i = 0; i < min_framples; i++)
	dest->obufs[chn][i] += in_coming->obufs[chn][i];
      mus_clear_floats(in_coming->obufs[chn], min_framples);
    }

  if (min_framples > dest->out_end)
    dest->out_end = min_framples;

  in_coming->out_end = 0;
  in_coming->data_start = 0;

  return((mus_any*)dest);
}


mus_float_t mus_out_any_to_file(mus_any *ptr, mus_long_t samp, int chan, mus_float_t val)
{
  rdout *gen = (rdout *)ptr;
  if (!ptr) return(val);
  
  if ((chan >= gen->chans) ||  /* checking for (val == 0.0) here appears to make no difference overall */
      (!(gen->obufs)))
    return(val);

  if ((samp <= gen->data_end) &&
      (samp >= gen->data_start))
    gen->obufs[chan][samp - gen->data_start] += val;
  else
    {
      int j;
      if (samp < 0) return(val);
      flush_buffers(gen);
      for (j = 0; j < gen->chans; j++)
	mus_clear_floats(gen->obufs[j], clm_file_buffer_size);
      gen->data_start = samp;
      gen->data_end = samp + clm_file_buffer_size - 1;
      gen->obufs[chan][0] += val;
      gen->out_end = samp; /* this resets the current notion of where in the buffer the new data ends */
    }

  if (samp > gen->out_end) 
    gen->out_end = samp;
  return(val);
}


static void mus_out_chans_to_file(rdout *gen, mus_long_t samp, int chans, mus_float_t *vals)
{
  int i;
  if ((samp <= gen->data_end) &&
      (samp >= gen->data_start))
    {
      mus_long_t pos;
      pos = samp - gen->data_start;
      for (i = 0; i < chans; i++)
	gen->obufs[i][pos] += vals[i];
    }
  else
    {
      int j;
      if (samp < 0) return;
      flush_buffers(gen);
      for (j = 0; j < gen->chans; j++)
	mus_clear_floats(gen->obufs[j], clm_file_buffer_size);
      gen->data_start = samp;
      gen->data_end = samp + clm_file_buffer_size - 1;
      for (i = 0; i < chans; i++)
	gen->obufs[i][0] += vals[i];
      gen->out_end = samp; /* this resets the current notion of where in the buffer the new data ends */
    }

  if (samp > gen->out_end) 
    gen->out_end = samp;
}


static mus_float_t mus_outa_to_file(mus_any *ptr, mus_long_t samp, mus_float_t val)
{
  rdout *gen = (rdout *)ptr;
  if (!ptr) return(val);
  
  if ((!(gen->obuf0)) || 
      (!(gen->obufs)))
    return(val);

  if ((samp <= gen->data_end) &&
      (samp >= gen->data_start))
    gen->obuf0[samp - gen->data_start] += val;
  else
    {
      int j;
      if (samp < 0) return(val);
      flush_buffers(gen);
      for (j = 0; j < gen->chans; j++)
	mus_clear_floats(gen->obufs[j], clm_file_buffer_size);
      gen->data_start = samp;
      gen->data_end = samp + clm_file_buffer_size - 1;
      gen->obuf0[0] += val;
      gen->out_end = samp; /* this resets the current notion of where in the buffer the new data ends */
    }

  if (samp > gen->out_end) 
    gen->out_end = samp;
  return(val);
}


static mus_float_t mus_outb_to_file(mus_any *ptr, mus_long_t samp, mus_float_t val)
{
  rdout *gen = (rdout *)ptr;
  if (!ptr) return(val);
  
  if ((!(gen->obuf1)) ||
      (!(gen->obufs)))
    return(val);

  if ((samp <= gen->data_end) &&
      (samp >= gen->data_start))
    gen->obuf1[samp - gen->data_start] += val;
  else
    {
      int j;
      if (samp < 0) return(val);
      flush_buffers(gen);
      for (j = 0; j < gen->chans; j++)
	mus_clear_floats(gen->obufs[j], clm_file_buffer_size);
      gen->data_start = samp;
      gen->data_end = samp + clm_file_buffer_size - 1;
      gen->obuf1[0] += val;
      gen->out_end = samp; /* this resets the current notion of where in the buffer the new data ends */
    }

  if (samp > gen->out_end) 
    gen->out_end = samp;
  return(val);
}


static int sample_to_file_end(mus_any *ptr)
{
  rdout *gen = (rdout *)ptr;
  if ((gen) && (gen->obufs))
    {
      if (gen->chans > 0)
	{
	  int i;
	  flush_buffers(gen); /* this forces the error handling stuff, unlike in free reader case */
	  for (i = 0; i < gen->chans; i++)
	    if (gen->obufs[i]) 
	      free(gen->obufs[i]);
	}
      free(gen->obufs);
      gen->obufs = NULL;
      gen->obuf0 = NULL;
      gen->obuf1 = NULL;
    }
  return(0);
}


bool mus_is_sample_to_file(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_SAMPLE_TO_FILE));
}


static mus_any *mus_make_sample_to_file_with_comment_1(const char *filename, int out_chans, 
						       mus_sample_t samp_type, mus_header_t head_type, const char *comment, bool reopen)
{
  if (!filename)
    mus_error(MUS_NO_FILE_NAME_PROVIDED, S_make_sample_to_file " requires a file name");
  else
    {
      int fd;
      if (out_chans <= 0)
	return(NULL);
      if (reopen)
	fd = mus_sound_reopen_output(filename, out_chans, samp_type, head_type, mus_sound_data_location(filename));
      else fd = mus_sound_open_output(filename, (int)sampling_rate, out_chans, samp_type, head_type, comment);
      if (fd == -1)
	mus_error(MUS_CANT_OPEN_FILE, 
		  S_make_sample_to_file ": open(%s) -> %s", 
		  filename, STRERROR(errno));
      else
	{
	  rdout *gen;
	  int i;

	  gen = (rdout *)calloc(1, sizeof(rdout));
	  gen->core = &SAMPLE_TO_FILE_CLASS;
	  gen->file_name = (char *)calloc(strlen(filename) + 1, sizeof(char));
	  strcpy(gen->file_name, filename);
	  gen->data_start = 0;
	  gen->data_end = clm_file_buffer_size - 1;
	  gen->out_end = 0;
	  gen->chans = out_chans;
	  gen->output_sample_type = samp_type;
	  gen->output_header_type = head_type;
	  gen->obufs = (mus_float_t **)malloc(gen->chans * sizeof(mus_float_t *));
	  for (i = 0; i < gen->chans; i++) 
	    gen->obufs[i] = (mus_float_t *)calloc(clm_file_buffer_size, sizeof(mus_float_t));
	  gen->obuf0 = gen->obufs[0];
	  if (out_chans > 1)
	    gen->obuf1 = gen->obufs[1];
	  else gen->obuf1 = NULL;

	  /* clear previous, if any */
	  if (mus_file_close(fd) != 0)
	    mus_error(MUS_CANT_CLOSE_FILE, 
		      S_make_sample_to_file ": close(%d, %s) -> %s", 
		      fd, gen->file_name, STRERROR(errno));

	  return((mus_any *)gen);
	}
    }
  return(NULL);
}


mus_any *mus_continue_sample_to_file(const char *filename)
{
  return(mus_make_sample_to_file_with_comment_1(filename,
						mus_sound_chans(filename),
						mus_sound_sample_type(filename),
						mus_sound_header_type(filename),
						NULL,
						true));
}


mus_any *mus_make_sample_to_file_with_comment(const char *filename, int out_chans, mus_sample_t samp_type, mus_header_t head_type, const char *comment)
{
  return(mus_make_sample_to_file_with_comment_1(filename, out_chans, samp_type, head_type, comment, false));
}


mus_float_t mus_sample_to_file(mus_any *fd, mus_long_t samp, int chan, mus_float_t val)
{
  /* return(mus_write_sample(ptr, samp, chan, val)); */
  if ((fd) &&
      ((fd->core)->write_sample))
    return(((*(fd->core)->write_sample))(fd, samp, chan, val));
  mus_error(MUS_NO_SAMPLE_OUTPUT, 
	    S_sample_to_file ": can't find %s's sample output function", 
	    mus_name(fd));
  return(val);
}


int mus_close_file(mus_any *ptr)
{
  rdout *gen = (rdout *)ptr;
  if ((mus_is_output(ptr)) && (gen->obufs)) sample_to_file_end(ptr);
  return(0);
}


/* ---------------- out-any ---------------- */

mus_float_t mus_out_any(mus_long_t samp, mus_float_t val, int chan, mus_any *IO)
{
  if ((IO) && 
      (samp >= 0))
    {
      if ((IO->core)->write_sample)
	return(((*(IO->core)->write_sample))(IO, samp, chan, val));
      mus_error(MUS_NO_SAMPLE_OUTPUT, 
		"can't find %s's sample output function", 
		mus_name(IO));
    }
  return(val);
}


mus_float_t mus_safe_out_any_to_file(mus_long_t samp, mus_float_t val, int chan, mus_any *IO)
{
  rdout *gen = (rdout *)IO;
  if (chan >= gen->chans)  /* checking for (val == 0.0) here appears to make no difference overall */
    return(val);
  /* does this need to check obufs? */
      
  if ((samp <= gen->data_end) &&
      (samp >= gen->data_start))
    {
      gen->obufs[chan][samp - gen->data_start] += val;
      if (samp > gen->out_end) 
	gen->out_end = samp;
    }
  else
    {
      int j;
      if (samp < 0) return(val);
      flush_buffers(gen);
      for (j = 0; j < gen->chans; j++)
	mus_clear_floats(gen->obufs[j], clm_file_buffer_size);
      gen->data_start = samp;
      gen->data_end = samp + clm_file_buffer_size - 1;
      gen->obufs[chan][0] += val;
      gen->out_end = samp; /* this resets the current notion of where in the buffer the new data ends */
    }
  return(val);
  
}


bool mus_out_any_is_safe(mus_any *IO)
{
  rdout *gen = (rdout *)IO;
  return((gen) && 
	 (gen->obufs) &&
	 (gen->core->write_sample == mus_out_any_to_file));
}



/* ---------------- frample->file ---------------- */

static char *describe_frample_to_file(mus_any *ptr)
{
  rdout *gen = (rdout *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s \"%s\"", 
	       mus_name(ptr),
	       gen->file_name);
  return(describe_buffer);
}

static mus_float_t run_frample_to_file(mus_any *ptr, mus_float_t arg1, mus_float_t arg2) 
{
  mus_error(MUS_NO_RUN, "no run method for frample->file"); 
  return(0.0);
}


static mus_any_class FRAMPLE_TO_FILE_CLASS = {
  MUS_FRAMPLE_TO_FILE,
  (char *)S_frample_to_file,
  &free_sample_to_file,
  &describe_frample_to_file,
  &sample_to_file_equalp,
  0, 0,
  &bufferlen, &set_bufferlen,
  0, 0, 0, 0,
  &fallback_scaler, 0,
  0, 0,
  &run_frample_to_file,
  MUS_OUTPUT,
  NULL,
  &sample_to_file_channels,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0,
  &mus_out_any_to_file,
  &sample_to_file_file_name,
  &sample_to_file_end,
  &sample_to_file_samp_type, 0, 
  &sample_to_file_head_type,
  0, 0, 0, 0,
  &no_reset,
  0, &rdout_copy
};


mus_any *mus_make_frample_to_file_with_comment(const char *filename, int chans, mus_sample_t samp_type, mus_header_t head_type, const char *comment)
{
  rdout *gen;
  gen = (rdout *)mus_make_sample_to_file_with_comment(filename, chans, samp_type, head_type, comment);
  if (gen) gen->core = &FRAMPLE_TO_FILE_CLASS;
  return((mus_any *)gen);
}


bool mus_is_frample_to_file(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_FRAMPLE_TO_FILE));
}


mus_float_t *mus_frample_to_file(mus_any *ptr, mus_long_t samp, mus_float_t *data)
{
  rdout *gen = (rdout *)ptr;
  if (!gen) return(data);

  if (gen->chans == 1)
    mus_outa_to_file(ptr, samp, data[0]);
  else
    {
      if (gen->chans == 2)
	{
	  mus_outa_to_file(ptr, samp, data[0]);
	  mus_outb_to_file(ptr, samp, data[1]);
	}
      else mus_out_chans_to_file(gen, samp, gen->chans, data);
    }
  return(data);
}


mus_any *mus_continue_frample_to_file(const char *filename)
{
  rdout *gen;
  gen = (rdout *)mus_continue_sample_to_file(filename);
  if (gen) gen->core = &FRAMPLE_TO_FILE_CLASS;
  return((mus_any *)gen);
}


mus_float_t *mus_frample_to_frample(mus_float_t *matrix, int mx_chans, mus_float_t *in_samps, int in_chans, mus_float_t *out_samps, int out_chans)
{
  /* in->out conceptually, so left index is in_chan, it (j below) steps by out_chans */
  int i, j, offset;
  if (mx_chans < out_chans) out_chans = mx_chans;
  if (mx_chans < in_chans) in_chans = mx_chans;
  for (i = 0; i < out_chans; i++)
    {
      out_samps[i] = in_samps[0] * matrix[i];
      for (j = 1, offset = mx_chans; j < in_chans; j++, offset += mx_chans)
	out_samps[i] += in_samps[j] * matrix[offset + i];
    }
  return(out_samps);
}



/* ---------------- locsig ---------------- */

typedef struct {
  mus_any_class *core;
  mus_any *outn_writer;
  mus_any *revn_writer;
  mus_float_t *outf, *revf;
  mus_float_t *outn;
  mus_float_t *revn;
  int chans, rev_chans;
  mus_interp_t type;
  mus_float_t reverb, degree, distance;
  bool safe_output;
  void *closure;
  void (*locsig_func)(mus_any *ptr, mus_long_t loc, mus_float_t val);
  void (*detour)(mus_any *ptr, mus_long_t loc);
} locs;


static bool locsig_equalp(mus_any *p1, mus_any *p2) 
{
  locs *g1 = (locs *)p1;
  locs *g2 = (locs *)p2;
  if (p1 == p2) return(true);
  return((g1) && (g2) &&
	 (g1->core->type == g2->core->type) &&
	 (g1->chans == g2->chans) &&
	 (clm_arrays_are_equal(g1->outn, g2->outn, g1->chans)) &&
	 (((g1->revn) && (g2->revn)) || 
	  ((!g1->revn) && (!g2->revn))) &&
	 ((!(g1->revn)) || (clm_arrays_are_equal(g1->revn, g2->revn, g1->rev_chans))));
}


static char *describe_locsig(mus_any *ptr)
{
  char *str;
  int i, lim = 16;
  locs *gen = (locs *)ptr;

  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s chans %d, outn: [", 
	       mus_name(ptr),
	       gen->chans);
  str = (char *)malloc(STR_SIZE * sizeof(char));

  if (gen->outn)
    {
      if (gen->chans - 1 < lim) lim = gen->chans - 1;
      for (i = 0; i < lim; i++)
	{
	  snprintf(str, STR_SIZE, "%.3f ", gen->outn[i]);
	  if ((strlen(describe_buffer) + strlen(str)) < (DESCRIBE_BUFFER_SIZE - 16))
	    strcat(describe_buffer, str);
	  else break;
	}
      if (gen->chans - 1 > lim) strcat(describe_buffer, "...");
      snprintf(str, STR_SIZE, "%.3f]", gen->outn[gen->chans - 1]);
      strcat(describe_buffer, str);
    }
  else
    {
      strcat(describe_buffer, "nil!]");
    }

  if ((gen->rev_chans > 0) && (gen->revn))
    {
      strcat(describe_buffer, ", revn: [");
      lim = 16;
      if (gen->rev_chans - 1 < lim) lim = gen->rev_chans - 1;
      for (i = 0; i < lim; i++)
	{
	  snprintf(str, STR_SIZE, "%.3f ", gen->revn[i]);
	  if ((strlen(describe_buffer) + strlen(str)) < (DESCRIBE_BUFFER_SIZE - 16))
	    strcat(describe_buffer, str);
	  else break;
	}
      if (gen->rev_chans - 1 > lim) strcat(describe_buffer, "...");
      snprintf(str, STR_SIZE, "%.3f]", gen->revn[gen->rev_chans - 1]);
      strcat(describe_buffer, str);
    }

  snprintf(str, STR_SIZE, ", interp: %s", mus_interp_type_to_string(gen->type));
  strcat(describe_buffer, str);
  free(str);
  return(describe_buffer);
}


static void free_locsig(mus_any *p) 
{
  locs *ptr = (locs *)p;
  if (ptr->outn) 
    {
      free(ptr->outn);
      ptr->outn = NULL;
    }
  if (ptr->revn) 
    {
      free(ptr->revn);
      ptr->revn = NULL;
    }
  if (ptr->outf) free(ptr->outf);
  ptr->outf = NULL;
  if (ptr->revf) free(ptr->revf);
  ptr->revf = NULL;
  ptr->outn_writer = NULL;
  ptr->revn_writer = NULL;
  ptr->chans = 0;
  ptr->rev_chans = 0;
  free(ptr);
}

static mus_any *locs_copy(mus_any *ptr)
{
  locs *g, *p;
  int bytes;
  p = (locs *)ptr;
  g = (locs *)malloc(sizeof(locs));
  memcpy((void *)g, (void *)ptr, sizeof(locs));
  bytes = g->chans * sizeof(mus_float_t);
  if (p->outn)
    {
      g->outn = (mus_float_t *)malloc(bytes);
      mus_copy_floats(g->outn, p->outn, g->chans);
    }
  if (p->outf)
    {
      g->outf = (mus_float_t *)malloc(bytes);
      mus_copy_floats(g->outf, p->outf, g->chans);
    }
  bytes = g->rev_chans * sizeof(mus_float_t);
  if (p->revn)
    {
      g->revn = (mus_float_t *)malloc(bytes);
      mus_copy_floats(g->revn, p->revn, g->rev_chans);
    }
  if (p->revf)
    {
      g->revf = (mus_float_t *)malloc(bytes);
      mus_copy_floats(g->revf, p->revf, g->rev_chans);
    }
  return((mus_any *)g);
}

static mus_long_t locsig_length(mus_any *ptr) {return(((locs *)ptr)->chans);}
static mus_long_t locsig_type(mus_any *ptr) {return(((locs *)ptr)->type);}
static mus_float_t locsig_degree(mus_any *ptr) {return(((locs *)ptr)->degree);}
static mus_float_t locsig_distance(mus_any *ptr) {return(((locs *)ptr)->distance);}
static mus_float_t locsig_reverb(mus_any *ptr) {return(((locs *)ptr)->reverb);}

static int locsig_channels(mus_any *ptr) {return(((locs *)ptr)->chans);}

static mus_float_t *locsig_data(mus_any *ptr) {return(((locs *)ptr)->outn);}

static mus_float_t *locsig_xcoeffs(mus_any *ptr) {return(((locs *)ptr)->revn);}

mus_float_t *mus_locsig_outf(mus_any *ptr) {return(((locs *)ptr)->outf);}  /* clm2xen.c */
mus_float_t *mus_locsig_revf(mus_any *ptr) {return(((locs *)ptr)->revf);}

void *mus_locsig_closure(mus_any *ptr) {return(((locs *)ptr)->closure);} 
static void *locsig_set_closure(mus_any *ptr, void *e) {((locs *)ptr)->closure = e; return(e);}

void mus_locsig_set_detour(mus_any *ptr, void (*detour)(mus_any *ptr, mus_long_t val))
{
  locs *gen = (locs *)ptr;
  gen->detour = detour;
}


static void locsig_reset(mus_any *ptr)
{
  locs *gen = (locs *)ptr;
  if (gen->outn) mus_clear_floats(gen->outn, gen->chans);
  if (gen->revn) mus_clear_floats(gen->revn, gen->rev_chans);
}


static mus_float_t locsig_xcoeff(mus_any *ptr, int index) 
{
  locs *gen = (locs *)ptr;
  if (gen->revn)
    return(gen->revn[index]);
  return(0.0);
}


static mus_float_t locsig_set_xcoeff(mus_any *ptr, int index, mus_float_t val) 
{
  locs *gen = (locs *)ptr;
  if (gen->revn)
    gen->revn[index] = val; 
  return(val);
}


static mus_any *locsig_warned = NULL; 
/* these locsig error messages are a pain -- using the output pointer in the wan hope that
 *   subsequent runs will use a different output generator.
 */

mus_float_t mus_locsig_ref(mus_any *ptr, int chan) 
{
  locs *gen = (locs *)ptr;
  if ((ptr) && (mus_is_locsig(ptr))) 
    {
      if ((chan >= 0) && 
	  (chan < gen->chans))
	return(gen->outn[chan]);
      else 
	{
	  if (locsig_warned != gen->outn_writer)
	    {
	      mus_error(MUS_NO_SUCH_CHANNEL, 
			S_locsig_ref ": chan %d >= %d", 
			chan, gen->chans);
	      locsig_warned = gen->outn_writer;
	    }
	}
    }
  return(0.0);
}


mus_float_t mus_locsig_set(mus_any *ptr, int chan, mus_float_t val) 
{
  locs *gen = (locs *)ptr;
  if ((ptr) && (mus_is_locsig(ptr))) 
    {
      if ((chan >= 0) && 
	  (chan < gen->chans))
	gen->outn[chan] = val;
      else 
	{
	  if (locsig_warned != gen->outn_writer)
	    {
	      mus_error(MUS_NO_SUCH_CHANNEL, 
			S_locsig_set ": chan %d >= %d", 
			chan, gen->chans);
	      locsig_warned = gen->outn_writer;
	    }
	}
    }
  return(val);
}


mus_float_t mus_locsig_reverb_ref(mus_any *ptr, int chan) 
{
  locs *gen = (locs *)ptr;
  if ((ptr) && (mus_is_locsig(ptr))) 
    {
      if ((chan >= 0) && 
	  (chan < gen->rev_chans))
	return(gen->revn[chan]);
      else 
	{
	  if (locsig_warned != gen->outn_writer)
	    {
	      mus_error(MUS_NO_SUCH_CHANNEL, 
			S_locsig_reverb_ref ": chan %d, but this locsig has %d reverb chans", 
			chan, gen->rev_chans);
	      locsig_warned = gen->outn_writer;
	    }
	}
    }
  return(0.0);
}


mus_float_t mus_locsig_reverb_set(mus_any *ptr, int chan, mus_float_t val) 
{
  locs *gen = (locs *)ptr;
  if ((ptr) && (mus_is_locsig(ptr))) 
    {
      if ((chan >= 0) && 
	  (chan < gen->rev_chans))
	gen->revn[chan] = val;
      else 
	{
	  if (locsig_warned != gen->outn_writer)
	    {
	      mus_error(MUS_NO_SUCH_CHANNEL, 
			S_locsig_reverb_set ": chan %d >= %d", 
			chan, gen->rev_chans);
	      locsig_warned = gen->outn_writer;
	    }
	}
    }
  return(val);
}


static mus_float_t run_locsig(mus_any *ptr, mus_float_t arg1, mus_float_t arg2) 
{
  mus_locsig(ptr, (mus_long_t)arg1, arg2); 
  return(arg2);
}


static mus_any_class LOCSIG_CLASS = {
  MUS_LOCSIG,
  (char *)S_locsig,
  &free_locsig,
  &describe_locsig,
  &locsig_equalp,
  &locsig_data, 0,
  &locsig_length,
  0,
  0, 0, 0, 0,
  &locsig_degree, 0,
  &locsig_distance, 0,
  &run_locsig,
  MUS_OUTPUT,
  &mus_locsig_closure,
  &locsig_channels,
  &locsig_reverb, 0,
  0, 0,
  &locsig_xcoeff, &locsig_set_xcoeff, 
  &locsig_type, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 
  &locsig_xcoeffs, 0,
  &locsig_reset,
  &locsig_set_closure,  /* the method name is set_environ (clm2xen.c) */
  &locs_copy
};


bool mus_is_locsig(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_LOCSIG));
}


static void mus_locsig_fill(mus_float_t *arr, int chans, mus_float_t degree, mus_float_t scaler, mus_interp_t type)
{
  if (chans == 1)
    arr[0] = scaler;
  else
    {
      mus_float_t deg, pos, frac, degs_per_chan;
      int left, right;
      /* this used to check for degree < 0.0 first, but as Michael Klingbeil noticed, that
       *   means that in the stereo case, the location can jump to 90 => click.
       */
      if (chans == 2)
	{
	  /* there's no notion of a circle of speakers here, so we don't have to equate, for example, -90 and 270 */
	  if (degree > 90.0)
	    deg = 90.0;
	  else
	    {
	      if (degree < 0.0)
		deg = 0.0;
	      else deg = degree;
	    }
	  degs_per_chan = 90.0;
	}
      else 
	{
	  deg = fmod(degree, 360.0);
	  if (deg < 0.0) 
	    {
	      /* -0.0 is causing trouble when mus_float_t == float */
	      if (deg < -0.0000001)
		deg += 360.0;              /* C's fmod can return negative results when modulus is positive */
	      else deg = 0.0;
	    }
	  degs_per_chan = 360.0 / chans;
	}
      pos = deg / degs_per_chan;
      left = (int)pos; /* floor(pos) */
      right = left + 1;
      if (right >= chans) right = 0;
      frac = pos - left;
      if (type == MUS_INTERP_LINEAR)
	{
	  arr[left] = scaler * (1.0 - frac);
	  arr[right] = scaler * frac;
	}
      else
	{
	  mus_float_t ldeg, c, s;
	  ldeg = M_PI_2 * (0.5 - frac);
	  scaler *= sqrt(2.0) / 2.0;
	  c = cos(ldeg);
	  s = sin(ldeg);
	  arr[left] = scaler * (c + s);
	  arr[right] = scaler * (c - s);
	}
    }
}


static void mus_locsig_mono_no_reverb(mus_any *ptr, mus_long_t loc, mus_float_t val)
{
  locs *gen = (locs *)ptr;
  mus_outa_to_file(gen->outn_writer, loc, val * gen->outn[0]);
}


static void mus_locsig_mono(mus_any *ptr, mus_long_t loc, mus_float_t val)
{
  locs *gen = (locs *)ptr;
  mus_outa_to_file(gen->outn_writer, loc, val * gen->outn[0]);
  mus_outa_to_file(gen->revn_writer, loc, val * gen->revn[0]);
}


static void mus_locsig_stereo_no_reverb(mus_any *ptr, mus_long_t loc, mus_float_t val)
{
  locs *gen = (locs *)ptr;
  mus_outa_to_file(gen->outn_writer, loc, val * gen->outn[0]);
  mus_outb_to_file(gen->outn_writer, loc, val * gen->outn[1]);
}


static void mus_locsig_stereo(mus_any *ptr, mus_long_t loc, mus_float_t val) /* but mono rev */
{
  locs *gen = (locs *)ptr;
  mus_outa_to_file(gen->outn_writer, loc, val * gen->outn[0]);
  mus_outb_to_file(gen->outn_writer, loc, val * gen->outn[1]);
  mus_outa_to_file(gen->revn_writer, loc, val * gen->revn[0]);
}


static void mus_locsig_any(mus_any *ptr, mus_long_t loc, mus_float_t val)
{
  int i;
  locs *gen = (locs *)ptr;
  rdout *writer = (rdout *)(gen->outn_writer);
  for (i = 0; i < gen->chans; i++)
    {
      gen->outf[i] = val * gen->outn[i];
      if (writer)
	mus_out_any_to_file((mus_any *)writer, loc, i, gen->outf[i]);
    }
  writer = (rdout *)(gen->revn_writer);
  for (i = 0; i < gen->rev_chans; i++)
    {
      gen->revf[i] = val * gen->revn[i];
      if (writer)
	mus_out_any_to_file((mus_any *)writer, loc, i, gen->revf[i]);
    }
}

static void mus_locsig_safe_mono_no_reverb(mus_any *ptr, mus_long_t loc, mus_float_t val)
{
  /* here we know in each safe case that obufs fits loc chans and the output gen is ok */
  locs *gen = (locs *)ptr;
  rdout *writer = (rdout *)(gen->outn_writer);

  if ((loc <= writer->data_end) &&
      (loc >= writer->data_start))
    {
      writer->obufs[0][loc - writer->data_start] += (val * gen->outn[0]);
      if (loc > writer->out_end) 
	writer->out_end = loc;
    }
  else mus_outa_to_file((mus_any *)writer, loc, val * gen->outn[0]);
}


static void mus_locsig_safe_mono(mus_any *ptr, mus_long_t loc, mus_float_t val)
{
  locs *gen = (locs *)ptr;
  rdout *writer = (rdout *)(gen->outn_writer);

  if ((loc <= writer->data_end) &&
      (loc >= writer->data_start))
    {
      writer->obufs[0][loc - writer->data_start] += (val * gen->outn[0]); 
      if (loc > writer->out_end) 
	writer->out_end = loc;
    }
  else mus_outa_to_file((mus_any *)writer, loc, val * gen->outn[0]);

  writer = (rdout *)(gen->revn_writer);
  if ((loc <= writer->data_end) &&
      (loc >= writer->data_start))
    {
      writer->obufs[0][loc - writer->data_start] += (val * gen->revn[0]); 
      if (loc > writer->out_end) 
	writer->out_end = loc;
    }
  else mus_outa_to_file((mus_any *)writer, loc, val * gen->revn[0]);
}

static void mus_locsig_safe_stereo_no_reverb(mus_any *ptr, mus_long_t loc, mus_float_t val)
{
  locs *gen = (locs *)ptr;
  rdout *writer = (rdout *)(gen->outn_writer);

  if ((loc <= writer->data_end) &&
      (loc >= writer->data_start))
    {
      mus_long_t pos;
      pos = loc - writer->data_start;
      writer->obufs[0][pos] += (val * gen->outn[0]);   
      writer->obufs[1][pos] += (val * gen->outn[1]);   
      if (loc > writer->out_end) 
	writer->out_end = loc;
    }
  else
    {
      mus_outa_to_file((mus_any *)writer, loc, val * gen->outn[0]);
      mus_outb_to_file((mus_any *)writer, loc, val * gen->outn[1]);
    }
}

static void mus_locsig_safe_stereo(mus_any *ptr, mus_long_t loc, mus_float_t val)
{
  locs *gen = (locs *)ptr;
  rdout *writer = (rdout *)(gen->outn_writer);

  if ((loc <= writer->data_end) &&
      (loc >= writer->data_start))
    {
      mus_long_t pos;
      pos = loc - writer->data_start;
      writer->obufs[0][pos] += (val * gen->outn[0]); 
      writer->obufs[1][pos] += (val * gen->outn[1]); 
      if (loc > writer->out_end) 
	writer->out_end = loc;
    }
  else
    {
      mus_outa_to_file((mus_any *)writer, loc, val * gen->outn[0]);
      mus_outb_to_file((mus_any *)writer, loc, val * gen->outn[1]);
    }

  writer = (rdout *)(gen->revn_writer);
  if ((loc <= writer->data_end) &&
      (loc >= writer->data_start))
    {
      writer->obufs[0][loc - writer->data_start] += (val * gen->revn[0]); 
      if (loc > writer->out_end) 
	writer->out_end = loc;
    }
  else mus_outa_to_file((mus_any *)writer, loc, val * gen->revn[0]);
}


static void mus_locsig_detour(mus_any *ptr, mus_long_t loc, mus_float_t val)
{
  /* here we let the closure data decide what to do with the output */
  locs *gen = (locs *)ptr;
  if (gen->detour)
    {
      int i;
      for (i = 0; i < gen->chans; i++)
	gen->outf[i] = val * gen->outn[i];
      
      for (i = 0; i < gen->rev_chans; i++)
	gen->revf[i] = val * gen->revn[i];
      
      (*(gen->detour))(ptr, loc);
    }
}

static void mus_locsig_any_no_reverb(mus_any *ptr, mus_long_t loc, mus_float_t val)
{
  int i;
  locs *gen = (locs *)ptr;
  rdout *writer = (rdout *)(gen->outn_writer);
  for (i = 0; i < gen->chans; i++)
    {
      gen->outf[i] = val * gen->outn[i];
      if (writer)
	mus_out_any_to_file((mus_any *)writer, loc, i, gen->outf[i]);
    }
}

static void mus_locsig_safe_any_no_reverb(mus_any *ptr, mus_long_t loc, mus_float_t val)
{
  int i;
  locs *gen = (locs *)ptr;
  rdout *writer = (rdout *)(gen->outn_writer);

  if ((loc <= writer->data_end) &&
      (loc >= writer->data_start))
    {
      mus_long_t pos;
      pos = loc - writer->data_start;
      for (i = 0; i < gen->chans; i++)      
	writer->obufs[i][pos] += (val * gen->outn[i]); 
      if (loc > writer->out_end) 
	writer->out_end = loc;
    }
  else
    {
      for (i = 0; i < gen->chans; i++)
	mus_safe_out_any_to_file(loc, val * gen->outn[i], i, (mus_any *)writer);
    }
}


static void mus_locsig_safe_any(mus_any *ptr, mus_long_t loc, mus_float_t val)
{
  int i;
  locs *gen = (locs *)ptr;
  rdout *writer = (rdout *)(gen->outn_writer);

  if ((loc <= writer->data_end) &&
      (loc >= writer->data_start))
    {
      mus_long_t pos;
      pos = loc - writer->data_start;
      for (i = 0; i < gen->chans; i++)      
	writer->obufs[i][pos] += (val * gen->outn[i]); 
      if (loc > writer->out_end) 
	writer->out_end = loc;
    }
  else
    {
      for (i = 0; i < gen->chans; i++)
	mus_safe_out_any_to_file(loc, val * gen->outn[i], i, (mus_any *)writer);
    }

  writer = (rdout *)(gen->revn_writer);
  if ((loc <= writer->data_end) &&
      (loc >= writer->data_start))
    {
      mus_long_t pos;
      pos = loc - writer->data_start;
      for (i = 0; i < gen->rev_chans; i++)      
	writer->obufs[i][pos] += (val * gen->revn[i]); 
      if (loc > writer->out_end) 
	writer->out_end = loc;
    }
  else
    {
      for (i = 0; i < gen->rev_chans; i++)
	mus_safe_out_any_to_file(loc, val * gen->revn[i], i, (mus_any *)writer);
    }
}


mus_any *mus_make_locsig(mus_float_t degree, mus_float_t distance, mus_float_t reverb, 
			 int chans, mus_any *output,      /* direct signal output */
			 int rev_chans, mus_any *revput,  /* reverb output */
			 mus_interp_t type)
{
  locs *gen;
  mus_float_t dist;
  if (chans <= 0)
    {
      mus_error(MUS_ARG_OUT_OF_RANGE, S_make_locsig ": chans: %d", chans);
      return(NULL);
    }
  if (isnan(degree))
    {
      mus_error(MUS_ARG_OUT_OF_RANGE, S_make_locsig ": degree: %f", degree);
      return(NULL);
    }

  gen = (locs *)calloc(1, sizeof(locs));
  gen->core = &LOCSIG_CLASS;
  gen->outf = (mus_float_t *)calloc(chans, sizeof(mus_float_t));

  gen->type = type;
  gen->reverb = reverb;
  gen->distance = distance;
  gen->degree = degree;
  gen->safe_output = false;
  if (distance > 1.0)
    dist = 1.0 / distance;
  else dist = 1.0;

  if (mus_is_output(output)) 
    gen->outn_writer = output;
  gen->chans = chans;
  gen->outn = (mus_float_t *)calloc(gen->chans, sizeof(mus_float_t));
  mus_locsig_fill(gen->outn, gen->chans, degree, dist, type);

  if (mus_is_output(revput))
    gen->revn_writer = revput;
  gen->rev_chans = rev_chans;
  if (gen->rev_chans > 0)
    {
      gen->revn = (mus_float_t *)calloc(gen->rev_chans, sizeof(mus_float_t));
      gen->revf = (mus_float_t *)calloc(gen->rev_chans, sizeof(mus_float_t));
      mus_locsig_fill(gen->revn, gen->rev_chans, degree, (reverb * sqrt(dist)), type);
    }

  /* now choose the output function based on chans, and reverb
   */
  if ((!output) && (!revput))
    gen->locsig_func = mus_locsig_detour;
  else
    {
      gen->locsig_func = mus_locsig_any;

      if ((mus_is_output(output)) &&
	  (mus_out_any_is_safe(output)) &&
	  (mus_channels(output) == chans))
	{
	  if (rev_chans > 0)
	    {
	      if ((rev_chans == 1) &&
		  (mus_is_output(revput)) &&
		  (mus_out_any_is_safe(revput)) &&
		  (mus_channels(revput) == 1))
		{
		  gen->safe_output = true;
		  switch (chans)
		    {
		    case 1:  gen->locsig_func = mus_locsig_safe_mono;   break;
		    case 2:  gen->locsig_func = mus_locsig_safe_stereo; break;
		    default: gen->locsig_func = mus_locsig_safe_any;    break;
		    }
		}
	    }
	  else
	    {
	      gen->safe_output = true;
	      switch (chans)
		{
		case 1:  gen->locsig_func = mus_locsig_safe_mono_no_reverb;   break;
		case 2:  gen->locsig_func = mus_locsig_safe_stereo_no_reverb; break;
		default: gen->locsig_func = mus_locsig_safe_any_no_reverb;    break;
		}
	    }
	}
      else
	{
	  if (rev_chans > 0)
	    {
	      if (rev_chans == 1)
		{
		  switch (chans)
		    {
		    case 1:  gen->locsig_func = mus_locsig_mono;   break;
		    case 2:  gen->locsig_func = mus_locsig_stereo; break;
		    default: gen->locsig_func = mus_locsig_any;    break;
		    }
		}
	    }
	  else
	    {
	      switch (chans)
		{
		case 1:  gen->locsig_func = mus_locsig_mono_no_reverb;   break;
		case 2:  gen->locsig_func = mus_locsig_stereo_no_reverb; break;
		default: gen->locsig_func = mus_locsig_any_no_reverb;    break;
		}
	    }
	}
    }
  return((mus_any *)gen);
}

void mus_locsig(mus_any *ptr, mus_long_t loc, mus_float_t val)
{
  locs *gen = (locs *)ptr;
  (*(gen->locsig_func))(ptr, loc, val);
}

int mus_locsig_channels(mus_any *ptr)
{
  return(((locs *)ptr)->chans);
}

int mus_locsig_reverb_channels(mus_any *ptr)
{
  return(((locs *)ptr)->rev_chans);
}


void mus_move_locsig(mus_any *ptr, mus_float_t degree, mus_float_t distance)
{
  locs *gen = (locs *)ptr;
  mus_float_t dist;

  if (distance > 1.0)
    dist = 1.0 / distance;
  else dist = 1.0;

  if (gen->rev_chans > 0)
    {
      if (gen->rev_chans > 2)
	mus_clear_floats(gen->revn, gen->rev_chans);
      mus_locsig_fill(gen->revn, gen->rev_chans, degree, (gen->reverb * sqrt(dist)), gen->type);
    }
  if (gen->chans > 2)
    mus_clear_floats(gen->outn, gen->chans);
  mus_locsig_fill(gen->outn, gen->chans, degree, dist, gen->type);
}



/* ---------------- move-sound ---------------- */

typedef struct {
  mus_any_class *core;
  mus_any *outn_writer;
  mus_any *revn_writer;
  mus_float_t *outf, *revf;
  int out_channels, rev_channels;
  mus_long_t start, end;
  mus_any *doppler_delay, *doppler_env, *rev_env;
  mus_any **out_delays, **out_envs, **rev_envs;
  int *out_map;
  bool free_arrays, free_gens;
  void *closure;
  void (*detour)(mus_any *ptr, mus_long_t loc);
} dloc;


static bool move_sound_equalp(mus_any *p1, mus_any *p2) {return(p1 == p2);}
int mus_move_sound_channels(mus_any *ptr) {return(((dloc *)ptr)->out_channels);}
int mus_move_sound_reverb_channels(mus_any *ptr) {return(((dloc *)ptr)->rev_channels);}
static mus_long_t move_sound_length(mus_any *ptr) {return(((dloc *)ptr)->out_channels);} /* need both because return types differ */
static void move_sound_reset(mus_any *ptr) {}

mus_float_t *mus_move_sound_outf(mus_any *ptr) {return(((dloc *)ptr)->outf);}
mus_float_t *mus_move_sound_revf(mus_any *ptr) {return(((dloc *)ptr)->revf);}

void *mus_move_sound_closure(mus_any *ptr) {return(((dloc *)ptr)->closure);}
static void *move_sound_set_closure(mus_any *ptr, void *e) {((dloc *)ptr)->closure = e; return(e);}

void mus_move_sound_set_detour(mus_any *ptr, void (*detour)(mus_any *ptr, mus_long_t val))
{
  dloc *gen = (dloc *)ptr;
  gen->detour = detour;
}


static char *describe_move_sound(mus_any *ptr)
{
  dloc *gen = (dloc *)ptr;
  char *dopdly, *dopenv, *revenv;
  char *outdlys, *outenvs, *revenvs;
  char *outmap;
  char *starts;
  char *str1 = NULL, *str2 = NULL, *str3 = NULL;
  char *allstr;
  int len;

  starts = mus_format("%s start: %" print_mus_long ", end: %" print_mus_long ", out chans %d, rev chans: %d",
		      mus_name(ptr),
		      gen->start, 
		      gen->end, 
		      gen->out_channels, 
		      gen->rev_channels);
  dopdly = mus_format("doppler %s", str1 = mus_describe(gen->doppler_delay));
  dopenv = mus_format("doppler %s", str2 = mus_describe(gen->doppler_env));
  revenv = mus_format("global reverb %s", str3 = mus_describe(gen->rev_env));
  outdlys = clm_array_to_string(gen->out_delays, gen->out_channels, "out_delays", "    ");
  outenvs = clm_array_to_string(gen->out_envs, gen->out_channels, "out_envs", "    ");
  revenvs = clm_array_to_string(gen->rev_envs, gen->rev_channels, "rev_envs", "    ");
  outmap = int_array_to_string(gen->out_map, gen->out_channels, "out_map");

  len = 64 + strlen(starts) + strlen(dopdly) + strlen(dopenv) + strlen(revenv) + 
    strlen(outdlys) + strlen(outenvs) + strlen(revenvs) + strlen(outmap);
  allstr = (char *)malloc(len * sizeof(char));
  snprintf(allstr, len, "%s\n  %s\n  %s\n  %s\n  %s\n  %s\n  %s\n  %s\n  free: arrays: %s, gens: %s\n",
		      starts, dopdly, dopenv, revenv, outdlys, outenvs, revenvs, outmap,
		      (gen->free_arrays) ? "true" : "false",
		      (gen->free_gens) ? "true" : "false");
  if (str1) free(str1);
  if (str2) free(str2);
  if (str3) free(str3);
  free(starts); 
  free(dopdly); 
  free(dopenv); 
  free(revenv); 
  free(outdlys); 
  free(outenvs); 
  free(revenvs); 
  free(outmap);
  return(allstr);
}


static void free_move_sound(mus_any *p) 
{
  dloc *ptr = (dloc *)p;
  if (ptr->free_gens)
    {
      int i;
      /* free everything except outer arrays and IO stuff */
      if (ptr->doppler_delay) mus_free(ptr->doppler_delay);
      if (ptr->doppler_env) mus_free(ptr->doppler_env);
      if (ptr->rev_env) mus_free(ptr->rev_env);
      if (ptr->out_delays)
	for (i = 0; i < ptr->out_channels; i++)
	  if (ptr->out_delays[i]) mus_free(ptr->out_delays[i]);
      if (ptr->out_envs)
	for (i = 0; i < ptr->out_channels; i++)
	  if (ptr->out_envs[i]) mus_free(ptr->out_envs[i]);
      if (ptr->rev_envs)
	for (i = 0; i < ptr->rev_channels; i++)
	  if (ptr->rev_envs[i]) mus_free(ptr->rev_envs[i]);
    }
  
  if (ptr->free_arrays)
    {
      /* free outer arrays */
      if (ptr->out_envs) {free(ptr->out_envs); ptr->out_envs = NULL;}
      if (ptr->rev_envs) {free(ptr->rev_envs); ptr->rev_envs = NULL;}
      if (ptr->out_delays) {free(ptr->out_delays); ptr->out_delays = NULL;}
      if (ptr->out_map) free(ptr->out_map);
    }
  
  /* we created these in make_move_sound, so it should always be safe to free them */
  if (ptr->outf) free(ptr->outf);
  if (ptr->revf) free(ptr->revf);
  free(ptr);
}

static mus_any *dloc_copy(mus_any *ptr)
{
  dloc *g, *p;
  int i, bytes;
  p = (dloc *)ptr;
  g = (dloc *)malloc(sizeof(dloc));
  memcpy((void *)g, (void *)ptr, sizeof(dloc));

  if (p->outf)
    {
      bytes = p->out_channels * sizeof(mus_float_t);
      g->outf = (mus_float_t *)malloc(bytes);
      mus_copy_floats(g->outf, p->outf, p->out_channels);
    }
  if (p->revf)
    {
      bytes = p->rev_channels * sizeof(mus_float_t);
      g->revf = (mus_float_t *)malloc(bytes);
      mus_copy_floats(g->revf, p->revf, p->rev_channels);
    }

  g->free_arrays = true;
  g->free_gens = true;
  if (p->doppler_delay) g->doppler_delay = mus_copy(p->doppler_delay);
  if (p->doppler_env) g->doppler_env = mus_copy(p->doppler_env);
  if (p->rev_env) g->rev_env = mus_copy(p->rev_env);
  if (p->out_envs) 
    {
      g->out_envs = (mus_any **)malloc(p->out_channels * sizeof(mus_any *));
      for (i = 0; i < p->out_channels; i++) g->out_envs[i] = mus_copy(p->out_envs[i]);
    }
  if (p->rev_envs) 
    {
      g->rev_envs = (mus_any **)malloc(p->rev_channels * sizeof(mus_any *));
      for (i = 0; i < p->rev_channels; i++) g->rev_envs[i] = mus_copy(p->rev_envs[i]);
    }
  if (p->out_delays) 
    {
      g->out_delays = (mus_any **)malloc(p->out_channels * sizeof(mus_any *));
      for (i = 0; i < p->out_channels; i++) g->out_delays[i] = mus_copy(p->out_delays[i]);
    }
  if (p->out_map)
    {
      bytes = p->out_channels * sizeof(int);
      g->out_map = (int *)malloc(bytes);
      memcpy((void *)(g->out_map), (void *)(p->out_map), bytes);
    }

  return((mus_any *)g);
}

bool mus_is_move_sound(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_MOVE_SOUND));
}


mus_float_t mus_move_sound(mus_any *ptr, mus_long_t loc, mus_float_t uval)
{
  dloc *gen = (dloc *)ptr;
  mus_float_t val;
  int chan;

  if (loc > gen->end) val = 0.0; else val = uval;

  /* initial silence */
  if (loc < gen->start)
    {
      mus_delay_unmodulated(gen->doppler_delay, val);
      /* original calls out_any here with 0.0 -- a no-op */
      return(val);
    }

  /* doppler */
  if (gen->doppler_delay)
    val = mus_delay(gen->doppler_delay, val, mus_env(gen->doppler_env));

  /* direct signal */
  for (chan = 0; chan < gen->out_channels; chan++)
    {
      mus_float_t sample;
      sample = val * mus_env(gen->out_envs[chan]);
      if (gen->out_delays[chan])
	sample = mus_delay_unmodulated(gen->out_delays[chan], sample);
      gen->outf[gen->out_map[chan]] = sample;
    }

  /* reverb */
  if ((gen->rev_env) &&
      (gen->revf))
    {
      val *= mus_env(gen->rev_env);
      if (gen->rev_envs)
	{
	  if (gen->rev_channels == 1)
	    gen->revf[0] = val * mus_env(gen->rev_envs[0]);
	  else
	    {
	      for (chan = 0; chan < gen->rev_channels; chan++)
		gen->revf[gen->out_map[chan]] = val * mus_env(gen->rev_envs[chan]);
	    }
	}
      else gen->revf[0] = val;
      
      if (gen->revn_writer)
	mus_frample_to_file(gen->revn_writer, loc, gen->revf);
    }

  /* file output */
  if (gen->outn_writer)
    mus_frample_to_file(gen->outn_writer, loc, gen->outf);

  if (gen->detour)
    (*(gen->detour))(ptr, loc);
  return(uval);
}


static mus_float_t run_move_sound(mus_any *ptr, mus_float_t arg1, mus_float_t arg2) 
{
  mus_move_sound(ptr, (mus_long_t)arg1, arg2); 
  return(arg2);
}


static mus_any_class MOVE_SOUND_CLASS = {
  MUS_MOVE_SOUND,
  (char *)S_move_sound,
  &free_move_sound,
  &describe_move_sound,
  &move_sound_equalp,
  0, 0,
  &move_sound_length,
  0,
  0, 0, 0, 0,
  0, 0,
  0, 0,
  &run_move_sound,
  MUS_OUTPUT,
  &mus_move_sound_closure,
  &mus_move_sound_channels,
  0, 0, 0, 0,
  0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 
  0, 0,
  &move_sound_reset,
  &move_sound_set_closure,
  &dloc_copy
};


mus_any *mus_make_move_sound(mus_long_t start, mus_long_t end, int out_channels, int rev_channels,
			     mus_any *doppler_delay, mus_any *doppler_env, mus_any *rev_env,
			     mus_any **out_delays, mus_any **out_envs, mus_any **rev_envs,
			     int *out_map, mus_any *output, mus_any *revput, bool free_arrays, bool free_gens)
{
  /* most of these args come to us in a list at the lisp/xen level ("dlocs" struct is actually a list) 
   *   so the make-move-sound function in lisp/xen is (make-move-sound dloc-list output revout)
   *   where the trailing args mimic locsig.
   */
  dloc *gen;
  if (out_channels <= 0)
    {
      mus_error(MUS_ARG_OUT_OF_RANGE, S_make_move_sound ": out chans: %d", out_channels);
      return(NULL);
    }
  gen = (dloc *)calloc(1, sizeof(dloc));
  gen->core = &MOVE_SOUND_CLASS;

  gen->start = start;
  gen->end = end;
  gen->out_channels = out_channels;
  gen->rev_channels = rev_channels;
  gen->doppler_delay = doppler_delay;
  gen->doppler_env = doppler_env;
  gen->rev_env = rev_env;
  gen->out_delays = out_delays;
  gen->out_envs = out_envs;
  gen->rev_envs = rev_envs;
  gen->out_map = out_map;

  /* default is to free only what we make ourselves */
  gen->free_gens = free_gens;
  gen->free_arrays = free_arrays;

  gen->outf = (mus_float_t *)calloc(out_channels, sizeof(mus_float_t));
  if (mus_is_output(output)) 
    gen->outn_writer = output;

  if (rev_channels > 0)
    {
      if (mus_is_output(revput))
	gen->revn_writer = revput;
      gen->revf = (mus_float_t *)calloc(rev_channels, sizeof(mus_float_t));
    }

  return((mus_any *)gen);
}



/* ---------------- src ---------------- */

/* sampling rate conversion */
/* taken from sweep_srate.c of Perry Cook.  To quote Perry:
 *
 * 'The conversion is performed by sinc interpolation.
 *    J. O. Smith and P. Gossett, "A Flexible Sampling-Rate Conversion Method," 
 *    Proc. of the IEEE Conference on Acoustics, Speech, and Signal Processing, San Diego, CA, March, 1984.
 * There are essentially two cases, one where the conversion factor
 * is less than one, and the sinc table is used as is yielding a sound
 * which is band limited to the 1/2 the new sampling rate (we don't
 * want to create bandwidth where there was none).  The other case
 * is where the conversion factor is greater than one and we 'warp'
 * the sinc table to make the final cutoff equal to the original sampling
 * rate /2.  Warping the sinc table is based on the similarity theorem
 * of the time and frequency domain, stretching the time domain (sinc
 * table) causes shrinking in the frequency domain.'
 *
 * we also scale the amplitude if interpolating to take into account the broadened sinc 
 *   this means that isolated pulses get scaled by 1/src, but that's a dumb special case
 */

typedef struct {
  mus_any_class *core;
  mus_float_t (*feeder)(void *arg, int direction);
  mus_float_t (*block_feeder)(void *arg, int direction, mus_float_t *block, mus_long_t start, mus_long_t end);
  mus_float_t x;
  mus_float_t incr, width_1;
  int width, lim, start, sinc4;
  int len;
  mus_float_t *data, *sinc_table, *coeffs;
  void *closure;
} sr;


#define SRC_SINC_DENSITY 2000
#define SRC_SINC_WIDTH 10
#define SRC_SINC_WINDOW_SIZE 8000

static mus_float_t **sinc_tables = NULL;
static int *sinc_widths = NULL;
static int sincs = 0;
static mus_float_t *sinc = NULL, *sinc_window = NULL;
static int sinc_size = 0;

void mus_clear_sinc_tables(void)
{
  if (sincs)
    {
      int i;
      for (i = 0; i < sincs; i++) 
	if (sinc_tables[i]) 
	  free(sinc_tables[i]);
      free(sinc_tables);
      sinc_tables = NULL;
      
      free(sinc_window);
      sinc_window = NULL;
      free(sinc_widths);
      sinc_widths = NULL;
      sincs = 0;
    }
}


static int init_sinc_table(int width)
{
  int i, size, padded_size, loc;
  mus_float_t win_freq, win_phase;
#if HAVE_SINCOS
  double sn, snp, cs, csp;
#endif

  if (width > sinc_size)
    {
      int old_end;
      mus_float_t sinc_phase, sinc_freq;
      if (sinc_size == 0)
	old_end = 1;
      else old_end = sinc_size * SRC_SINC_DENSITY + 4;
      padded_size = width * SRC_SINC_DENSITY + 4;
      if (sinc_size == 0)
	{
	  sinc = (mus_float_t *)malloc(padded_size * sizeof(mus_float_t));
	  sinc[0] = 1.0;
	}
      else sinc = (mus_float_t *)realloc(sinc, padded_size * sizeof(mus_float_t));
      sinc_size = width;
      sinc_freq = M_PI / (mus_float_t)SRC_SINC_DENSITY;
      sinc_phase = old_end * sinc_freq;
#if HAVE_SINCOS
      sincos(sinc_freq, &sn, &cs);
      if (old_end == 1)
	{
	  sinc[1] = sin(sinc_phase) / (2.0 * sinc_phase);
	  old_end++;
	  sinc_phase += sinc_freq;
	}
      for (i = old_end; i < padded_size;)
	{
	  sincos(sinc_phase, &snp, &csp);
	  sinc[i] = snp / (2.0 * sinc_phase);
	  i++;
	  sinc_phase += sinc_freq;
	  sinc[i] = (snp * cs + csp * sn) / (2.0 * sinc_phase);
	  i++;
	  sinc_phase += sinc_freq;
	}
#else
      for (i = old_end; i < padded_size; i++, sinc_phase += sinc_freq)
	sinc[i] = sin(sinc_phase) / (2.0 * sinc_phase);
#endif
    }

  for (i = 0; i < sincs; i++)
    if (sinc_widths[i] == width)
      return(i);

  if (sincs == 0)
    {
      mus_float_t ph, incr;
      incr = M_PI / SRC_SINC_WINDOW_SIZE;
      sinc_window = (mus_float_t *)calloc(SRC_SINC_WINDOW_SIZE + 16, sizeof(mus_float_t));
      for (i = 0, ph = 0.0; i < SRC_SINC_WINDOW_SIZE; i++, ph += incr) 
	sinc_window[i] = 1.0 + cos(ph);

      sinc_tables = (mus_float_t **)calloc(8, sizeof(mus_float_t *));
      sinc_widths = (int *)calloc(8, sizeof(int));
      sincs = 8;
      loc = 0;
    }
  else
    {
      loc = -1;
      for (i = 0; i < sincs; i++)
	if (sinc_widths[i] == 0)
	  {
	    loc = i;
	    break;
	  }
      if (loc == -1)
	{
	  sinc_tables = (mus_float_t **)realloc(sinc_tables, (sincs + 8) * sizeof(mus_float_t *));
	  sinc_widths = (int *)realloc(sinc_widths, (sincs + 8) * sizeof(int));
	  for (i = sincs; i < (sincs + 8); i++) 
	    {
	      sinc_widths[i] = 0; 
	      sinc_tables[i] = NULL;
	    }
	  loc = sincs;
	  sincs += 8;
	}
    }

  sinc_widths[loc] = width;
  size = width * SRC_SINC_DENSITY;
  padded_size = size + 4;
  win_freq = (mus_float_t)SRC_SINC_WINDOW_SIZE / (mus_float_t)size;

  sinc_tables[loc] = (mus_float_t *)malloc(padded_size * 2 * sizeof(mus_float_t));
  sinc_tables[loc][padded_size] = 1.0;

  for (i = 1, win_phase = win_freq; i < padded_size; i++, win_phase += win_freq)
    {
      mus_float_t val;
      val = sinc[i] * sinc_window[(int)win_phase];
      sinc_tables[loc][padded_size + i] = val;
      sinc_tables[loc][padded_size - i] = val;
    }

  return(loc);
}


bool mus_is_src(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_SRC));
}


static void free_src_gen(mus_any *srptr)
{
  sr *srp = (sr *)srptr;
  if (srp->data) free(srp->data);
  if (srp->coeffs) free(srp->coeffs);
  free(srp);
}

static mus_any *sr_copy(mus_any *ptr)
{
  sr *g, *p;
  int bytes;

  p = (sr *)ptr;
  g = (sr *)malloc(sizeof(sr));
  memcpy((void *)g, (void *)ptr, sizeof(sr));

  bytes = (2 * g->lim + 1) * sizeof(mus_float_t);
  g->data = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->data, p->data, 2 * g->lim + 1);
  
  if (p->coeffs)
    {
      bytes = p->lim * sizeof(mus_float_t);
      g->coeffs = (mus_float_t *)malloc(bytes);
      mus_copy_floats(g->coeffs, p->coeffs, p->lim);
    }
  return((mus_any *)g);
}

static bool src_equalp(mus_any *p1, mus_any *p2) {return(p1 == p2);}


static char *describe_src(mus_any *ptr)
{
  sr *gen = (sr *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s width: %d, x: %.3f, incr: %.3f, sinc table len: %d",
	       mus_name(ptr),
	       gen->width, gen->x, gen->incr, gen->len);
  return(describe_buffer);
}


static mus_long_t src_length(mus_any *ptr) {return(((sr *)ptr)->width);}
static mus_float_t run_src_gen(mus_any *srptr, mus_float_t sr_change, mus_float_t unused) {return(mus_src(srptr, sr_change, NULL));}

static void *src_closure(mus_any *rd) {return(((sr *)rd)->closure);}
static void *src_set_closure(mus_any *rd, void *e) {((sr *)rd)->closure = e; return(e);}

static mus_float_t src_increment(mus_any *rd) {return(((sr *)rd)->incr);}
static mus_float_t src_set_increment(mus_any *rd, mus_float_t val) {((sr *)rd)->incr = val; return(val);}

static mus_float_t *src_sinc_table(mus_any *rd) {return(((sr *)rd)->sinc_table);}

static void src_reset(mus_any *ptr)
{
  sr *gen = (sr *)ptr;
  mus_clear_floats(gen->data, gen->lim + 1);
  gen->x = 0.0;
  /* center the data if possible */
  if (gen->feeder)
    {
      int i, dir = 1;
      if (gen->incr < 0.0) dir = -1;
      for (i = gen->width - 1; i < gen->lim; i++) 
	gen->data[i] = gen->feeder(gen->closure, dir);
    }
  gen->start = 0;
}

void mus_src_init(mus_any *ptr)
{
  sr *srp = (sr *)ptr;
  if (srp->feeder)
    {
      int i, dir = 1;
      if (srp->incr < 0.0) dir = -1;
      for (i = srp->width - 1; i < srp->lim; i++) 
	{
	  srp->data[i] = srp->feeder(srp->closure, dir);
	  srp->data[i + srp->lim] = srp->data[i];
	}
    }
}

static mus_any_class SRC_CLASS = {
  MUS_SRC,
  (char *)S_src,
  &free_src_gen,
  &describe_src,
  &src_equalp,
  &src_sinc_table, 0,
  &src_length,  /* sinc width actually */
  0,
  0, 0, 0, 0,
  &fallback_scaler, 0,
  &src_increment,
  &src_set_increment,
  &run_src_gen,
  MUS_NOT_SPECIAL,
  &src_closure,
  0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &src_reset,
  &src_set_closure, 
  &sr_copy
};


mus_any *mus_make_src_with_init(mus_float_t (*input)(void *arg, int direction), mus_float_t srate, int width, void *closure, void (*init)(void *p, mus_any *g))
{
  /* besides 1, 2, .5, other common cases: 1.5, 3
   */

  if (fabs(srate) > MUS_MAX_CLM_SRC)
    mus_error(MUS_ARG_OUT_OF_RANGE, S_make_src ": srate arg invalid: %f", srate);
  else
    {
      if ((width < 0) || (width > MUS_MAX_CLM_SINC_WIDTH))
	mus_error(MUS_ARG_OUT_OF_RANGE, S_make_src ": width arg invalid: %d", width);
      else
	{
	  sr *srp;
	  int wid, loc;

	  if (width <= 0) width = SRC_SINC_WIDTH;
	  if (width < (int)(fabs(srate) * 2))
	    wid = (int)(ceil(fabs(srate)) * 2); 
	  else wid = width;
	  if ((srate == 2.0) &&
	      ((wid & 1) != 0))
	    wid++;

	  srp = (sr *)calloc(1, sizeof(sr));
	  srp->core = &SRC_CLASS;
	  srp->x = 0.0;
	  srp->feeder = input;
	  srp->block_feeder = NULL;
	  srp->closure = closure;
	  srp->incr = srate;
	  srp->width = wid;
	  srp->lim = 2 * wid;
	  srp->start = 0;
	  srp->len = wid * SRC_SINC_DENSITY;
	  srp->width_1 = 1.0 - wid;
	  srp->sinc4 = srp->width * SRC_SINC_DENSITY + 4;
	  srp->data = (mus_float_t *)calloc(2 * srp->lim + 1, sizeof(mus_float_t));
	  loc = init_sinc_table(wid);
	  srp->sinc_table = sinc_tables[loc];
	  srp->coeffs = NULL;

	  if (init)
	    init(closure, (mus_any *)srp);

	  if (srp->feeder)
	    {
	      int i, dir = 1;
	      if (srate < 0.0) dir = -1;
	      for (i = wid - 1; i < srp->lim; i++) 
		{
		  srp->data[i] = srp->feeder(closure, dir);
		  srp->data[i + srp->lim] = srp->data[i];
		}
	      /* was i = 0 here but we want the incoming data centered */
	    }
	  return((mus_any *)srp);
	}
    }
  return(NULL);
}

mus_any *mus_make_src(mus_float_t (*input)(void *arg, int direction), mus_float_t srate, int width, void *closure)
{
  return(mus_make_src_with_init(input, srate, width, closure, NULL));
}

mus_float_t mus_src(mus_any *srptr, mus_float_t sr_change, mus_float_t (*input)(void *arg, int direction))
{
  sr *srp = (sr *)srptr;
  mus_float_t sum, zf, srx, factor;
  int lim, loc, xi;
  bool int_ok;
  mus_float_t *data, *sinc_table;

  lim = srp->lim;
  loc = srp->start;
  data = srp->data;
  sinc_table = srp->sinc_table;

  if (sr_change > MUS_MAX_CLM_SRC) 
    sr_change = MUS_MAX_CLM_SRC;
  else
    {
      if (sr_change < -MUS_MAX_CLM_SRC) 
	sr_change = -MUS_MAX_CLM_SRC;
    }
  srx = srp->incr + sr_change;

  if (srp->x >= 1.0)
    {
      int i, fsx, dir = 1;

      if (srx < 0.0) dir = -1;
      fsx = (int)(srp->x);
      srp->x -= fsx;

      if (input) {srp->feeder = input; srp->block_feeder = NULL;}

      data[loc] = srp->feeder(srp->closure, dir);
      data[loc + lim] = data[loc];
      loc++;
      if (loc == lim) loc = 0;
      
      for (i = 1; i < fsx; i++)
	{
	  /* there are two copies of the circular data buffer back-to-back so that we can
	   *   run the convolution below without worrying about the buffer end.
	   */
	  data[loc] = srp->feeder(srp->closure, dir);
	  data[loc + lim] = data[loc];
	  loc++;
	  if (loc == lim) loc = 0;
	}
      srp->start = loc; /* next time around we start here */
    }

  /* now loc = beginning of data */

  /* if (srx == 0.0) srx = 0.01; */ /* can't decide about this ... */
  if (srx < 0.0) srx = -srx;
  if (srx > 1.0) 
    {
      factor = 1.0 / srx;
      /* this is not exact since we're sampling the sinc and so on, but it's close over a wide range */
      zf = factor * (mus_float_t)SRC_SINC_DENSITY; 
      xi = (int)(zf + 0.5);

      /* (let ((e (make-env '(0 1 1 1.1) :length 11))) (src-channel e))
       */
      /* we're comparing adding xi lim times to zf and if there's no difference, using the int case */
      if (fabs((xi - zf) * lim) > 2.0) int_ok = false; else int_ok = true;
    }
  else 
    {
      factor = 1.0;
      zf = (mus_float_t)SRC_SINC_DENSITY;
      xi = SRC_SINC_DENSITY;
      int_ok = true;
    }

  sum = 0.0;
  if (int_ok)
    {
      int sinc_loc, sinc_incr, last, last10, xs;
      
      xs = (int)(zf * (srp->width_1 - srp->x));
      sinc_loc = xs + srp->sinc4;
      sinc_incr = xi;
      last = loc + lim;
      last10 = last - 10;

      while (loc <= last10)
	{
	  sum += data[loc++] * sinc_table[sinc_loc]; sinc_loc += sinc_incr;
	  sum += data[loc++] * sinc_table[sinc_loc]; sinc_loc += sinc_incr;
	  sum += data[loc++] * sinc_table[sinc_loc]; sinc_loc += sinc_incr;
	  sum += data[loc++] * sinc_table[sinc_loc]; sinc_loc += sinc_incr;
	  sum += data[loc++] * sinc_table[sinc_loc]; sinc_loc += sinc_incr;
	  sum += data[loc++] * sinc_table[sinc_loc]; sinc_loc += sinc_incr;
	  sum += data[loc++] * sinc_table[sinc_loc]; sinc_loc += sinc_incr;
	  sum += data[loc++] * sinc_table[sinc_loc]; sinc_loc += sinc_incr;
	  sum += data[loc++] * sinc_table[sinc_loc]; sinc_loc += sinc_incr;
	  sum += data[loc++] * sinc_table[sinc_loc]; sinc_loc += sinc_incr;
	}
      for (; loc < last; loc++, sinc_loc += sinc_incr)
	sum += data[loc] * sinc_table[sinc_loc];
    }
  else
    {
      mus_float_t sinc_loc, sinc_incr, x;
      int last, last10;

      x = zf * (srp->width_1 - srp->x);
      sinc_loc = x + srp->sinc4;
      sinc_incr = zf;
      last = loc + lim;

      last10 = last - 10;

      while (loc <= last10)
	{
	  sum += data[loc++] * sinc_table[(int)sinc_loc]; sinc_loc += sinc_incr;
	  sum += data[loc++] * sinc_table[(int)sinc_loc]; sinc_loc += sinc_incr;
	  sum += data[loc++] * sinc_table[(int)sinc_loc]; sinc_loc += sinc_incr;
	  sum += data[loc++] * sinc_table[(int)sinc_loc]; sinc_loc += sinc_incr;
	  sum += data[loc++] * sinc_table[(int)sinc_loc]; sinc_loc += sinc_incr;
	  sum += data[loc++] * sinc_table[(int)sinc_loc]; sinc_loc += sinc_incr;
	  sum += data[loc++] * sinc_table[(int)sinc_loc]; sinc_loc += sinc_incr;
	  sum += data[loc++] * sinc_table[(int)sinc_loc]; sinc_loc += sinc_incr;
	  sum += data[loc++] * sinc_table[(int)sinc_loc]; sinc_loc += sinc_incr;
	  sum += data[loc++] * sinc_table[(int)sinc_loc]; sinc_loc += sinc_incr;
	}
      for (; loc < last; loc++, sinc_loc += sinc_incr)
	sum += data[loc] * sinc_table[(int)sinc_loc];
    }

  srp->x += srx;
  return(sum * factor);
}


void mus_src_to_buffer(mus_any *srptr, mus_float_t (*input)(void *arg, int direction), mus_float_t *out_data, mus_long_t dur)
{
  /* sr_change = 0.0
   */
  sr *srp = (sr *)srptr;
  mus_float_t x, zf, srx, factor, sincx, srpx;
  int lim, i, xi, xs, dir = 1;
  bool int_ok;
  mus_long_t k;
  mus_float_t *data, *sinc_table;

  lim = srp->lim;
  sincx = (mus_float_t)SRC_SINC_DENSITY; 
  data = srp->data;
  sinc_table = srp->sinc_table;
  srx = srp->incr;
  srpx = srp->x;
  if (srx < 0.0) 
    {
      dir = -1;
      srx = -srx;
    }
  if (srx > 1.0) 
    {
      factor = 1.0 / srx;
      /* this is not exact since we're sampling the sinc and so on, but it's close over a wide range */
      zf = factor * sincx;
      xi = (int)zf;
      if (fabs((xi - zf) * lim) > 2.0) int_ok = false; else int_ok = true;
    }
  else 
    {
      factor = 1.0;
      zf = sincx;
      xi = SRC_SINC_DENSITY;
      int_ok = true;
    }

  for (k = 0; k < dur; k++)
    {
      int loc;
      mus_float_t sum;
      loc = srp->start;
      if (srpx >= 1.0)
	{
	  int fsx;
	  /* modf here is very slow??! */
	  fsx = (int)srpx;
 	  srpx -= fsx;

	  data[loc] = input(srp->closure, dir);
	  data[loc + lim] = data[loc];
	  loc++;
	  if (loc == lim) loc = 0;

	  for (i = 1; i < fsx; i++)
	    {
	      /* there are two copies of the circular data buffer back-to-back so that we can
	       *   run the convolution below without worrying about the buffer end.
	       */
	      data[loc] = input(srp->closure, dir);
	      data[loc + lim] = data[loc];
	      loc++;
	      if (loc == lim) loc = 0;
	    }
	  srp->start = loc; /* next time around we start here */
	}
      
      sum = 0.0;
      if (int_ok)
	{
	  int sinc_loc, sinc_incr, last, last10;
	  
	  xs = (int)(zf * (srp->width_1 - srpx));
	  sinc_loc = xs + srp->sinc4;
	  sinc_incr = xi;
	  last = loc + lim;
	  last10 = last - 10;
	  
	  while (loc <= last10)
	    {
	      sum += data[loc++] * sinc_table[sinc_loc]; sinc_loc += sinc_incr;
	      sum += data[loc++] * sinc_table[sinc_loc]; sinc_loc += sinc_incr;
	      sum += data[loc++] * sinc_table[sinc_loc]; sinc_loc += sinc_incr;
	      sum += data[loc++] * sinc_table[sinc_loc]; sinc_loc += sinc_incr;
	      sum += data[loc++] * sinc_table[sinc_loc]; sinc_loc += sinc_incr;
	      sum += data[loc++] * sinc_table[sinc_loc]; sinc_loc += sinc_incr;
	      sum += data[loc++] * sinc_table[sinc_loc]; sinc_loc += sinc_incr;
	      sum += data[loc++] * sinc_table[sinc_loc]; sinc_loc += sinc_incr;
	      sum += data[loc++] * sinc_table[sinc_loc]; sinc_loc += sinc_incr;
	      sum += data[loc++] * sinc_table[sinc_loc]; sinc_loc += sinc_incr;
	    }
	  for (; loc < last; loc++, sinc_loc += sinc_incr)
	    sum += data[loc] * sinc_table[sinc_loc];
	}
      else
	{
	  mus_float_t sinc_loc, sinc_incr;
	  int last, last10;
	  
	  x = zf * (srp->width_1 - srpx);
	  sinc_loc = x + srp->sinc4;
	  sinc_incr = zf;
	  last = loc + lim;
	  
	  last10 = last - 10;
	  
	  while (loc <= last10)
	    {
	      sum += data[loc++] * sinc_table[(int)sinc_loc]; sinc_loc += sinc_incr;
	      sum += data[loc++] * sinc_table[(int)sinc_loc]; sinc_loc += sinc_incr;
	      sum += data[loc++] * sinc_table[(int)sinc_loc]; sinc_loc += sinc_incr;
	      sum += data[loc++] * sinc_table[(int)sinc_loc]; sinc_loc += sinc_incr;
	      sum += data[loc++] * sinc_table[(int)sinc_loc]; sinc_loc += sinc_incr;
	      sum += data[loc++] * sinc_table[(int)sinc_loc]; sinc_loc += sinc_incr;
	      sum += data[loc++] * sinc_table[(int)sinc_loc]; sinc_loc += sinc_incr;
	      sum += data[loc++] * sinc_table[(int)sinc_loc]; sinc_loc += sinc_incr;
	      sum += data[loc++] * sinc_table[(int)sinc_loc]; sinc_loc += sinc_incr;
	      sum += data[loc++] * sinc_table[(int)sinc_loc]; sinc_loc += sinc_incr;
	    }
	  for (; loc < last; loc++, sinc_loc += sinc_incr)
	    sum += data[loc] * sinc_table[(int)sinc_loc];
	}
      srpx += srx;
      out_data[k] = sum * factor;
    }
  srp->x = srpx;
}


/* it was a cold, rainy day...
 *   and on an even colder day, I changed this to use a circular data buffer, rather than memmove 
 *   then changed yet again to use straight buffers
 */

mus_float_t *mus_src_20(mus_any *srptr, mus_float_t *in_data, mus_long_t dur)
{
  sr *srp = (sr *)srptr;
  int lim, i, width, wid1, wid10, xs, xi;
  mus_long_t k, dur2;
  mus_float_t *out_data, *ldata, *coeffs;
  
  dur2 = dur / 2 + 1;
  if ((dur & 1) != 0) dur2++;
  out_data = (mus_float_t *)malloc(dur2 * sizeof(mus_float_t));

  lim = srp->lim; /* 2 * width so it's even */
  width = srp->width;

  coeffs = (mus_float_t *)malloc(lim * sizeof(mus_float_t));
  if ((width & 1) != 0)
    xs = (int)((2 + width) * (SRC_SINC_DENSITY / 2)) + 4; /* Humph -- looks like crap -- maybe if odd width use the real one above, or insist on even width */
  else xs = (int)((1 + width) * (SRC_SINC_DENSITY / 2)) + 4;
  xi = SRC_SINC_DENSITY; /* skip a location (coeff=0.0) */

  for (i = 0; i < width; i++, xs += xi)
    coeffs[i] = srp->sinc_table[xs];

  for (i = 0; i < lim; i++)
    in_data[i] = srp->data[i];

  ldata = (mus_float_t *)in_data;
  wid10 = width - 10;
  wid1 = width - 1;

  for (k = 0; k < dur2; k++, ldata += 2)
    {
      int j;
      mus_float_t sum;
      sum = ldata[wid1];
      i = 0;
      j = 0;
      while (i <= wid10)
	{
	  sum += (ldata[j] * coeffs[i++]); j += 2;
	  sum += (ldata[j] * coeffs[i++]); j += 2;
	  sum += (ldata[j] * coeffs[i++]); j += 2;
	  sum += (ldata[j] * coeffs[i++]); j += 2;
	  sum += (ldata[j] * coeffs[i++]); j += 2;
	  sum += (ldata[j] * coeffs[i++]); j += 2;
	  sum += (ldata[j] * coeffs[i++]); j += 2;
	  sum += (ldata[j] * coeffs[i++]); j += 2;
	  sum += (ldata[j] * coeffs[i++]); j += 2;
	  sum += (ldata[j] * coeffs[i++]); j += 2;
	}
      for (; i < width; i++, j += 2)
	sum += (ldata[j] * coeffs[i]);
      out_data[k] = sum * 0.5;
    }

  free(coeffs);
  return(out_data);
}


mus_float_t *mus_src_05(mus_any *srptr, mus_float_t *in_data, mus_long_t dur)
{
  sr *srp = (sr *)srptr;
  int lim, i, width, wid1, wid10, xs, xi;
  mus_long_t k, dur2;
  mus_float_t *out_data, *ldata, *coeffs;
  
  dur2 = dur * 2;
  out_data = (mus_float_t *)malloc((dur2 + 1) * sizeof(mus_float_t));
  out_data[dur2] = 0.0;

  lim = srp->lim;
  width = srp->width;

  coeffs = (mus_float_t *)malloc(lim * sizeof(mus_float_t));
  xs = (SRC_SINC_DENSITY / 2) + 4;
  xi = SRC_SINC_DENSITY;

  for (i = 0; i < lim; i++, xs += xi)
    coeffs[i] = srp->sinc_table[xs];

  for (i = 0; i < lim; i++)
    in_data[i] = srp->data[i];

  ldata = (mus_float_t *)in_data;
  wid10 = lim - 10;
  wid1 = width - 1;

  for (k = 0; k < dur2; k += 2)
    {
      mus_float_t sum;
      out_data[k] = ldata[wid1];

      sum = 0.0;
      i = 0;
      while (i <= wid10)
	{
	  sum += (ldata[i] * coeffs[i]); i++;
	  sum += (ldata[i] * coeffs[i]); i++;
	  sum += (ldata[i] * coeffs[i]); i++;
	  sum += (ldata[i] * coeffs[i]); i++;
	  sum += (ldata[i] * coeffs[i]); i++;
	  sum += (ldata[i] * coeffs[i]); i++;
	  sum += (ldata[i] * coeffs[i]); i++;
	  sum += (ldata[i] * coeffs[i]); i++;
	  sum += (ldata[i] * coeffs[i]); i++;
	  sum += (ldata[i] * coeffs[i]); i++;
	}
      for (; i < lim; i++)
	sum += (ldata[i] * coeffs[i]);
      out_data[k + 1] = sum;

      ldata++;
    }

  free(coeffs);
  return(out_data);
}




/* ---------------- granulate ---------------- */

typedef struct {
  mus_any_class *core;
  mus_float_t (*rd)(void *arg, int direction);
  mus_float_t (*block_rd)(void *arg, int direction, mus_float_t *block, mus_long_t start, mus_long_t end);
  int s20;
  int s50;
  int rmp;
  mus_float_t amp, jitter;
  int cur_out;
  int input_hop;
  int ctr;
  int output_hop;
  mus_float_t *out_data;     /* output buffer */
  int out_data_len;
  mus_float_t *in_data;      /* input buffer */
  int in_data_len;
  void *closure;
  int (*edit)(void *closure);
  mus_float_t *grain;        /* grain data */
  int grain_len;
  bool first_samp;
  unsigned long randx; /* gen-local random number seed */
} grn_info;


bool mus_is_granulate(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_GRANULATE));
}


static bool granulate_equalp(mus_any *p1, mus_any *p2) {return(p1 == p2);}


static char *describe_granulate(mus_any *ptr)
{
  grn_info *gen = (grn_info *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s expansion: %.3f (%d/%d), scaler: %.3f, length: %.3f secs (%d samps), ramp: %.3f, jitter: %.3f",
	   mus_name(ptr),
	   (mus_float_t)(gen->output_hop) / (mus_float_t)(gen->input_hop),
	   gen->input_hop, gen->output_hop,
	   gen->amp,
	   (mus_float_t)(gen->grain_len) / (mus_float_t)sampling_rate, gen->grain_len,
	   (mus_float_t)(gen->rmp) / (mus_float_t)gen->grain_len,
	   gen->jitter);
  return(describe_buffer);
}


static void free_granulate(mus_any *ptr)
{
  grn_info *gen = (grn_info *)ptr;
  if (gen->out_data) free(gen->out_data);
  if (gen->in_data) free(gen->in_data);
  if (gen->grain) free(gen->grain);
  free(gen);
}

static mus_any *grn_info_copy(mus_any *ptr)
{
  grn_info *g, *p;
  int bytes;

  p = (grn_info *)ptr;
  g = (grn_info *)malloc(sizeof(grn_info));
  memcpy((void *)g, (void *)ptr, sizeof(grn_info));

  bytes = g->out_data_len * sizeof(mus_float_t);
  g->out_data = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->out_data, p->out_data, g->out_data_len);

  bytes = g->in_data_len * sizeof(mus_float_t);
  g->in_data = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->in_data, p->in_data, g->in_data_len);
  g->grain = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->grain, p->grain, g->in_data_len);
  
  return((mus_any *)g);
}

static mus_long_t grn_length(mus_any *ptr) {return(((grn_info *)ptr)->grain_len);}

static mus_long_t grn_set_length(mus_any *ptr, mus_long_t val) 
{
  grn_info *gen = ((grn_info *)ptr);
  if ((val > 0) && (val < gen->out_data_len)) 
    gen->grain_len = (int)val;                /* larger -> segfault */
  return(gen->grain_len);
}

static mus_float_t grn_scaler(mus_any *ptr) {return(((grn_info *)ptr)->amp);}
static mus_float_t grn_set_scaler(mus_any *ptr, mus_float_t val) {((grn_info *)ptr)->amp = val; return(val);}

static void grn_set_s20_and_s50(grn_info *gen)
{
  gen->s20 = 2 * (int)(gen->jitter * gen->output_hop);  /* was *.05 here and *.02 below */
   /* added "2 *" 21-Mar-05 and replaced irandom with (grn)mus_irandom below */
  gen->s50 = (int)(gen->jitter * gen->output_hop * 0.4); 
}

static mus_float_t grn_frequency(mus_any *ptr) {return(((mus_float_t)((grn_info *)ptr)->output_hop) / (mus_float_t)sampling_rate);}
static mus_float_t grn_set_frequency(mus_any *ptr, mus_float_t val)
{
  grn_info *gen = ((grn_info *)ptr);
  gen->output_hop = (int)((mus_float_t)sampling_rate * val);
  grn_set_s20_and_s50(gen);
  return(val);
}

static void *grn_closure(mus_any *rd) {return(((grn_info *)rd)->closure);}
static void *grn_set_closure(mus_any *rd, void *e) {((grn_info *)rd)->closure = e; return(e);}

static mus_float_t grn_increment(mus_any *ptr) 
{
  grn_info *gen = ((grn_info *)ptr);
  return(((mus_float_t)(gen->output_hop)) / ((mus_float_t)(gen->input_hop)));
}

static mus_float_t grn_set_increment(mus_any *ptr, mus_float_t val) 
{
  grn_info *gen = ((grn_info *)ptr);
  if (val != 0.0) 
    gen->input_hop = (int)(gen->output_hop / val); 
  return(val);
}

static mus_long_t grn_hop(mus_any *ptr) {return(((grn_info *)ptr)->output_hop);}
static mus_long_t grn_set_hop(mus_any *ptr, mus_long_t val) 
{
  grn_info *gen = ((grn_info *)ptr);
  gen->output_hop = (int)val;
  grn_set_s20_and_s50(gen);
  return(val);
}

static mus_long_t grn_ramp(mus_any *ptr) {return(((grn_info *)ptr)->rmp);}

static mus_long_t grn_set_ramp(mus_any *ptr, mus_long_t val)
{
  grn_info *gen = (grn_info *)ptr; 
  if (val < (gen->grain_len * .5))
    gen->rmp = (int)val;
  return(val);
}

static mus_float_t *granulate_data(mus_any *ptr) {return(((grn_info *)ptr)->grain);}
int mus_granulate_grain_max_length(mus_any *ptr) {return(((grn_info *)ptr)->in_data_len);}

static mus_long_t grn_location(mus_any *ptr) {return((mus_long_t)(((grn_info *)ptr)->randx));}
static mus_long_t grn_set_location(mus_any *ptr, mus_long_t val) {((grn_info *)ptr)->randx = (unsigned long)val; return(val);}

static mus_float_t grn_jitter(mus_any *ptr) {return(((grn_info *)ptr)->jitter);}
static mus_float_t grn_set_jitter(mus_any *ptr, mus_float_t val) /* K Matheussen 15-Jul-18 */
{ 
  grn_info *gen = (grn_info *)ptr; 
  gen->jitter = val; 
  grn_set_s20_and_s50(gen);
  return(val);
} 

static mus_float_t run_granulate(mus_any *ptr, mus_float_t unused1, mus_float_t unused2) {return(mus_granulate(ptr, NULL));}


static void grn_reset(mus_any *ptr)
{
  grn_info *gen = (grn_info *)ptr; 
  gen->cur_out = 0;
  gen->ctr = 0;
  mus_clear_floats(gen->out_data, gen->out_data_len);
  mus_clear_floats(gen->in_data, gen->in_data_len);
  mus_clear_floats(gen->grain, gen->in_data_len);
  gen->first_samp = true;
}


static int grn_irandom(grn_info *spd, int amp)
{
  /* gen-local next_random */
  spd->randx = spd->randx * 1103515245 + 12345;
  return((int)(amp * INVERSE_MAX_RAND2 * ((mus_float_t)((uint32_t)(spd->randx >> 16) & 32767))));
}


static mus_any_class GRANULATE_CLASS = {
  MUS_GRANULATE,
  (char *)S_granulate,
  &free_granulate,
  &describe_granulate,
  &granulate_equalp,
  &granulate_data, 0,
  &grn_length,    /* segment-length */
  &grn_set_length,
  &grn_frequency, /* spd-out */
  &grn_set_frequency,
  0, 0,
  &grn_scaler,    /* segment-scaler */
  &grn_set_scaler,
  &grn_increment,
  &grn_set_increment,
  &run_granulate,
  MUS_NOT_SPECIAL,
  &grn_closure,
  0,
  &grn_jitter, &grn_set_jitter, 
  0, 0, 0, 0, 
  &grn_hop, &grn_set_hop, 
  &grn_ramp, &grn_set_ramp,
  0, 0, 0, 0, 
  &grn_location, &grn_set_location, /* local randx */
  0, 0, 0, 0, 0,
  &grn_reset,
  &grn_set_closure,
  &grn_info_copy
};


mus_any *mus_make_granulate(mus_float_t (*input)(void *arg, int direction), 
			    mus_float_t expansion, mus_float_t length, mus_float_t scaler, 
			    mus_float_t hop, mus_float_t ramp, mus_float_t jitter, int max_size, 
			    int (*edit)(void *closure),
			    void *closure)
{
  grn_info *spd;
  int outlen;
  outlen = (int)(sampling_rate * (hop + length));
  if (max_size > outlen) outlen = max_size;
  if (expansion <= 0.0)
    {
      mus_error(MUS_ARG_OUT_OF_RANGE, S_make_granulate ": expansion must be > 0.0: %f", expansion);
      return(NULL);
    }
  if (outlen <= 0) 
    {
      mus_error(MUS_NO_LENGTH, S_make_granulate ": size is %d (hop: %f, segment-length: %f)?", outlen, hop, length);
      return(NULL);
    }
  if ((hop * sampling_rate) < expansion)
    {
      mus_error(MUS_ARG_OUT_OF_RANGE, S_make_granulate ": expansion (%f) must be < hop * srate (%f)", expansion, hop * sampling_rate);
      return(NULL);
    }
  spd = (grn_info *)malloc(sizeof(grn_info));
  spd->core = &GRANULATE_CLASS;
  spd->cur_out = 0;
  spd->ctr = 0;
  spd->grain_len = (int)(ceil(length * sampling_rate));
  spd->rmp = (int)(ramp * spd->grain_len);
  spd->amp = scaler;
  spd->jitter = jitter;
  spd->output_hop = (int)(hop * sampling_rate);
  spd->input_hop = (int)((mus_float_t)(spd->output_hop) / expansion);
  grn_set_s20_and_s50(spd);
  spd->out_data_len = outlen;
  spd->out_data = (mus_float_t *)calloc(spd->out_data_len, sizeof(mus_float_t));
  /* spd->in_data_len = outlen + spd->s20 + 1; */
  spd->in_data_len = outlen + (2 * sampling_rate * hop) + 1;
  spd->in_data = (mus_float_t *)malloc(spd->in_data_len * sizeof(mus_float_t));
  spd->rd = input; 
  spd->block_rd = NULL;
  spd->closure = closure;
  spd->edit = edit;
  spd->grain = (mus_float_t *)malloc(spd->in_data_len * sizeof(mus_float_t));
  spd->first_samp = true;
  spd->randx = mus_rand_seed(); /* caller can override this via the mus_location method */
  next_random();
  return((mus_any *)spd);
}


void mus_granulate_set_edit_function(mus_any *ptr, int (*edit)(void *closure))
{
  grn_info *gen = (grn_info *)ptr;
  if (!(gen->grain))
    gen->grain = (mus_float_t *)calloc(gen->in_data_len, sizeof(mus_float_t));
  gen->edit = edit;
}


mus_float_t mus_granulate_with_editor(mus_any *ptr, mus_float_t (*input)(void *arg, int direction), int (*edit)(void *closure))
{ 
  /* in_data_len is the max grain size (:maxsize arg), not the current grain size
   * out_data_len is the size of the output buffer
   * grain_len is the current grain size
   * cur_out is the out_data buffer location where we need to add in the next grain
   * ctr is where we are now in out_data
   */
  grn_info *spd = (grn_info *)ptr;
  mus_float_t result = 0.0;

  if (spd->ctr < spd->out_data_len)
    result = spd->out_data[spd->ctr]; /* else return 0.0 */
  spd->ctr++;

  if (spd->ctr >= spd->cur_out)       /* time for next grain */
    {
      /* set up edit/input functions and possible outside-accessible grain array */
      int i;
      int (*spd_edit)(void *closure) = edit;
      if (input) {spd->rd = input; spd->block_rd = NULL;}
      if (!spd_edit) spd_edit = spd->edit;

      if (spd->first_samp)
	{
	  /* fill up in_data, out_data is already cleared */
	  if (spd->block_rd)
	    spd->block_rd(spd->closure, 1, spd->in_data, 0, spd->in_data_len);
	  else
	    {
	      for (i = 0; i < spd->in_data_len; i++)
		spd->in_data[i] = spd->rd(spd->closure, 1);
	    }
	}
      else
	{

	  /* align output buffer to flush the data we've already output, and zero out new trailing portion */
	  if (spd->cur_out >= spd->out_data_len)
	    {
	      /* entire buffer has been output, and in fact we've been sending 0's for awhile to fill out hop */
	      mus_clear_floats(spd->out_data, spd->out_data_len); /* so zero the entire thing (it's all old) */
	    }
	  else 
	    {
	      /* move yet-un-output data to 0, zero trailers */
	      int good_samps;
	      good_samps = (spd->out_data_len - spd->cur_out);
	      memmove((void *)(spd->out_data), (void *)(spd->out_data + spd->cur_out), good_samps * sizeof(mus_float_t));
	      mus_clear_floats(spd->out_data + good_samps, spd->cur_out); /* must be cur_out trailing samples to 0 */
	    }

	  /* align input buffer */
	  if (spd->input_hop > spd->in_data_len)
	    {
	      /* need to flush enough samples to accommodate the fact that the hop is bigger than our data buffer */
	      for (i = spd->in_data_len; i < spd->input_hop; i++) spd->rd(spd->closure, 1);
	      /* then get a full input buffer */
	      if (spd->block_rd)
		spd->block_rd(spd->closure, 1, spd->in_data, 0, spd->in_data_len);
	      else
		{
		  for (i = 0; i < spd->in_data_len; i++)
		    spd->in_data[i] = spd->rd(spd->closure, 1);
		}
	    }
	  else
	    {
	      /* align input buffer with current input hop location */
	      int good_samps;
	      good_samps = (spd->in_data_len - spd->input_hop);
	      memmove((void *)(spd->in_data), (void *)(spd->in_data + spd->input_hop), good_samps * sizeof(mus_float_t));
	      if (spd->block_rd)
		spd->block_rd(spd->closure, 1, spd->in_data, good_samps, spd->in_data_len);
	      else
		{
		  for (i = good_samps; i < spd->in_data_len; i++)
		    spd->in_data[i] = spd->rd(spd->closure, 1);
		}
	    }
	}

      /* create current grain */
      {
	int lim, curstart, j;

	lim = spd->grain_len;
	curstart = grn_irandom(spd, spd->s20); /* start location in input buffer */
	if ((curstart + spd->grain_len) > spd->in_data_len)
	  lim = (spd->in_data_len - curstart);
	if (lim > spd->grain_len)
	  lim = spd->grain_len;
	else
	  {
	    if (lim < spd->grain_len)
	      mus_clear_floats(spd->grain, spd->grain_len - lim);
	  }
	if (spd->rmp > 0)
	  {
	    int steady_end, up_end;
	    mus_float_t amp = 0.0, incr;
	    steady_end = (spd->grain_len - spd->rmp);
	    incr = (mus_float_t)(spd->amp) / (mus_float_t)(spd->rmp);
	    up_end = spd->rmp;
	    if (up_end > lim) up_end = lim;
	    for (i = 0, j = curstart; i < up_end; i++, j++)
	      {
		spd->grain[i] = (amp * spd->in_data[j]);
		amp += incr; 
	      }
	    if (steady_end > lim) steady_end = lim;
	    for (; i < steady_end; i++, j++)
	      spd->grain[i] = (amp * spd->in_data[j]);
	    for (; i < lim; i++, j++)
	      {
		spd->grain[i] = (amp * spd->in_data[j]);
		amp -= incr; 
	      }
	  }
	else
	  {
	    /* ramp is 0.0, so just scale the input buffer by the current amp */
	    if (spd->amp == 1.0)
	      mus_copy_floats(spd->grain, spd->in_data + curstart, lim);
	    else
	      {
		for (i = 0, j = curstart; i < lim; i++, j++)
		  spd->grain[i] = (spd->amp * spd->in_data[j]);
	      }
	  }
      }

      /* add new grain into output buffer */
      {
	int new_len;
	if (spd_edit)
	  {
	    new_len = (*spd_edit)(spd->closure);
	    if (new_len <= 0)
	      new_len = spd->grain_len;
	    else
	      {
		if (new_len > spd->out_data_len)
		  new_len = spd->out_data_len;
	      }
	  }
	else new_len = spd->grain_len;
	if (new_len > spd->out_data_len) /* can be off-by-one here if hop is just barely greater then 0.0 (user is screwing around...) */
	  new_len = spd->out_data_len;
	mus_add_floats(spd->out_data, spd->grain, new_len);
      }
      
      /* set location of next grain */
      spd->ctr = 0;
      spd->cur_out = spd->output_hop + grn_irandom(spd, 2 * spd->s50) - spd->s50; /* irandom is 0..x */
      /* this form suggested by Marc Lehmann */
      /* "2 *" added 21-Mar-05 and irandom replaced with mus_irandom, grn_irandom 28-Feb-06 */
      /* in clm-2 (2004) it was spd->cur_out = spd->output_hop + irandom(spd->s50) */
      /* use of gen-local random sequence suggested by Kjetil Matheussen (to keep multi-channel grns in sync) */
      if (spd->cur_out < 0) spd->cur_out = 0;

      if (spd->first_samp)
	{
	  spd->first_samp = false;
	  spd->ctr = 1;
	  return(spd->out_data[0]);
	}
    }
  return(result);
}


mus_float_t mus_granulate(mus_any *ptr, mus_float_t (*input)(void *arg, int direction))
{
  return(mus_granulate_with_editor(ptr, input, NULL));
}



/* ---------------- Fourier transform ---------------- */

/* fft of mus_float_t data in zero-based arrays
 */

static void mus_big_fft(mus_float_t *rl, mus_float_t *im, mus_long_t n, int is);

#if HAVE_FFTW3 && HAVE_COMPLEX_TRIG

static fftw_complex *c_in_data = NULL, *c_out_data = NULL;
static fftw_plan c_r_plan, c_i_plan;  
static int last_c_fft_size = 0;   

static void mus_fftw_with_imag(mus_float_t *rl, mus_float_t *im, int n, int dir)
{
  int i, n4;

  if (n != last_c_fft_size)
    {
      if (c_in_data) 
	{
	  fftw_free(c_in_data); 
	  fftw_free(c_out_data); 
	  fftw_destroy_plan(c_r_plan); 
	  fftw_destroy_plan(c_i_plan);
	}
      c_in_data = (fftw_complex *)fftw_malloc(n * sizeof(fftw_complex)); /* rl/im data is mus_float_t */
      c_out_data = (fftw_complex *)fftw_malloc(n * sizeof(fftw_complex));
      c_r_plan = fftw_plan_dft_1d(n, c_in_data, c_out_data, FFTW_FORWARD, FFTW_ESTIMATE); 
      c_i_plan = fftw_plan_dft_1d(n, c_in_data, c_out_data, FFTW_BACKWARD, FFTW_ESTIMATE);
      last_c_fft_size = n;
    }

  n4 = n - 4;
  i = 0;
  while (i <= n4)
    {
      /* adding code to avoid this loop saves essentially nothing, mainly because the great majority of the calls
       *   are actually handling two real arrays at once -- the imag=0 case is 1/10 of the total.  In the zero case,
       *   the savings here is about 10%, but that is swamped by the fft itself (say 5-10 in c*).
       * using the new split array code (see below) saves essentially nothing -- perhaps 1 to 2% overall.
       */
      c_in_data[i] = rl[i] + _Complex_I * im[i];
      i++;
      c_in_data[i] = rl[i] + _Complex_I * im[i];
      i++;
      c_in_data[i] = rl[i] + _Complex_I * im[i];
      i++;
      c_in_data[i] = rl[i] + _Complex_I * im[i];
      i++;
    }
  for (; i < n; i++) 
    c_in_data[i] = rl[i] + _Complex_I * im[i];

  if (dir == -1) 
    fftw_execute(c_r_plan);
  else fftw_execute(c_i_plan);

  i = 0;
  while (i <= n4)
    {
      rl[i] = creal(c_out_data[i]);
      im[i] = cimag(c_out_data[i]);
      i++;
      rl[i] = creal(c_out_data[i]);
      im[i] = cimag(c_out_data[i]);
      i++;
      rl[i] = creal(c_out_data[i]);
      im[i] = cimag(c_out_data[i]);
      i++;
      rl[i] = creal(c_out_data[i]);
      im[i] = cimag(c_out_data[i]);
      i++;
    }
  for (; i < n; i++) 
    {
      rl[i] = creal(c_out_data[i]);
      im[i] = cimag(c_out_data[i]);
    }
}


void mus_fft(mus_float_t *rl, mus_float_t *im, mus_long_t n, int is)
{
  /* simple timing tests indicate fftw is slightly faster than mus_fft in this context
   */
  if (n < (1 << 30))
    mus_fftw_with_imag(rl, im, n, is);
  else mus_big_fft(rl, im, n, is);
}

#else

static void mus_scramble(mus_float_t *rl, mus_float_t *im, int n)
{
  /* bit reversal */

  int i, j;
  mus_float_t vr, vi;
  j = 0;
  for (i = 0; i < n; i++)
    {
      int m;
      if (j > i)
	{
	  vr = rl[j];
	  vi = im[j];
	  rl[j] = rl[i];
	  im[j] = im[i];
	  rl[i] = vr;
	  im[i] = vi;
	}
      m = n >> 1;
      while ((m >= 2) && (j >= m))
	{
	  j -= m;
	  m = m >> 1;
	}
      j += m;
    }
}


void mus_fft(mus_float_t *rl, mus_float_t *im, mus_long_t n, int is)
{
  /* standard fft: real part in rl, imaginary in im,
   * rl and im are zero-based.
   * see fxt/simplfft/fft.c (Joerg Arndt) 
   */
  int m, j, mh, ldm, lg, i, i2, j2, imh;
  mus_float_t u, vr, vi, angle;

  if (n >= (1 << 30))
    {
      mus_big_fft(rl, im, n, is);
      return;
    }

  imh = (int)(log(n + 1) / log(2.0));
  mus_scramble(rl, im, n);
  m = 2;
  ldm = 1;
  mh = n >> 1;
  angle = (M_PI * is);
  for (lg = 0; lg < imh; lg++)
    {
      mus_float_t c, s, ur, ui;
      c = cos(angle);
      s = sin(angle);
      ur = 1.0;
      ui = 0.0;
      for (i2 = 0; i2 < ldm; i2++)
	{
	  i = i2;
	  j = i2 + ldm;
	  for (j2 = 0; j2 < mh; j2++)
	    {
	      vr = ur * rl[j] - ui * im[j];
	      vi = ur * im[j] + ui * rl[j];
	      rl[j] = rl[i] - vr;
	      im[j] = im[i] - vi;
	      rl[i] += vr;
	      im[i] += vi;
	      i += m;
	      j += m;
	    }
	  u = ur;
	  ur = (ur * c) - (ui * s);
	  ui = (ui * c) + (u * s);
	}
      mh >>= 1;
      ldm = m;
      angle *= 0.5;
      m <<= 1;
    }
}
#endif


static void mus_big_fft(mus_float_t *rl, mus_float_t *im, mus_long_t n, int is)
{
  mus_long_t m, j, mh, ldm, i, i2, j2;
  int imh, lg;
  mus_float_t u, vr, vi, angle;

  imh = (int)(log(n + 1) / log(2.0));

  j = 0;
  for (i = 0; i < n; i++)
    {
      if (j > i)
	{
	  vr = rl[j];
	  vi = im[j];
	  rl[j] = rl[i];
	  im[j] = im[i];
	  rl[i] = vr;
	  im[i] = vi;
	}
      m = n >> 1;
      while ((m >= 2) && (j >= m))
	{
	  j -= m;
	  m = m >> 1;
	}
      j += m;
    }

  m = 2;
  ldm = 1;
  mh = n >> 1;
  angle = (M_PI * is);
  for (lg = 0; lg < imh; lg++)
    {
      mus_float_t c, s, ur, ui;
      c = cos(angle);
      s = sin(angle);
      ur = 1.0;
      ui = 0.0;
      for (i2 = 0; i2 < ldm; i2++)
	{
	  i = i2;
	  j = i2 + ldm;
	  for (j2 = 0; j2 < mh; j2++)
	    {
	      vr = ur * rl[j] - ui * im[j];
	      vi = ur * im[j] + ui * rl[j];
	      rl[j] = rl[i] - vr;
	      im[j] = im[i] - vi;
	      rl[i] += vr;
	      im[i] += vi;
	      i += m;
	      j += m;
	    }
	  u = ur;
	  ur = (ur * c) - (ui * s);
	  ui = (ui * c) + (u * s);
	}
      mh >>= 1;
      ldm = m;
      angle *= 0.5;
      m <<= 1;
    }
}


#if HAVE_GSL
#include <gsl/gsl_sf_bessel.h>

mus_float_t mus_bessi0(mus_float_t x)
{
  gsl_sf_result res;
  gsl_sf_bessel_I0_e(x, &res);
  return((mus_float_t)(res.val));
}

#else

mus_float_t mus_bessi0(mus_float_t x)
{ 
  if (x == 0.0) return(1.0);
  if (fabs(x) <= 15.0) 
    {
      mus_float_t z, denominator, numerator;
      z = x * x;
      numerator = (z * (z * (z * (z * (z * (z * (z * (z * (z * (z * (z * (z * (z * (z * 
										    0.210580722890567e-22 + 0.380715242345326e-19) +
									       0.479440257548300e-16) + 0.435125971262668e-13) +
								     0.300931127112960e-10) + 0.160224679395361e-7) +
							   0.654858370096785e-5) + 0.202591084143397e-2) +
						 0.463076284721000e0) + 0.754337328948189e2) +
				       0.830792541809429e4) + 0.571661130563785e6) +
			     0.216415572361227e8) + 0.356644482244025e9) +
		   0.144048298227235e10);
      denominator = (z * (z * (z - 0.307646912682801e4) +
			  0.347626332405882e7) - 0.144048298227235e10);
      return(-numerator / denominator);
    } 
  return(1.0);
}
#endif


#if HAVE_COMPLEX_TRIG || HAVE_GSL
static mus_float_t ultraspherical(int n, mus_float_t x, mus_float_t lambda)
{
  /* this is also the algorithm used in gsl gegenbauer.c -- slow but not as bad as using the binomials! */
  mus_float_t fn1, fn2 = 1.0, fn = 1.0;
  int k;
  if (n == 0) return(1.0);
  if (lambda == 0.0)
    fn1 = 2.0 * x;
  else fn1 = 2.0 * x * lambda;
  if (n == 1) return(fn1);
  for (k = 2; k <= n; k++)
    {
      fn = ((2.0 * x * (k + lambda - 1.0) * fn1) - ((k + (2.0 * lambda) - 2.0) * fn2)) / (mus_float_t)k;
      fn2 = fn1;
      fn1 = fn;
    }
  return(fn);
}
#endif


bool mus_is_fft_window(int val)
{
  switch (val)
    {
    case MUS_RECTANGULAR_WINDOW: case MUS_HANN_WINDOW: case MUS_WELCH_WINDOW: case MUS_PARZEN_WINDOW: 
    case MUS_BARTLETT_WINDOW: case MUS_HAMMING_WINDOW: case MUS_BLACKMAN2_WINDOW: case MUS_BLACKMAN3_WINDOW: 
    case MUS_BLACKMAN4_WINDOW: case MUS_EXPONENTIAL_WINDOW: case MUS_RIEMANN_WINDOW: case MUS_KAISER_WINDOW: 
    case MUS_CAUCHY_WINDOW: case MUS_POISSON_WINDOW: case MUS_GAUSSIAN_WINDOW: case MUS_TUKEY_WINDOW: 
    case MUS_DOLPH_CHEBYSHEV_WINDOW: case MUS_HANN_POISSON_WINDOW: case MUS_CONNES_WINDOW: 
    case MUS_SAMARAKI_WINDOW: case MUS_ULTRASPHERICAL_WINDOW: case MUS_BARTLETT_HANN_WINDOW: 
    case MUS_BOHMAN_WINDOW: case MUS_FLAT_TOP_WINDOW: case MUS_BLACKMAN5_WINDOW: case MUS_BLACKMAN6_WINDOW: 
    case MUS_BLACKMAN7_WINDOW: case MUS_BLACKMAN8_WINDOW: case MUS_BLACKMAN9_WINDOW: case MUS_BLACKMAN10_WINDOW: 
    case MUS_RV2_WINDOW: case MUS_RV3_WINDOW: case MUS_RV4_WINDOW: case MUS_MLT_SINE_WINDOW: 
    case MUS_PAPOULIS_WINDOW: case MUS_DPSS_WINDOW: case MUS_SINC_WINDOW:
      return(true);
    }
  return(false);
}


#if HAVE_GSL
  #include <gsl/gsl_version.h>
  #if ((GSL_MAJOR_VERSION >= 1) && (GSL_MINOR_VERSION >= 9))
    #include <gsl/gsl_math.h>
    #include <gsl/gsl_eigen.h>
    #define HAVE_GSL_EIGEN_NONSYMMV_WORKSPACE 1
  #endif
#endif


static mus_float_t sqr(mus_float_t x) {return(x * x);}


mus_float_t *mus_make_fft_window_with_window(mus_fft_window_t type, mus_long_t size, mus_float_t beta, mus_float_t mu, mus_float_t *window)
{
  /* mostly taken from
   *    Fredric J. Harris, "On the Use of Windows for Harmonic Analysis with the
   *    Discrete Fourier Transform," Proceedings of the IEEE, Vol. 66, No. 1,
   *    January 1978.
   *
   *    Albert H. Nuttall, "Some Windows with Very Good Sidelobe Behaviour", 
   *    IEEE Transactions of Acoustics, Speech, and Signal Processing, Vol. ASSP-29,
   *    No. 1, February 1981, pp 84-91
   */

  mus_long_t i, j, midn, midp1;
  mus_float_t freq, rate, angle = 0.0, cx;
  if (!window) return(NULL);

  midn = size >> 1;
  if (midn == 0) return(window);
  midp1 = (size + 1) / 2;
  freq = TWO_PI / (mus_float_t)size;
  rate = 1.0 / (mus_float_t)midn;

  switch (type)
    {
    case MUS_RECTANGULAR_WINDOW:
      for (i = 0; i < size; i++) 
	window[i] = 1.0;
      break; 

    case MUS_WELCH_WINDOW:
      for (i = 0, j = size - 1; i <= midn; i++, j--) 
	{
	  window[i] = 1.0 - sqr((mus_float_t)(i - midn) / (mus_float_t)midp1);
	  window[j] = window[i];
	}
      break; 

    case MUS_CONNES_WINDOW:
      for (i = 0, j = size - 1; i <= midn; i++, j--)
	{
	  window[i] = sqr(1.0 - sqr((mus_float_t)(i - midn) / (mus_float_t)midp1));
	  window[j] = window[i];
	}
      break; 

    case MUS_PARZEN_WINDOW:
      for (i = 0, j = size - 1; i <= midn; i++, j--)
	{
	  window[i] = 1.0 - fabs((mus_float_t)(i - midn) / (mus_float_t)midp1);
	  window[j] = window[i];
	}
      break; 

    case MUS_BARTLETT_WINDOW:
      for (i = 0, j = size - 1, angle = 0.0; i <= midn; i++, j--, angle += rate)
	{
	  window[i] = angle;
	  window[j] = angle;
	}
      break; 

    case MUS_BARTLETT_HANN_WINDOW:
      {
	mus_float_t ramp;
	rate *= 0.5;
	/* this definition taken from mathworks docs: they use size - 1 throughout -- this makes very little
	 *    difference unless you're using a small window.  I decided to be consistent with all the other
	 *    windows, and besides, this way actually peaks at 1.0 (which matlab misses)
	 */
	for (i = 0, j = size - 1, angle = -M_PI, ramp = 0.5; i <= midn; i++, j--, angle += freq, ramp -= rate)
	  {
	    window[i] = 0.62 - 0.48 * ramp + 0.38 * cos(angle);
	    window[j] = window[i];
	  }
      }
      break; 

    case MUS_BOHMAN_WINDOW:
      {
	mus_float_t ramp;
	/* definition from diracdelta docs and "DSP Handbook" -- used in bispectrum ("minimum bispectrum bias supremum") */
	for (i = 0, j = size - 1, angle = M_PI, ramp = 0.0; i <= midn; i++, j--, angle -= freq, ramp += rate)
	  {
	    window[i] = ramp * cos(angle) + (sin(angle) / M_PI);
	    window[j] = window[i];
	  }
      }
      break; 

    case MUS_HANN_WINDOW:
      for (i = 0, j = size - 1, angle = 0.0; i <= midn; i++, j--, angle += freq) 
	{
	  window[i] = 0.5 - 0.5 * cos(angle); 
	  window[j] = window[i];
	}
      break; 

      /* Rife-Vincent windows are an elaboration of this (Hann = RV1) */

    case MUS_RV2_WINDOW:
      for (i = 0, j = size - 1, angle = 0.0; i <= midn; i++, j--, angle += freq) 
	{
	  window[i] = .375 - 0.5 * cos(angle) + .125 * cos(2 * angle);
	  window[j] = window[i];
	}
      break; 

    case MUS_RV3_WINDOW:
      for (i = 0, j = size - 1, angle = 0.0; i <= midn; i++, j--, angle += freq) 
	{
	  window[i] = (10.0 / 32.0) -
	              (15.0 / 32.0) * cos(angle) + 
                       (6.0 / 32.0) * cos(2 * angle) - 
                       (1.0 / 32.0) * cos(3 * angle);
	  window[j] = window[i];
	}
      break; 

    case MUS_RV4_WINDOW:
      for (i = 0, j = size - 1, angle = 0.0; i <= midn; i++, j--, angle += freq) 
	{
	  window[i] = (35.0 / 128.0) - 
	              (56.0 / 128.0) * cos(angle) + 
	              (28.0 / 128.0) * cos(2 * angle) - 
	               (8.0 / 128.0) * cos(3 * angle) + 
	               (1.0 / 128.0) * cos(4 * angle);
	  window[j] = window[i];
	}
      break; 

    case MUS_HAMMING_WINDOW:
      for (i = 0, j = size - 1, angle = 0.0; i <= midn; i++, j--, angle += freq) 
	{
	  window[i] = 0.54 - 0.46 * cos(angle);
	  window[j] = window[i];
	}
      break; 

      /* Blackman 1 is the same as Hamming */

    case MUS_BLACKMAN2_WINDOW: /* using Chebyshev polynomial equivalents here (this is also given as .42 .5 .08) */
      for (i = 0, j = size - 1, angle = 0.0; i <= midn; i++, j--, angle += freq) 
	{              /* (+ 0.42323 (* -0.49755 (cos a)) (* 0.07922 (cos (* a 2)))) */
	               /* "A Family...": .42438 .49341 .078279 */
	  cx = cos(angle);
	  window[i] = .34401 + (cx * (-.49755 + (cx * .15844)));
	  window[j] = window[i];
	}
      break; 

    case MUS_BLACKMAN3_WINDOW:
      for (i = 0, j = size - 1, angle = 0.0; i <= midn; i++, j--, angle += freq) 
	{              /* (+ 0.35875 (* -0.48829 (cos a)) (* 0.14128 (cos (* a 2))) (* -0.01168 (cos (* a 3)))) */
	               /* (+ 0.36336 (*  0.48918 (cos a)) (* 0.13660 (cos (* a 2))) (*  0.01064 (cos (* a 3)))) is "Nuttall" window? */
	               /* "A Family...": .36358 .489177 .136599 .0106411 */

	  cx = cos(angle);
	  window[i] = .21747 + (cx * (-.45325 + (cx * (.28256 - (cx * .04672)))));
	  window[j] = window[i];
	}
      break; 

    case MUS_BLACKMAN4_WINDOW:
      for (i = 0, j = size - 1, angle = 0.0; i <= midn; i++, j--, angle += freq) 
	{             /* (+ 0.287333 (* -0.44716 (cos a)) (* 0.20844 (cos (* a 2))) (* -0.05190 (cos (* a 3))) (* 0.005149 (cos (* a 4)))) */
	              /* "A Family...": .32321 .471492 .175534 .0284969 .001261357 */
	  cx = cos(angle);
	  window[i] = .084037 + (cx * (-.29145 + (cx * (.375696 + (cx * (-.20762 + (cx * .041194)))))));
	  window[j] = window[i];
	}
      break; 

      /* "A Family of Cosine-Sum Windows..." Albrecht */

    case MUS_BLACKMAN5_WINDOW:
      for (i = 0, j = size - 1, angle = 0.0; i <= midn; i++, j--, angle += freq) 
	{ 
	  /* .293557 -.451935 .201416 -.047926 .00502619 -.000137555 */
	  /*   partials->polynomial -> -0.196389809 -0.308844775 0.3626224697 -0.188952908 0.0402095206 -0.002200880, then fixup constant */
	  cx = cos(angle);
	  window[i] = 0.097167 + 
	    (cx * (-.3088448 + 
		   (cx * (.3626224 + 
			  (cx * (-.1889530 + 
				 (cx * (.04020952 + 
					(cx * -.0022008)))))))));
	  window[j] = window[i];
	}
      break; 

    case MUS_BLACKMAN6_WINDOW:
      for (i = 0, j = size - 1, angle = 0.0; i <= midn; i++, j--, angle += freq) 
	{ 
	  /* .2712203 -.4334446 .2180041 -.0657853 .010761867 -.0007700127 .00001368088 */
	  /*   partials->polynomial -> -0.207255900 -0.239938736 0.3501594961 
	   *                           -0.247740954 0.0854382589 -0.012320203 0.0004377882  
	   */
	  cx = cos(angle);
	  window[i] = 0.063964353 +
	    (cx * (-0.239938736 + 
		   (cx * (0.3501594961 + 
			  (cx * (-0.247740954 + 
				 (cx * (0.0854382589 + 
					(cx * (-0.012320203 + 
					       (cx * 0.0004377882)))))))))));
	  window[j] = window[i];
	}
      break; 

    case MUS_BLACKMAN7_WINDOW:
      for (i = 0, j = size - 1, angle = 0.0; i <= midn; i++, j--, angle += freq) 
	{ 
	  /* .2533176 -.4163269 .2288396 -.08157508 .017735924 -.0020967027 .00010677413 -.0000012807 */
	  /*   partials->polynomial -> -0.211210445 -0.182076216 0.3177137375 -0.284437984 
	   *                           0.1367622316 -0.033403806 0.0034167722 -0.000081965 
	   */
	  cx = cos(angle);
	  window[i] = 0.04210723 +
	    (cx * (-0.18207621 + 
		   (cx * (0.3177137375 + 
			  (cx * (-0.284437984 + 
				 (cx * (0.1367622316 + 
					(cx * (-0.033403806 + 
						(cx * (0.0034167722 + 
						       (cx * -0.000081965)))))))))))));
	  window[j] = window[i];
	}
      break; 

    case MUS_BLACKMAN8_WINDOW:
      for (i = 0, j = size - 1, angle = 0.0; i <= midn; i++, j--, angle += freq) 
	{ 
	  /* .2384331 -.4005545 .2358242 -.09527918 .025373955 -.0041524329  .00036856041 -.00001384355 .0000001161808 */
	  /*   partials->polynomial -> -0.210818693 -0.135382235 0.2752871215 -0.298843294 0.1853193194 
	   *                           -0.064888448 0.0117641902 -0.000885987 0.0000148711 
	   */
	  cx = cos(angle);
	  window[i] = 0.027614462 + 
	    (cx * (-0.135382235 + 
		   (cx * (0.2752871215 + 
			  (cx * (-0.298843294 + 
				 (cx * (0.1853193194 + 
					(cx * (-0.064888448 + 
						(cx * (0.0117641902 + 
						       (cx * (-0.000885987 +
							      (cx * 0.0000148711)))))))))))))));
	  window[j] = window[i];
	}
      break; 

    case MUS_BLACKMAN9_WINDOW:
      for (i = 0, j = size - 1, angle = 0.0; i <= midn; i++, j--, angle += freq) 
	{ 
	  /* .2257345 -.3860122 .2401294 -.1070542 .03325916 -.00687337  .0008751673 -.0000600859 .000001710716 -.00000001027272 */
	  /*   partials->polynomial -> -0.207743675 -0.098795950 0.2298837751 -0.294112951 0.2243389785 
	   *                           -0.103248745 0.0275674108 -0.003839580 0.0002189716 -0.000002630 
	   */
	  cx = cos(angle);
	  window[i] = 0.01799071953 + 
	    (cx * (-0.098795950 + 
		   (cx * (0.2298837751 + 
			  (cx * (-0.294112951 + 
				 (cx * (0.2243389785 + 
					(cx * (-0.103248745 + 
						(cx * (0.0275674108 + 
						       (cx * (-0.003839580 +
							       (cx * (0.0002189716 +
								      (cx * -0.000002630)))))))))))))))));
	  window[j] = window[i];
	}
      break; 

    case MUS_BLACKMAN10_WINDOW:
      for (i = 0, j = size - 1, angle = 0.0; i <= midn; i++, j--, angle += freq) 
	{ 
	  /* .2151527 -.3731348 .2424243 -.1166907 .04077422 -.01000904 .0016398069 -.0001651660 .000008884663 -.000000193817 .00000000084824 */
	  /*   partials->polynomial -> -0.203281015 -0.071953468 0.1878870875 -0.275808066 
	   *                            0.2489042133 -0.141729787 0.0502002984 -0.010458985 0.0011361511 -0.000049617 0.0000004343  
	   */
	  cx = cos(angle);
	  window[i] = 0.0118717384 + 
	    (cx * (-0.071953468 + 
		   (cx * (0.1878870875 + 
			  (cx * (-0.275808066 + 
				 (cx * (0.2489042133 + 
					(cx * (-0.141729787 + 
						(cx * (0.0502002984 + 
						       (cx * (-0.010458985 +
							       (cx * (0.0011361511 +
								       (cx * (-0.000049617 +
									      (cx * 0.0000004343)))))))))))))))))));
	  window[j] = window[i];
	}
      break; 

    case MUS_FLAT_TOP_WINDOW:
      /* this definition taken from mathworks docs -- see above */
      for (i = 0, j = size - 1, angle = 0.0; i <= midn; i++, j--, angle += freq)
	{
	  window[i] = 0.2156 - 0.4160 * cos(angle) + 0.2781 * cos(2 * angle) - 0.0836 * cos(3 * angle) + 0.0069 * cos(4 * angle);
	  window[j] = window[i];
	}
      break; 

    case MUS_EXPONENTIAL_WINDOW:
      {
	mus_float_t expn, expsum = 1.0;
	expn = log(2) / (mus_float_t)midn + 1.0;
	for (i = 0, j = size - 1; i <= midn; i++, j--) 
	  {
	    window[i] = expsum - 1.0; 
	    window[j] = window[i];
	    expsum *= expn;
	  }
      }
      break;

    case MUS_KAISER_WINDOW:
      {
	mus_float_t I0beta;
	I0beta = mus_bessi0(beta); /* Harris multiplies beta by pi */
	for (i = 0, j = size - 1, angle = 1.0; i <= midn; i++, j--, angle -= rate)
	  {
	    window[i] = mus_bessi0(beta * sqrt(1.0 - sqr(angle))) / I0beta;
	    window[j] = window[i];
	  }
      }
      break;

    case MUS_CAUCHY_WINDOW:
      for (i = 0, j = size - 1, angle = 1.0; i <= midn; i++, j--, angle -= rate)
	{
	  window[i] = 1.0 / (1.0 + sqr(beta * angle));
	  window[j] = window[i];
	}
      break;

    case MUS_POISSON_WINDOW:
      for (i = 0, j = size - 1, angle = 1.0; i <= midn; i++, j--, angle -= rate)
	{
	  window[i] = exp((-beta) * angle);
	  window[j] = window[i];
	}
      break;

    case MUS_HANN_POISSON_WINDOW:
      /* Hann * Poisson -- from JOS */
      {
	mus_float_t angle1;
	for (i = 0, j = size - 1, angle = 1.0, angle1 = 0.0; i <= midn; i++, j--, angle -= rate, angle1 += freq)
	  {
	    window[i] = exp((-beta) * angle) * (0.5 - 0.5 * cos(angle1));
	    window[j] = window[i];
	  }
      }
      break;

    case MUS_RIEMANN_WINDOW:
      {
	mus_float_t sr1;
	sr1 = TWO_PI / (mus_float_t)size;
	for (i = 0, j = size - 1; i <= midn; i++, j--) 
	  {
	    if (i == midn) 
	      window[i] = 1.0;
	    else 
	      {
		cx = sr1 * (midn - i);
		window[i] = sin(cx) / cx;
	      }
	    window[j] = window[i];
	  }
      }
      break;

    case MUS_GAUSSIAN_WINDOW:
      for (i = 0, j = size - 1, angle = 1.0; i <= midn; i++, j--, angle -= rate)
	{
	  window[i] = exp(-.5 * sqr(beta * angle));
	  window[j] = window[i];
	}
      break;

    case MUS_TUKEY_WINDOW:
      cx = midn * (1.0 - beta);
      for (i = 0, j = size - 1; i <= midn; i++, j--) 
	{
	  if (i >= cx) 
	    window[i] = 1.0;
	  else window[i] = .5 * (1.0 - cos(M_PI * i / cx));
	  window[j] = window[i];
	}
      break;

    case MUS_MLT_SINE_WINDOW:
      {
	mus_float_t scl;
	scl = M_PI / (mus_float_t)size;
	for (i = 0, j = size - 1; i <= midn; i++, j--)
	  {
	    window[i] = sin((i + 0.5) * scl);
	    window[j] = window[i];
	  }
      }
      break;
      
    case MUS_PAPOULIS_WINDOW:
      {
	int n2;
	n2 = size / 2;
	for (i = -n2; i < n2; i++)
	  {
	    mus_float_t ratio, pratio;
	    ratio = (mus_float_t)i / (mus_float_t)n2;
	    pratio = M_PI * ratio;
	    window[i + n2] = (fabs(sin(pratio)) / M_PI) + (cos(pratio) * (1.0 - fabs(ratio)));
	  }
      }
      break;

    case MUS_SINC_WINDOW:
      {
	mus_float_t scl;
	scl = 2 * M_PI / (size - 1);
	for (i = -midn, j = 0; i < midn; i++, j++)
	  {
	    if (i == 0)
	      window[j] = 1.0;
	    else window[j] = sin(i * scl) / (i * scl);
	  }
      }
      break;

    case MUS_DPSS_WINDOW:
#if HAVE_GSL_EIGEN_NONSYMMV_WORKSPACE
      {
	/* from Verma, Bilbao, Meng, "The Digital Prolate Spheroidal Window"
	 *   output checked using Julius Smith's dpssw.m, although my "beta" is different
	 */
	double *data; /* "double" for gsl func */
	double cw, n1, pk = 0.0;

	cw = cos(2 * M_PI * beta);
	n1 = (size - 1) * 0.5;
	if ((mus_long_t)(size * size * sizeof(double)) > mus_max_malloc())
	  {
	    mus_error(MUS_ARG_OUT_OF_RANGE, S_make_fft_window ": dpss window requires size^2 * 8 bytes, but that exceeds the current mus-max-malloc amount");
	    return(window);
	  }

	data = (double *)calloc(size * size, sizeof(double));
	for (i = 0; i < size; i++)
	  {
	    double n2;
	    n2 = n1 - i;
	    data[i * size + i] = cw * n2 * n2;
	    if (i < (size - 1))
	      data[i * (size + 1) + 1] = 0.5 * (i + 1) * (size - 1 - i);
	    if (i > 0)
	      data[i * (size + 1) - 1] = 0.5 * i * (size - i);
	  }
	{
	  gsl_vector_complex_view evec_i;
	  gsl_matrix_view m = gsl_matrix_view_array(data, size, size);
	  gsl_vector_complex *eval = gsl_vector_complex_alloc(size);
	  gsl_matrix_complex *evec = gsl_matrix_complex_alloc(size, size);

	  gsl_eigen_nonsymmv_workspace *w = gsl_eigen_nonsymmv_alloc(size);
	  gsl_eigen_nonsymmv(&m.matrix, eval, evec, w);
	  gsl_eigen_nonsymmv_free(w);

	  gsl_eigen_nonsymmv_sort(eval, evec, GSL_EIGEN_SORT_ABS_DESC);
	  evec_i = gsl_matrix_complex_column(evec, 0);

	  for (j = 0; j < size; j++)
	    window[j] = GSL_REAL(gsl_vector_complex_get(&evec_i.vector, j));

	  gsl_vector_complex_free(eval);
	  gsl_matrix_complex_free(evec);
	}
	
	for (i = 0; i < size; i++)
	  if (fabs(window[i]) > fabs(pk))
	    pk = window[i];
	if (pk != 0.0)
	  for (i = 0; i < size; i++)
	    window[i] /= pk;

	free(data);
      }
#else
      mus_error(MUS_NO_SUCH_FFT_WINDOW, S_make_fft_window ": DPSS window needs GSL");
#endif
      break;

    case MUS_ULTRASPHERICAL_WINDOW:
    case MUS_SAMARAKI_WINDOW:
    case MUS_DOLPH_CHEBYSHEV_WINDOW:
      /* "Design of Ultraspherical Window Functions with Prescribed Spectral Characteristics", Bergen and Antoniou, EURASIP JASP 2004 */
      if (type == MUS_ULTRASPHERICAL_WINDOW)
	{
	  if (mu == 0.0)
	    type = MUS_DOLPH_CHEBYSHEV_WINDOW;
	  else
	    {
	      if (mu == 1.0)
		type = MUS_SAMARAKI_WINDOW;
	    }
	}

#if HAVE_COMPLEX_TRIG
      {
	mus_float_t *rl, *im;
	mus_float_t pk = 0.0;
	mus_float_t alpha;

	freq = M_PI / (mus_float_t)size;
	if (beta < 0.2) beta = 0.2;
	alpha = creal(ccosh(cacosh(pow(10.0, beta)) / (mus_float_t)size));

	rl = (mus_float_t *)malloc(size * sizeof(mus_float_t));
	im = (mus_float_t *)calloc(size, sizeof(mus_float_t));

	for (i = 0, angle = 0.0; i < size; i++, angle += freq)
	  {
	    switch (type)
	      {
	      case MUS_DOLPH_CHEBYSHEV_WINDOW:
		rl[i] = creal(ccos(cacos(alpha * cos(angle)) * size)); /* here is Tn (Chebyshev polynomial first kind) */
		break;

	      case MUS_SAMARAKI_WINDOW:
		/* Samaraki window uses Un instead */
		rl[i] = creal(csin(cacos(alpha * cos(angle)) * (size + 1.0)) / csin(cacos(alpha * cos(angle))));
		break;

	      case MUS_ULTRASPHERICAL_WINDOW:
		/* Cn here */
		rl[i] = ultraspherical(size, alpha * cos(angle), mu);
		break;

	      default: 
		break;
	      }
	  }

	mus_fft(rl, im, size, -1);    /* can be 1 here */

	pk = 0.0;
	for (i = 0; i < size; i++) 
	  if (pk < rl[i]) 
	    pk = rl[i];
	if ((pk != 0.0) && (pk != 1.0))
	  {
	    for (i = 0, j = size / 2; i < size; i++) 
	      {
		window[i] = rl[j++] / pk;
		if (j == size) j = 0;
	      }
	  }
	else
	  {
	    mus_copy_floats(window, rl, size);
	  }

	free(rl);
	free(im);
      }
#else
#if HAVE_GSL
      {
	mus_float_t *rl, *im;
	mus_float_t pk;
	mus_float_t alpha;

	freq = M_PI / (mus_float_t)size;
	if (beta < 0.2) beta = 0.2;
	alpha = GSL_REAL(gsl_complex_cosh(
			   gsl_complex_mul_real(
			     gsl_complex_arccosh_real(pow(10.0, beta)),
			     (mus_float_t)(1.0 / (mus_float_t)size))));

	rl = (mus_float_t *)malloc(size * sizeof(mus_float_t));
	im = (mus_float_t *)calloc(size, sizeof(mus_float_t));

	for (i = 0, angle = 0.0; i < size; i++, angle += freq)
	  {
	    switch (type)
	      {
	      case MUS_DOLPH_CHEBYSHEV_WINDOW:
		rl[i] = GSL_REAL(gsl_complex_cos(
			           gsl_complex_mul_real(
			             gsl_complex_arccos_real(alpha * cos(angle)),
				     (mus_float_t)size)));
		break;

	      case MUS_SAMARAKI_WINDOW:
		rl[i] = GSL_REAL(gsl_complex_div(
		                   gsl_complex_sin(
			             gsl_complex_mul_real(
			               gsl_complex_arccos_real(alpha * cos(angle)),
				       (mus_float_t)(size + 1.0))),
				   gsl_complex_sin(
				     gsl_complex_arccos_real(alpha * cos(angle)))));
		break;

	      case MUS_ULTRASPHERICAL_WINDOW:
		rl[i] = ultraspherical(size, alpha * cos(angle), mu);
		break;

	      default: 
		break;
	      }

	  }

	mus_fft(rl, im, size, -1);    /* can be 1 here */

	pk = 0.0;
	for (i = 0; i < size; i++) 
	  if (pk < rl[i]) 
	    pk = rl[i];
	if ((pk != 0.0) && (pk != 1.0))
	  {
	    for (i = 0, j = size / 2; i < size; i++) 
	      {
		window[i] = rl[j++] / pk;
		if (j == size) j = 0;
	      }
	  }
	else
	  {
	    mus_copy_floats(window, rl, size);
	  }
	free(rl);
	free(im);
      }
#else
      mus_error(MUS_NO_SUCH_FFT_WINDOW, S_make_fft_window ": Dolph-Chebyshev, Samaraki, and Ultraspherical windows need complex trig support");
#endif
#endif
      break;

    default: 
      mus_error(MUS_NO_SUCH_FFT_WINDOW, S_make_fft_window ": unknown fft data window: %d", (int)type); 
      break;
    }
  return(window);
}


mus_float_t *mus_make_fft_window(mus_fft_window_t type, mus_long_t size, mus_float_t beta)
{
  return(mus_make_fft_window_with_window(type, size, beta, 0.0, (mus_float_t *)calloc(size, sizeof(mus_float_t))));
}


static const char *fft_window_names[MUS_NUM_FFT_WINDOWS] = 
  {"Rectangular", "Hann", "Welch", "Parzen", "Bartlett", "Hamming", "Blackman2", "Blackman3", "Blackman4",
   "Exponential", "Riemann", "Kaiser", "Cauchy", "Poisson", "Gaussian", "Tukey", "Dolph-Chebyshev", "Hann-Poisson", "Connes",
   "Samaraki", "Ultraspherical", "Bartlett-Hann", "Bohman", "Flat-top",
   "Blackman5", "Blackman6", "Blackman7", "Blackman8", "Blackman9", "Blackman10",
   "Rife-Vincent2", "Rife-Vincent3", "Rife-Vincent4", "MLT Sine", "Papoulis", "DPSS (Slepian)", "Sinc"
};


const char *mus_fft_window_name(mus_fft_window_t win)
{
  if (mus_is_fft_window((int)win))
    return(fft_window_names[(int)win]);
  return("unknown");
}


const char **mus_fft_window_names(void)
{
  return(fft_window_names);
}


mus_float_t *mus_spectrum(mus_float_t *rdat, mus_float_t *idat, mus_float_t *window, mus_long_t n, mus_spectrum_t type)
{
  mus_long_t i;
  mus_float_t maxa, lowest;

  if (window) 
    {
      for (i = 0; i < n; i++) 
	rdat[i] *= window[i];
    }
  mus_clear_floats(idat, n);
  mus_fft(rdat, idat, n, 1);

  lowest = 0.000001;
  maxa = 0.0;
  n = n / 2;
  for (i = 0; i < n; i++)
    {
      mus_float_t val;
      val = rdat[i] * rdat[i] + idat[i] * idat[i];
      if (val < lowest)
	rdat[i] = 0.001;
      else 
	{
	  rdat[i] = sqrt(val);
	  if (rdat[i] > maxa) maxa = rdat[i];
	}
    }
  if (maxa > 0.0)
    {
      maxa = 1.0 / maxa;
      if (type == MUS_SPECTRUM_IN_DB)
	{
	  mus_float_t todb;
	  todb = 20.0 / log(10.0);
	  for (i = 0; i < n; i++) 
	    rdat[i] = todb * log(rdat[i] * maxa);
	}
      else
	{
	  if (type == MUS_SPECTRUM_NORMALIZED)
	    for (i = 0; i < n; i++) 
	      rdat[i] *= maxa;
	}
    }
  return(rdat);
}


mus_float_t *mus_autocorrelate(mus_float_t *data, mus_long_t n)
{
  mus_float_t *im;
  mus_float_t fscl;
  mus_long_t i, n2;

  n2 = n / 2;
  fscl = 1.0 / (mus_float_t)n;
  im = (mus_float_t *)calloc(n, sizeof(mus_float_t));

  mus_fft(data, im, n, 1);
  for (i = 0; i < n; i++)
    data[i] = data[i] * data[i] + im[i] * im[i];
  mus_clear_floats(im, n);

  mus_fft(data, im, n, -1);
  for (i = 0; i <= n2; i++) 
    data[i] *= fscl;
  for (i = n2 + 1; i < n; i++)
    data[i] = 0.0;

  free(im);
  return(data);
}


mus_float_t *mus_correlate(mus_float_t *data1, mus_float_t *data2, mus_long_t n)
{
  mus_float_t *im1, *im2;
  mus_long_t i;
  mus_float_t fscl;

  im1 = (mus_float_t *)calloc(n, sizeof(mus_float_t));
  im2 = (mus_float_t *)calloc(n, sizeof(mus_float_t));
  
  mus_fft(data1, im1, n, 1);
  mus_fft(data2, im2, n, 1);

  for (i = 0; i < n; i++)
    {
      mus_float_t tmp1, tmp2, tmp3, tmp4;
      tmp1 = data1[i] * data2[i];
      tmp2 = im1[i] * im2[i];
      tmp3 = data1[i] * im2[i];
      tmp4 = data2[i] * im1[i];
      data1[i] = tmp1 + tmp2;
      im1[i] = tmp3 - tmp4;
    }
  
  mus_fft(data1, im1, n, -1);
  fscl = 1.0 / (mus_float_t)n;
  for (i = 0; i < n; i++)
    data1[i] *= fscl;

  free(im1);
  free(im2);

  return(data1);
}


mus_float_t *mus_cepstrum(mus_float_t *data, mus_long_t n)
{
  mus_float_t *rl, *im;
  mus_float_t fscl, lowest;
  mus_long_t i;

  lowest = 0.00000001;
  fscl = 2.0 / (mus_float_t)n;

  rl = (mus_float_t *)malloc(n * sizeof(mus_float_t));
  im = (mus_float_t *)calloc(n, sizeof(mus_float_t));
  mus_copy_floats(rl, data, n);

  mus_fft(rl, im, n, 1);

  for (i = 0; i < n; i++)
    {
      rl[i] = rl[i] * rl[i] + im[i] * im[i];
      if (rl[i] < lowest)
	rl[i] = -10.0;
      else rl[i] = log(sqrt(rl[i]));
    }
  mus_clear_floats(im, n);

  mus_fft(rl, im, n, -1);

  for (i = 0; i < n; i++)
    if (fabs(rl[i]) > fscl) 
      fscl = fabs(rl[i]);

  if (fscl > 0.0)
    for (i = 0; i < n; i++) 
      data[i] = rl[i] / fscl;

  free(rl);
  free(im);
  return(data);
}



/* ---------------- convolve ---------------- */

mus_float_t *mus_convolution(mus_float_t *rl1, mus_float_t *rl2, mus_long_t n)
{
  /* convolves two real arrays.                                           
   * rl1 and rl2 are assumed to be set up correctly for the convolution   
   * (that is, rl1 (the "signal") is zero-padded by length of             
   * (non-zero part of) rl2 and rl2 is stored in wrap-around order)       
   * We treat rl2 as the imaginary part of the first fft, then do         
   * the split, scaling, and (complex) spectral multiply in one step.     
   * result in rl1                                                       
   */

  mus_long_t j, n2;
  mus_float_t invn;

  mus_fft(rl1, rl2, n, 1);
  
  n2 = n >> 1;
  invn = 0.25 / (mus_float_t)n;
  rl1[0] = ((rl1[0] * rl2[0]) / (mus_float_t)n);
  rl2[0] = 0.0;

  for (j = 1; j <= n2; j++)
    {
      mus_long_t nn2;
      mus_float_t rem, rep, aim, aip;

      nn2 = n - j;
      rep = (rl1[j] + rl1[nn2]);
      rem = (rl1[j] - rl1[nn2]);
      aip = (rl2[j] + rl2[nn2]);
      aim = (rl2[j] - rl2[nn2]);

      rl1[j] = invn * (rep * aip + aim * rem);
      rl2[j] = invn * (aim * aip - rep * rem);
      rl1[nn2] = rl1[j];
      rl2[nn2] = -rl2[j];
    }
  
  mus_fft(rl1, rl2, n, -1);
  return(rl1);
}


typedef struct {
  mus_any_class *core;
  mus_float_t (*feeder)(void *arg, int direction);
  mus_float_t (*block_feeder)(void *arg, int direction, mus_float_t *block, mus_long_t start, mus_long_t end);
  mus_long_t fftsize, fftsize2, ctr, filtersize;
  mus_float_t *rl1, *rl2, *buf, *filter; 
  void *closure;
} conv;


static bool convolve_equalp(mus_any *p1, mus_any *p2) {return(p1 == p2);}


static char *describe_convolve(mus_any *ptr)
{
  conv *gen = (conv *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s size: %" print_mus_long, 
	       mus_name(ptr),
	       gen->fftsize);
  return(describe_buffer);
}


static void free_convolve(mus_any *ptr)
{
  conv *gen = (conv *)ptr;
  if (gen->rl1) free(gen->rl1);
  if (gen->rl2) free(gen->rl2);
  if (gen->buf) free(gen->buf);
  free(gen);
}

static mus_any *conv_copy(mus_any *ptr)
{
  conv *g, *p;
  int bytes;

  p = (conv *)ptr;
  g = (conv *)malloc(sizeof(conv));
  memcpy((void *)g, (void *)ptr, sizeof(conv));
  bytes = g->fftsize * sizeof(mus_float_t);
  g->rl1 = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->rl1, p->rl1, g->fftsize);
  g->rl2 = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->rl2, p->rl2, g->fftsize);
  g->buf = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->buf, p->buf, g->fftsize);
  return((mus_any *)g);
}


static mus_long_t conv_length(mus_any *ptr) {return(((conv *)ptr)->fftsize);}
static mus_float_t run_convolve(mus_any *ptr, mus_float_t unused1, mus_float_t unused2) {return(mus_convolve(ptr, NULL));}

static void *conv_closure(mus_any *rd) {return(((conv *)rd)->closure);}
static void *conv_set_closure(mus_any *rd, void *e) {((conv *)rd)->closure = e; return(e);}

static void convolve_reset(mus_any *ptr)
{
  conv *gen = (conv *)ptr;
  gen->ctr = gen->fftsize2;
  mus_clear_floats(gen->rl1, gen->fftsize);
  mus_clear_floats(gen->rl2, gen->fftsize);
  mus_clear_floats(gen->buf, gen->fftsize);
}


static mus_any_class CONVOLVE_CLASS = {
  MUS_CONVOLVE,
  (char *)S_convolve,
  &free_convolve,
  &describe_convolve,
  &convolve_equalp,
  0, 0,
  &conv_length,
  0,
  0, 0, 0, 0,
  0, 0,
  0, 0,
  &run_convolve,
  MUS_NOT_SPECIAL,
  &conv_closure,
  0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,
  &convolve_reset,
  &conv_set_closure,
  &conv_copy
};


mus_float_t mus_convolve(mus_any *ptr, mus_float_t (*input)(void *arg, int direction))
{
  conv *gen = (conv *)ptr;
  mus_float_t result;
  if (gen->ctr >= gen->fftsize2)
    {
      mus_long_t N;
      N = gen->fftsize2;
      if (input) {gen->feeder = input; gen->block_feeder = NULL;}

      mus_clear_floats(gen->rl2, N * 2);
      mus_copy_floats(gen->rl2, gen->filter, gen->filtersize);
      mus_copy_floats(gen->buf, gen->buf + N, N);
      mus_clear_floats(gen->buf + N, N);
      mus_clear_floats(gen->rl1 + N, N);

      if (gen->block_feeder)
	gen->block_feeder(gen->closure, 1, gen->rl1, 0, N);
      else
	{
	  mus_long_t i;
	  for (i = 0; i < N;)
	    {
	      gen->rl1[i] = gen->feeder(gen->closure, 1); i++;
	      gen->rl1[i] = gen->feeder(gen->closure, 1); i++;
	    }
	}

      mus_convolution(gen->rl1, gen->rl2, gen->fftsize);
      mus_add_floats(gen->buf, gen->rl1, N);
      mus_copy_floats(gen->buf + N, gen->rl1 + N, N);
      gen->ctr = 0;
    }
  result = gen->buf[gen->ctr];
  gen->ctr++;
  return(result);
}


bool mus_is_convolve(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_CONVOLVE));
}


mus_any *mus_make_convolve(mus_float_t (*input)(void *arg, int direction), mus_float_t *filter, mus_long_t fftsize, mus_long_t filtersize, void *closure)
{
  conv *gen;
  gen = (conv *)malloc(sizeof(conv));
  gen->core = &CONVOLVE_CLASS;
  gen->feeder = input;
  gen->block_feeder = NULL;
  gen->closure = closure;
  gen->filter = filter;
  if (filter)
    {
      mus_long_t i;
      bool all_zero = true;
      for (i = 0; i < filtersize; i++)
	if (fabs(filter[i]) != 0.0) /* I'm getting -0.000 != 0.000 */
	  {
	    all_zero = false;
	    break;
	  }
      if (all_zero)
	mus_print("make_convolve: filter contains only 0.0.");
    }
  gen->filtersize = filtersize;
  gen->fftsize = fftsize;
  gen->fftsize2 = gen->fftsize / 2;
  gen->ctr = gen->fftsize2;
  gen->rl1 = (mus_float_t *)malloc(fftsize * sizeof(mus_float_t));
  gen->rl2 = (mus_float_t *)malloc(fftsize * sizeof(mus_float_t));
  gen->buf = (mus_float_t *)calloc(fftsize, sizeof(mus_float_t));
  return((mus_any *)gen);
}


void mus_convolve_files(const char *file1, const char *file2, mus_float_t maxamp, const char *output_file)
{
  mus_long_t file1_len, file2_len, outlen, totallen;
  int file1_chans, file2_chans, output_chans;
  mus_float_t *data1, *data2;
  const char *errmsg = NULL;
  mus_float_t maxval = 0.0;
  mus_long_t i, fftlen;

  file1_len = mus_sound_framples(file1);
  file2_len = mus_sound_framples(file2);
  if ((file1_len <= 0) || (file2_len <= 0)) return;

  file1_chans = mus_sound_chans(file1);
  if (file1_chans <= 0) mus_error(MUS_NO_CHANNELS, S_convolve_files ": %s chans: %d", file1, file1_chans);
  file2_chans = mus_sound_chans(file2);
  if (file2_chans <= 0) mus_error(MUS_NO_CHANNELS, S_convolve_files ": %s chans: %d", file2, file2_chans);
  output_chans = file1_chans; 
  if (file2_chans > output_chans) output_chans = file2_chans;

  fftlen = (mus_long_t)(pow(2.0, (int)ceil(log(file1_len + file2_len + 1) / log(2.0))));
  outlen = file1_len + file2_len + 1;
  totallen = outlen * output_chans;

  data1 = (mus_float_t *)calloc(fftlen, sizeof(mus_float_t));
  data2 = (mus_float_t *)calloc(fftlen, sizeof(mus_float_t));

  if (output_chans == 1)
    {
      mus_float_t *samps;
      samps = (mus_float_t *)calloc(fftlen, sizeof(mus_float_t));

      mus_file_to_array(file1, 0, 0, file1_len, samps); 
      for (i = 0; i < file1_len; i++) data1[i] = samps[i];
      mus_file_to_array(file2, 0, 0, file2_len, samps);
      for (i = 0; i < file2_len; i++) data2[i] = samps[i];

      mus_convolution(data1, data2, fftlen);

      for (i = 0; i < outlen; i++) 
	if (maxval < fabs(data1[i])) 
	  maxval = fabs(data1[i]);

      if (maxval > 0.0)
	{
	  maxval = maxamp / maxval;
	  for (i = 0; i < outlen; i++) data1[i] *= maxval;
	}

      for (i = 0; i < outlen; i++) samps[i] = data1[i];
      errmsg = mus_array_to_file_with_error(output_file, samps, outlen, mus_sound_srate(file1), 1);

      free(samps);
    }
  else
    {
      mus_float_t *samps;
      mus_float_t *outdat;
      int c1 = 0, c2 = 0, chan;

      samps = (mus_float_t *)calloc(totallen, sizeof(mus_float_t));
      outdat = (mus_float_t *)malloc(totallen * sizeof(mus_float_t));

      for (chan = 0; chan < output_chans; chan++)
	{
	  mus_long_t j, k;

	  mus_file_to_array(file1, c1, 0, file1_len, samps);
	  for (k = 0; k < file1_len; k++) data1[k] = samps[k];
	  mus_file_to_array(file2, c2, 0, file2_len, samps);
	  for (k = 0; k < file2_len; k++) data2[k] = samps[k];

	  mus_convolution(data1, data2, fftlen);

	  for (j = chan, k = 0; j < totallen; j += output_chans, k++) outdat[j] = data1[k];
	  c1++; 
	  if (c1 >= file1_chans) c1 = 0;
	  c2++; 
	  if (c2 >= file2_chans) c2 = 0;

	  mus_clear_floats(data1, fftlen);
	  mus_clear_floats(data2, fftlen);
	}

      for (i = 0; i < totallen; i++) 
	if (maxval < fabs(outdat[i])) 
	  maxval = fabs(outdat[i]);

      if (maxval > 0.0)
	{
	  maxval = maxamp / maxval;
	  for (i = 0; i < totallen; i++) outdat[i] *= maxval;
	}

      for (i = 0; i < totallen; i++) 
	samps[i] = outdat[i];

      errmsg = mus_array_to_file_with_error(output_file, samps, totallen, mus_sound_srate(file1), output_chans);
      free(samps);
      free(outdat);
    }

  free(data1);
  free(data2);

  if (errmsg)
    mus_error(MUS_CANT_OPEN_FILE, S_convolve_files ": %s", errmsg);
}



/* ---------------- phase-vocoder ---------------- */

typedef struct {
  mus_any_class *core;
  mus_float_t pitch;
  mus_float_t (*input)(void *arg, int direction);
  mus_float_t (*block_input)(void *arg, int direction, mus_float_t *block, mus_long_t start, mus_long_t end);
  void *closure;
  bool (*analyze)(void *arg, mus_float_t (*input)(void *arg1, int direction));
  int (*edit)(void *arg);
  mus_float_t (*synthesize)(void *arg);
  int outctr, interp, filptr, N, D, topN;
  mus_float_t *win, *ampinc, *amps, *freqs, *phases, *phaseinc, *lastphase, *in_data;

  mus_float_t sum1;
  bool calc;
#if HAVE_SINCOS
  double *cs, *sn;
  bool *sc_safe;
  int *indices;
#endif
} pv_info;


bool mus_is_phase_vocoder(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_PHASE_VOCODER));
}


static bool phase_vocoder_equalp(mus_any *p1, mus_any *p2) {return(p1 == p2);}


static char *describe_phase_vocoder(mus_any *ptr)
{
  char *arr = NULL;
  pv_info *gen = (pv_info *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s outctr: %d, interp: %d, filptr: %d, N: %d, D: %d, in_data: %s",
	       mus_name(ptr),
	       gen->outctr, gen->interp, gen->filptr, gen->N, gen->D,
	       arr = float_array_to_string(gen->in_data, gen->N, 0));
  if (arr) free(arr);
  return(describe_buffer);
}


static void free_phase_vocoder(mus_any *ptr)
{
  pv_info *gen = (pv_info *)ptr;
  if (gen->in_data) free(gen->in_data);
  if (gen->amps) free(gen->amps);
  if (gen->freqs) free(gen->freqs);
  if (gen->phases) free(gen->phases);
  if (gen->win) free(gen->win);
  if (gen->phaseinc) free(gen->phaseinc);
  if (gen->lastphase) free(gen->lastphase);
  if (gen->ampinc) free(gen->ampinc);
#if HAVE_SINCOS
  if (gen->indices) free(gen->indices);
  if (gen->sn) free(gen->sn);
  if (gen->cs) free(gen->cs);
  if (gen->sc_safe) free(gen->sc_safe);
#endif
  free(gen);
}


static mus_any *pv_info_copy(mus_any *ptr)
{
  pv_info *g, *p;
  int bytes;

  p = (pv_info *)ptr;
  g = (pv_info *)malloc(sizeof(pv_info));
  memcpy((void *)g, (void *)ptr, sizeof(pv_info));

  bytes = p->N * sizeof(mus_float_t);
  g->freqs = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->freqs, p->freqs, p->N);
  g->ampinc = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->ampinc, p->ampinc, p->N);
  g->win = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->win, p->win, p->N);
  if (p->in_data)
    {
      g->in_data = (mus_float_t *)malloc(bytes);
      mus_copy_floats(g->in_data, p->in_data, p->N);
    }

  bytes = (p->N / 2) * sizeof(mus_float_t);
  g->amps = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->amps, p->amps, p->N / 2);
  g->phases = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->phases, p->phases, p->N / 2);
  g->lastphase = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->lastphase, p->lastphase, p->N / 2);
  g->phaseinc = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->phaseinc, p->phaseinc, p->N / 2);

#if HAVE_SINCOS
  bytes = (p->N / 2) * sizeof(int);
  g->indices = (int *)malloc(bytes);
  memcpy((void *)(g->indices), (void *)(p->indices), bytes);

  bytes = p->N * sizeof(double);
  g->sn = (double *)malloc(bytes);
  memcpy((void *)(g->sn), (void *)(p->sn), bytes);
  g->cs = (double *)malloc(bytes);
  memcpy((void *)(g->cs), (void *)(p->cs), bytes);

  bytes = p->N * sizeof(bool);
  g->sc_safe = (bool *)malloc(bytes);
  memcpy((void *)(g->sc_safe), (void *)(p->sc_safe), bytes);
#endif

  return((mus_any *)g);
}


static mus_long_t pv_length(mus_any *ptr) {return(((pv_info *)ptr)->N);}

static mus_long_t pv_hop(mus_any *ptr) {return(((pv_info *)ptr)->D);}
static mus_long_t pv_set_hop(mus_any *ptr, mus_long_t val) {((pv_info *)ptr)->D = (int)val; return(val);}

static mus_float_t pv_frequency(mus_any *ptr) {return(((pv_info *)ptr)->pitch);}
static mus_float_t pv_set_frequency(mus_any *ptr, mus_float_t val) {((pv_info *)ptr)->pitch = val; return(val);}

static void *pv_closure(mus_any *rd) {return(((pv_info *)rd)->closure);}
static void *pv_set_closure(mus_any *rd, void *e) {((pv_info *)rd)->closure = e; return(e);}

mus_float_t *mus_phase_vocoder_amp_increments(mus_any *ptr) {return(((pv_info *)ptr)->ampinc);}
mus_float_t *mus_phase_vocoder_amps(mus_any *ptr) {return(((pv_info *)ptr)->amps);}

mus_float_t *mus_phase_vocoder_freqs(mus_any *ptr) {return(((pv_info *)ptr)->freqs);}
mus_float_t *mus_phase_vocoder_phases(mus_any *ptr) {return(((pv_info *)ptr)->phases);}
mus_float_t *mus_phase_vocoder_phase_increments(mus_any *ptr) {return(((pv_info *)ptr)->phaseinc);}

static mus_long_t pv_outctr(mus_any *ptr) {return((mus_long_t)(((pv_info *)ptr)->outctr));} /* mus_location wrapper */
static mus_long_t pv_set_outctr(mus_any *ptr, mus_long_t val) {((pv_info *)ptr)->outctr = (int)val; return(val);}

static mus_float_t run_phase_vocoder(mus_any *ptr, mus_float_t unused1, mus_float_t unused2) {return(mus_phase_vocoder(ptr, NULL));}

static mus_float_t pv_increment(mus_any *rd) {return((mus_float_t)(((pv_info *)rd)->interp));}
static mus_float_t pv_set_increment(mus_any *rd, mus_float_t val) {((pv_info *)rd)->interp = (int)val; return(val);}


static void pv_reset(mus_any *ptr)
{
  pv_info *gen = (pv_info *)ptr;
  if (gen->in_data) free(gen->in_data);
  gen->in_data = NULL;
  gen->outctr = gen->interp;
  gen->filptr = 0;
  mus_clear_floats(gen->ampinc, gen->N);
  mus_clear_floats(gen->freqs, gen->N);
  mus_clear_floats(gen->amps, gen->N / 2);
  mus_clear_floats(gen->phases, gen->N / 2);
  mus_clear_floats(gen->lastphase, gen->N / 2);
  mus_clear_floats(gen->phaseinc, gen->N / 2);
}


static mus_any_class PHASE_VOCODER_CLASS = {
  MUS_PHASE_VOCODER,
  (char *)S_phase_vocoder,
  &free_phase_vocoder,
  &describe_phase_vocoder,
  &phase_vocoder_equalp,
  0, 0,
  &pv_length, 0,
  &pv_frequency,
  &pv_set_frequency,
  0, 0,
  0, 0,
  &pv_increment,
  &pv_set_increment,
  &run_phase_vocoder,
  MUS_NOT_SPECIAL,
  &pv_closure,
  0,
  0, 0, 0, 0, 0, 0, 
  &pv_hop, &pv_set_hop, 
  0, 0,
  0, 0, 0, 0, 
  &pv_outctr, &pv_set_outctr,
  0, 0, 0, 0, 0,
  &pv_reset,
  &pv_set_closure,
  &pv_info_copy
};


static int pv_last_fftsize = -1;
static mus_float_t *pv_last_window = NULL;

mus_any *mus_make_phase_vocoder(mus_float_t (*input)(void *arg, int direction), 
				int fftsize, int overlap, int interp,
				mus_float_t pitch,
				bool (*analyze)(void *arg, mus_float_t (*input)(void *arg1, int direction)),
				int (*edit)(void *arg), 
				mus_float_t (*synthesize)(void *arg), 
				void *closure)
{
  /* order of args is trying to match src, granulate etc
   *   the inclusion of pitch and interp provides built-in time/pitch scaling which is 99% of phase-vocoder use
   */
  pv_info *pv;
  int N2, D;

  N2 = (int)(fftsize / 2);
  if (N2 == 0) return(NULL);
  D = fftsize / overlap;
  if (D == 0) return(NULL);

  pv = (pv_info *)malloc(sizeof(pv_info));
  pv->core = &PHASE_VOCODER_CLASS;
  pv->N = fftsize;
  pv->D = D;
  pv->topN = 0;
  pv->interp = interp;
  pv->outctr = interp;
  pv->filptr = 0;
  pv->pitch = pitch;
  pv->ampinc = (mus_float_t *)calloc(fftsize, sizeof(mus_float_t));
  pv->freqs = (mus_float_t *)calloc(fftsize, sizeof(mus_float_t));
  pv->amps = (mus_float_t *)calloc(N2, sizeof(mus_float_t));
  pv->phases = (mus_float_t *)calloc(N2, sizeof(mus_float_t));
  pv->lastphase = (mus_float_t *)calloc(N2, sizeof(mus_float_t));
  pv->phaseinc = (mus_float_t *)calloc(N2, sizeof(mus_float_t));
  pv->in_data = NULL;
  pv->input = input;
  pv->block_input = NULL;
  pv->closure = closure;
  pv->analyze = analyze;
  pv->edit = edit;
  pv->synthesize = synthesize;
  pv->calc = true;

  if ((fftsize == pv_last_fftsize) && (pv_last_window))
    {
      pv->win = (mus_float_t *)malloc(fftsize * sizeof(mus_float_t));
      mus_copy_floats(pv->win, pv_last_window, fftsize);
    }
  else
    {
      int i;
      mus_float_t scl;
      if (pv_last_window) free(pv_last_window);
      pv_last_fftsize = fftsize;
      pv_last_window = (mus_float_t *)malloc(fftsize * sizeof(mus_float_t));
      pv->win = mus_make_fft_window(MUS_HAMMING_WINDOW, fftsize, 0.0);
      scl = 2.0 / (0.54 * (mus_float_t)fftsize);
      for (i = 0; i < fftsize; i++) 
	pv->win[i] *= scl;
      mus_copy_floats(pv_last_window, pv->win, fftsize);
    }

#if HAVE_SINCOS
  /* in some cases, sincos is slower than sin+cos? Callgrind is seriously confused by it!
   *   in Linux at least, sincos is faster than sin+sin -- in my timing tests, although
   *   callgrind is crazy, the actual runtimes are about 25% faster (sincos vs sin+sin).
   */
  pv->cs = (double *)calloc(fftsize, sizeof(double));
  pv->sn = (double *)calloc(fftsize, sizeof(double));
  pv->sc_safe = (bool *)calloc(fftsize, sizeof(bool));
  pv->indices = (int *)calloc(N2, sizeof(int));
#endif
  return((mus_any *)pv);
}


mus_float_t mus_phase_vocoder_with_editors(mus_any *ptr, 
					   mus_float_t (*input)(void *arg, int direction),
					   bool (*analyze)(void *arg, mus_float_t (*input)(void *arg1, int direction)),
					   int (*edit)(void *arg), 
					   mus_float_t (*synthesize)(void *arg))
 {
  pv_info *pv = (pv_info *)ptr;
  int N2, i;
  mus_float_t (*pv_synthesize)(void *arg) = synthesize;

  if (!pv_synthesize) pv_synthesize = pv->synthesize;
  N2 = pv->N / 2;

  if (pv->outctr >= pv->interp)
    {
      mus_float_t scl;
      bool (*pv_analyze)(void *arg, mus_float_t (*input)(void *arg1, int direction)) = analyze;
      int (*pv_edit)(void *arg) = edit;

      if (!pv_analyze) pv_analyze = pv->analyze;
      if (!pv_edit) pv_edit = pv->edit;
      if (input) {pv->input = input; pv->block_input = NULL;}
      pv->outctr = 0;

      if ((!pv_analyze) || 
	  ((*pv_analyze)(pv->closure, pv->input)))
	{
	  int buf;
	  mus_clear_floats(pv->freqs, pv->N);
	  if (!pv->in_data)
	    {
	      pv->in_data = (mus_float_t *)malloc(pv->N * sizeof(mus_float_t));
	      if (pv->block_input)
		pv->block_input(pv->closure, 1, pv->in_data, 0, pv->N);
	      else
		{
		  for (i = 0; i < pv->N; i++) 
		    pv->in_data[i] = pv->input(pv->closure, 1);
		}
	    }
	  else
	    {
	      int j;
	      /* if back-to-back here we could omit a lot of data movement or just use a circle here! */
	      for (i = 0, j = pv->D; j < pv->N; i++, j++)
		pv->in_data[i] = pv->in_data[j];
	      
	      if (pv->block_input)
		pv->block_input(pv->closure, 1, pv->in_data, pv->N - pv->D, pv->N);
	      else
		{
		  for (i = pv->N - pv->D; i < pv->N; i++) 
		    pv->in_data[i] = pv->input(pv->closure, 1);
		}
	    }
	  buf = pv->filptr % pv->N; /* filptr and N are both ints */
	  for (i = 0; i < pv->N; i++)
	    {
	      pv->ampinc[buf++] = pv->win[i] * pv->in_data[i];
	      if (buf >= pv->N) buf = 0;
	    }
	  pv->filptr += pv->D;
	  mus_fft(pv->ampinc, pv->freqs, pv->N, 1);
	  mus_rectangular_to_polar(pv->ampinc, pv->freqs, N2);
	}
      
      if ((!pv_edit) || 
	  ((*pv_edit)(pv->closure)))
	{
	  mus_float_t pscl, kscl, ks;
	  pscl = 1.0 / (mus_float_t)(pv->D);
	  kscl = TWO_PI / (mus_float_t)(pv->N);
	  for (i = 0, ks = 0.0; i < N2; i++, ks += kscl)
	    {
	      mus_float_t diff;
	      diff = pv->freqs[i] - pv->lastphase[i];
	      pv->lastphase[i] = pv->freqs[i];
	      
	      /* this used to be two while loops adding/subtracting two pi, but that can get into an infinite loop
	       *   while (diff > M_PI) diff -= TWO_PI;
	       *   while (diff < -M_PI) diff += TWO_PI;
	       * (anything to avoid fmod!)
	       */
	      if (diff > M_PI)
		{
		  diff -= TWO_PI;
		  if (diff > M_PI)
		    diff = fmod(diff, TWO_PI);
		}
	      if (diff < -M_PI)
		{
		  diff += TWO_PI;
		  if (diff < -M_PI)
		    {
		      diff = fmod(diff, TWO_PI);
		      if (diff < -M_PI) 
			diff += TWO_PI;
		    }
		}
	      pv->freqs[i] = pv->pitch * (diff * pscl + ks);
	    }
	}
      /* it's possible to build the endpoint waveforms here and interpolate, but there is no savings.
       *   other pvocs use ifft rather than sin-bank, but then they have to make excuses.
       *   Something I didn't expect -- the algorithm above focusses on the active frequency!  
       *   For example, the 4 or so bins around a given peak all tighten
       *   to 4 bins running at almost exactly the same frequency (the center).
       */

      scl = 1.0 / (mus_float_t)(pv->interp);
#if HAVE_SINCOS
      pv->topN = 0;
#else
      pv->topN = N2;
#endif

      for (i = 0; i < N2; i++)
	{
#if HAVE_SINCOS
	  double s, c;
	  bool amp_zero;
	  
	  amp_zero = ((pv->amps[i] < 1e-7) && (pv->ampinc[i] == 0.0));
	  if (!amp_zero)
	    {
	      pv->indices[pv->topN++] = i;

	      pv->sc_safe[i] = (fabs(pv->freqs[i] - pv->phaseinc[i]) < 0.02); /* .5 is too big, .01 and .03 ok by tests */
	      if (pv->sc_safe[i])
		{
		  sincos((pv->freqs[i] + pv->phaseinc[i]) * 0.5, &s, &c);
		  pv->sn[i] = s;
		  pv->cs[i] = c;
		}
	    }

	  if ((!(pv->synthesize)) && (amp_zero))
	    {
	      pv->phases[i] += (pv->interp * (pv->freqs[i] + pv->phaseinc[i]) * 0.5);
	      pv->phaseinc[i] = pv->freqs[i];
	    }
	  else
	    {
	      pv->ampinc[i] = scl * (pv->ampinc[i] - pv->amps[i]);
	      pv->freqs[i] = scl * (pv->freqs[i] - pv->phaseinc[i]);
	    }
#else
	  pv->ampinc[i] = scl * (pv->ampinc[i] - pv->amps[i]);
	  pv->freqs[i] = scl * (pv->freqs[i] - pv->phaseinc[i]);
#endif
	}
    }
  
  pv->outctr++;
  if (pv_synthesize) 
    return((*pv_synthesize)(pv->closure));

  if (pv->calc)
    {
      mus_float_t sum, sum1;
      mus_float_t *pinc, *frq, *ph, *amp, *panc;
      int topN;
#if HAVE_SINCOS
      int j;
      double *cs, *sn;
#endif

      topN = pv->topN;
      pinc = pv->phaseinc;
      frq = pv->freqs;
      ph = pv->phases;
      amp = pv->amps;
      panc = pv->ampinc;
#if HAVE_SINCOS
      cs = pv->cs;
      sn = pv->sn;
#endif

      sum = 0.0;
      sum1 = 0.0;

      /* amps can be negative here due to rounding troubles
       * sincos is faster (using shell time command) except in virtualbox running linux on a mac? 
       *   (callgrind does not handle sincos correctly).
       *
       * this version (22-Jan-14) is slower if no sincos;
       *   if sincos, we use sin(a + b) = sin(a)cos(b) + cos(a)sin(b)
       *   since sin(b) and cos(b) are constant through the pv->interp (implicit) loop, they are calculated once above.
       *   Then here we calculate 2 samples on each run through this loop.  I wonder if we could center the true case,
       *   and get 3 samples?  If 2, the difference is very small (we're taking the midpoint of the phase increment change,
       *   so the two are not quite the same).  In tests, 10000 samples, channel-distance is ca .15.
       *
       * If the amp zero phase is off (incorrectly incremented above), the effect is a sort of low-pass filter??
       *   Are we getting cancellation from the overlap?
       */
#if HAVE_SINCOS
      for (j = 0; j < topN; j++)
	{
	  double sx, cx;

	  i = pv->indices[j];
	  pinc[i] += frq[i];
	  ph[i] += pinc[i];
	  amp[i] += panc[i];
	  sincos(ph[i], &sx, &cx);
	  sum += (amp[i] * sx);

	  pinc[i] += frq[i];
	  ph[i] += pinc[i];
	  amp[i] += panc[i];
	  if (pv->sc_safe[i]) 
	    sum1 += amp[i] * (sx * cs[i] + cx * sn[i]);
	  else sum1 += amp[i] * sin(ph[i]);
	}
#else
      for (i = 0; i < topN; i++)
	{
	  pinc[i] += frq[i];
	  ph[i] += pinc[i];
	  amp[i] += panc[i];
	  if (amp[i] > 0.0) sum += amp[i] * sin(ph[i]);

	  pinc[i] += frq[i];
	  ph[i] += pinc[i];
	  amp[i] += panc[i];
	  if (amp[i] > 0.0) sum1 += amp[i] * sin(ph[i]);
	}
#endif
      pv->sum1 = sum1;
      pv->calc = false;
      return(sum);
    }
  pv->calc = true;
  return(pv->sum1);
}
    

mus_float_t mus_phase_vocoder(mus_any *ptr, mus_float_t (*input)(void *arg, int direction))
{
  return(mus_phase_vocoder_with_editors(ptr, input, NULL, NULL, NULL));
}


void mus_generator_set_feeders(mus_any *g, 
			       mus_float_t (*feed)(void *arg, int direction),
			       mus_float_t (*block_feed)(void *arg, int direction, mus_float_t *block, mus_long_t start, mus_long_t end))
{
  if (mus_is_src(g))
    {
      ((sr *)g)->feeder = feed;
      ((sr *)g)->block_feeder = block_feed;
    }
  else
    {
      if (mus_is_granulate(g))
	{
	  ((grn_info *)g)->rd = feed;
	  ((grn_info *)g)->block_rd = block_feed;
	}
      else
	{
	  if (mus_is_phase_vocoder(g))
	    {
	      ((pv_info *)g)->input = feed;
	      ((pv_info *)g)->block_input = block_feed;
	    }
	  else
	    {
	      if (mus_is_convolve(g))
		{
		  ((conv *)g)->feeder = feed;
		  ((conv *)g)->block_feeder = block_feed;
		}
	    }
	}
    }
}

void mus_generator_copy_feeders(mus_any *dest, mus_any *source)
{
  if (mus_is_src(dest))
    {
      ((sr *)dest)->feeder = ((sr *)source)->feeder;
      ((sr *)dest)->block_feeder = ((sr *)source)->block_feeder;
    }
  else
    {
      if (mus_is_granulate(dest))
	{
	  ((grn_info *)dest)->rd = ((grn_info *)source)->rd;
	  ((grn_info *)dest)->block_rd = ((grn_info *)source)->block_rd;
	}
      else
	{
	  if (mus_is_phase_vocoder(dest))
	    {
	      ((pv_info *)dest)->input = ((pv_info *)source)->input;
	      ((pv_info *)dest)->block_input = ((pv_info *)source)->block_input;
	    }
	  else
	    {
	      if (mus_is_convolve(dest))
		{
		  ((conv *)dest)->feeder = ((conv *)source)->feeder;
		  ((conv *)dest)->block_feeder = ((conv *)source)->block_feeder;
		}
	    }
	}
    }
}



/* ---------------- single sideband "suppressed carrier" amplitude modulation (ssb-am) ---------------- */

typedef struct {
  mus_any_class *core;
  bool shift_up;
  mus_float_t *coeffs;
  mus_any *hilbert, *dly;
#if (!HAVE_SINCOS)
  mus_any *sin_osc, *cos_osc;
#else
  double phase, freq, sign;
#endif
  int size;
} ssbam;


bool mus_is_ssb_am(mus_any *ptr) 
{
  return((ptr) && 
	 (ptr->core->type == MUS_SSB_AM));
}

static mus_float_t run_hilbert(flt *gen, mus_float_t input)
{
  mus_float_t xout = 0.0;
  mus_float_t *state, *ts, *x, *end;

  x = (mus_float_t *)(gen->x);
  state = (mus_float_t *)(gen->state + gen->loc);
  ts = (mus_float_t *)(state + gen->order);

  (*state) = input;
  (*ts) = input;
  state += 2;
  end = (mus_float_t *)(state + 20);

  while (ts > end)
    {
      xout += (*ts) * (*x); ts -= 2; x += 2;
      xout += (*ts) * (*x); ts -= 2; x += 2;
      xout += (*ts) * (*x); ts -= 2; x += 2;
      xout += (*ts) * (*x); ts -= 2; x += 2;
      xout += (*ts) * (*x); ts -= 2; x += 2;
      xout += (*ts) * (*x); ts -= 2; x += 2;
      xout += (*ts) * (*x); ts -= 2; x += 2;
      xout += (*ts) * (*x); ts -= 2; x += 2;
      xout += (*ts) * (*x); ts -= 2; x += 2;
      xout += (*ts) * (*x); ts -= 2; x += 2;
    }
  while (ts > state)
    {
      xout += (*ts) * (*x); ts -= 2; x += 2;
    }

  gen->loc++;
  if (gen->loc == gen->order)
    gen->loc = 0;

  return(xout + ((*ts) * (*x)));
#if 0
  int i, len;
  mus_float_t val = 0.0;
  len = g->order;
  g->state[0] = insig;
  for (i = 0; i < len; i += 2) val += (g->x[i] * g->state[i]);
  for (i = len - 1; i >= 1; i--) g->state[i] = g->state[i - 1];
  return(val);
#endif
}


mus_float_t mus_ssb_am_unmodulated(mus_any *ptr, mus_float_t insig)
{
  ssbam *gen = (ssbam *)ptr;
#if (!HAVE_SINCOS)
  return((mus_oscil_unmodulated(gen->cos_osc) * mus_delay_unmodulated_noz(gen->dly, insig)) +
	 (mus_oscil_unmodulated(gen->sin_osc) * run_hilbert((flt *)(gen->hilbert), insig)));
#else
  double cx, sx;
  sincos(gen->phase, &sx, &cx);
  gen->phase += gen->freq;
  return((cx * mus_delay_unmodulated_noz(gen->dly, insig)) +
         (sx * gen->sign * run_hilbert((flt *)(gen->hilbert), insig)));
#endif
}


mus_float_t mus_ssb_am(mus_any *ptr, mus_float_t insig, mus_float_t fm)
{
  ssbam *gen = (ssbam *)ptr;
#if (!HAVE_SINCOS)
  return((mus_oscil_fm(gen->cos_osc, fm) * mus_delay_unmodulated_noz(gen->dly, insig)) +
	 (mus_oscil_fm(gen->sin_osc, fm) * run_hilbert((flt *)(gen->hilbert), insig)));
#else
  double cx, sx;
  sincos(gen->phase, &sx, &cx);
  gen->phase += (fm + gen->freq);
  return((cx * mus_delay_unmodulated_noz(gen->dly, insig)) +
         (sx * gen->sign * run_hilbert((flt *)(gen->hilbert), insig)));
#endif
}


static void free_ssb_am(mus_any *ptr) 
{
  ssbam *gen = (ssbam *)ptr;
  mus_free(gen->dly);
  mus_free(gen->hilbert);
#if (!HAVE_SINCOS)
  mus_free(gen->cos_osc);
  mus_free(gen->sin_osc);
#endif
  if (gen->coeffs) {free(gen->coeffs); gen->coeffs = NULL;}
  free(ptr); 
}


static mus_any *ssbam_copy(mus_any *ptr)
{
  ssbam *g, *p;
  int bytes;

  p = (ssbam *)ptr;
  g = (ssbam *)malloc(sizeof(ssbam));
  memcpy((void *)g, (void *)ptr, sizeof(ssbam));

  g->dly = mus_copy(p->dly);
  g->hilbert = mus_copy(p->hilbert);
#if (!HAVE_SINCOS)
  g->cos_osc = mus_copy(p->cos_osc);
  g->sin_osc = mus_copy(p->sin_osc);
#endif

  bytes = p->size * sizeof(mus_float_t);
  g->coeffs = (mus_float_t *)malloc(bytes);
  mus_copy_floats(g->coeffs, p->coeffs, p->size);

  return((mus_any *)g);
}


static mus_float_t ssb_am_freq(mus_any *ptr) 
{
#if (!HAVE_SINCOS)
  return(mus_radians_to_hz(((osc *)((ssbam *)ptr)->sin_osc)->freq));
#else
  return(mus_radians_to_hz(((ssbam *)ptr)->freq));
#endif
}


static mus_float_t ssb_am_set_freq(mus_any *ptr, mus_float_t val) 
{
  ssbam *gen = (ssbam *)ptr;
  mus_float_t rads;
  rads = mus_hz_to_radians(val);
#if (!HAVE_SINCOS)
  ((osc *)(gen->sin_osc))->freq = rads;
  ((osc *)(gen->cos_osc))->freq = rads;
#else
  gen->freq = rads;
#endif
  return(val);
}


static mus_float_t ssb_am_increment(mus_any *ptr) 
{
#if (!HAVE_SINCOS)
  return(((osc *)((ssbam *)ptr)->sin_osc)->freq);
#else
  return(((ssbam *)ptr)->freq);
#endif
}


static mus_float_t ssb_am_set_increment(mus_any *ptr, mus_float_t val) 
{
  ssbam *gen = (ssbam *)ptr;
#if (!HAVE_SINCOS)
  ((osc *)(gen->sin_osc))->freq = val;
  ((osc *)(gen->cos_osc))->freq = val;
#else
  gen->freq = val;
#endif
  return(val);
}


static mus_float_t ssb_am_phase(mus_any *ptr) 
{
#if (!HAVE_SINCOS)
  return(fmod(((osc *)((ssbam *)ptr)->cos_osc)->phase - 0.5 * M_PI, TWO_PI));
#else
  return(fmod(((ssbam *)ptr)->phase, TWO_PI));
#endif
}


static mus_float_t ssb_am_set_phase(mus_any *ptr, mus_float_t val) 
{
  ssbam *gen = (ssbam *)ptr;
#if (!HAVE_SINCOS)
  if (gen->shift_up)
    ((osc *)(gen->sin_osc))->phase = val + M_PI;
  else ((osc *)(gen->sin_osc))->phase = val; 
  ((osc *)(gen->cos_osc))->phase = val + 0.5 * M_PI;
#else
  gen->phase = val;
#endif
  return(val);
}


static mus_long_t ssb_am_order(mus_any *ptr) {return(mus_order(((ssbam *)ptr)->dly));}

static int ssb_am_interp_type(mus_any *ptr) {return(delay_interp_type(((ssbam *)ptr)->dly));}

static mus_float_t *ssb_am_data(mus_any *ptr) {return(filter_data(((ssbam *)ptr)->hilbert));}
static mus_float_t ssb_am_run(mus_any *ptr, mus_float_t insig, mus_float_t fm) {return(mus_ssb_am(ptr, insig, fm));}

static mus_float_t *ssb_am_xcoeffs(mus_any *ptr) {return(mus_xcoeffs(((ssbam *)ptr)->hilbert));}
static mus_float_t ssb_am_xcoeff(mus_any *ptr, int index) {return(mus_xcoeff(((ssbam *)ptr)->hilbert, index));}
static mus_float_t ssb_am_set_xcoeff(mus_any *ptr, int index, mus_float_t val) {return(mus_set_xcoeff(((ssbam *)ptr)->hilbert, index, val));}


static bool ssb_am_equalp(mus_any *p1, mus_any *p2)
{
  return((p1 == p2) ||
	 ((mus_is_ssb_am((mus_any *)p1)) && 
	  (mus_is_ssb_am((mus_any *)p2)) &&
	  (((ssbam *)p1)->shift_up == ((ssbam *)p2)->shift_up) &&
#if (!HAVE_SINCOS)
	  (mus_equalp(((ssbam *)p1)->sin_osc, ((ssbam *)p2)->sin_osc)) &&
	  (mus_equalp(((ssbam *)p1)->cos_osc, ((ssbam *)p2)->cos_osc)) &&
#else
	  (((ssbam *)p1)->freq == ((ssbam *)p2)->freq) &&
	  (((ssbam *)p1)->phase == ((ssbam *)p2)->phase) &&
#endif
	  (mus_equalp(((ssbam *)p1)->dly, ((ssbam *)p2)->dly)) &&
	  (mus_equalp(((ssbam *)p1)->hilbert, ((ssbam *)p2)->hilbert))));
}


static char *describe_ssb_am(mus_any *ptr)
{
  ssbam *gen = (ssbam *)ptr;
  char *describe_buffer;
  describe_buffer = (char *)malloc(DESCRIBE_BUFFER_SIZE);
  snprintf(describe_buffer, DESCRIBE_BUFFER_SIZE, "%s shift: %s, sin/cos: %f Hz (%f radians), order: %d",
	       mus_name(ptr),
	       (gen->shift_up) ? "up" : "down",
	       mus_frequency(ptr),
	       mus_phase(ptr),
	       (int)mus_order(ptr));
  return(describe_buffer);
}


static void ssb_reset(mus_any *ptr)
{
  ssbam *gen = (ssbam *)ptr;
  ssb_am_set_phase(ptr, 0.0);
  mus_reset(gen->dly);
  mus_reset(gen->hilbert);
}


static mus_any_class SSB_AM_CLASS = {
  MUS_SSB_AM,
  (char *)S_ssb_am,
  &free_ssb_am,
  &describe_ssb_am,
  &ssb_am_equalp,
  &ssb_am_data, 0,
  &ssb_am_order, 0,
  &ssb_am_freq,
  &ssb_am_set_freq,
  &ssb_am_phase,
  &ssb_am_set_phase,
  &fallback_scaler, 0, 
  &ssb_am_increment,
  &ssb_am_set_increment,
  &ssb_am_run,
  MUS_NOT_SPECIAL, 
  NULL,
  &ssb_am_interp_type,
  0, 0, 0, 0,
  &ssb_am_xcoeff, &ssb_am_set_xcoeff, 
  0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0,
  0, 0, 
  &ssb_am_xcoeffs, 0,
  &ssb_reset,
  0, &ssbam_copy
};


static int ssb_am_last_flen = -1;
static mus_float_t *ssb_am_last_coeffs = NULL;

mus_any *mus_make_ssb_am(mus_float_t freq, int order)
{
  ssbam *gen;
  int len, flen;

  if ((order & 1) == 0) order++; /* if order is even, the first Hilbert coeff is 0.0 */
  gen = (ssbam *)malloc(sizeof(ssbam));
  gen->core = &SSB_AM_CLASS;

  if (freq > 0)
    gen->shift_up = true;
  else gen->shift_up = false;
#if (!HAVE_SINCOS)
  gen->sin_osc = mus_make_oscil(fabs(freq), (gen->shift_up) ? M_PI : 0.0);
  gen->cos_osc = mus_make_oscil(fabs(freq), M_PI * 0.5);
#else
  if (gen->shift_up) gen->sign = -1.0; else gen->sign = 1.0;
  gen->freq = mus_hz_to_radians(fabs(freq));
  gen->phase = 0.0;
#endif
  gen->dly = mus_make_delay(order, NULL, order, MUS_INTERP_NONE);

  len = order * 2 + 1;
  flen = len + 1; /* even -- need 4 */
  if ((flen & 2) != 0) flen += 2;
  gen->size = flen;

  if ((flen == ssb_am_last_flen) && (ssb_am_last_coeffs))
    {
      gen->coeffs = (mus_float_t *)malloc(flen * sizeof(mus_float_t));
      mus_copy_floats(gen->coeffs, ssb_am_last_coeffs, flen);
    }
  else
    {
      int i, k;
      gen->coeffs = (mus_float_t *)calloc(flen, sizeof(mus_float_t));
      for (i = -order, k = 0; i <= order; i++, k++)
	{
	  mus_float_t denom, num;
	  denom = i * M_PI;
	  num = 1.0 - cos(denom);
	  if (i == 0)
	    gen->coeffs[k] = 0.0;
	  else gen->coeffs[k] = (num / denom) * (0.54 + (0.46 * cos(denom / order)));
	}
      /* so odd numbered coeffs are zero */
      
      /* can't be too fancy here because there might be several of these gens running in parallel at different sizes */
      if (ssb_am_last_coeffs) free(ssb_am_last_coeffs);
      ssb_am_last_flen = flen;
      ssb_am_last_coeffs = (mus_float_t *)malloc(flen * sizeof(mus_float_t));
      mus_copy_floats(ssb_am_last_coeffs, gen->coeffs, flen);
    }

  gen->hilbert = mus_make_fir_filter(flen, gen->coeffs, NULL);
  return((mus_any *)gen);
}

/* (define (hi) (let ((g (make-ssb-am 100.0))) (ssb-am g 1.0) (ssb-am g 0.0))) */





/* ---------------- mus-apply ---------------- */

mus_float_t (*mus_run_function(mus_any *g))(mus_any *gen, mus_float_t arg1, mus_float_t arg2)
{
  if (g)
    return(g->core->run);
  return(NULL);
}

mus_float_t (*mus_run1_function(mus_any *g))(mus_any *gen, mus_float_t arg)
{
  if (g)
    {
      switch (g->core->type)
	{
	case MUS_FILTER:
	case MUS_FIR_FILTER:
	case MUS_IIR_FILTER:    return(((flt *)g)->filtw);

	case MUS_FORMANT:       return(mus_formant);
	case MUS_FIRMANT:       return(mus_firmant);
	  
	case MUS_ONE_POLE:      return(mus_one_pole);
	case MUS_ONE_ZERO:      return(mus_one_zero);
	case MUS_TWO_POLE:      return(mus_two_pole);
	case MUS_TWO_ZERO:      return(mus_two_zero);
	case MUS_ONE_POLE_ALL_PASS: return(((onepall *)g)->f);

	case MUS_DELAY:         return(mus_delay_unmodulated);
	case MUS_COMB:          return(mus_comb_unmodulated);
	case MUS_NOTCH:         return(mus_notch_unmodulated);
	case MUS_ALL_PASS:      return(mus_all_pass_unmodulated);

	case MUS_TRIANGLE_WAVE: return(mus_triangle_wave);
	case MUS_SAWTOOTH_WAVE: return(mus_sawtooth_wave);
	case MUS_SQUARE_WAVE:   return(mus_square_wave);
	case MUS_PULSE_TRAIN:   return(mus_pulse_train);
	case MUS_PULSED_ENV:    return(mus_pulsed_env);
	  
	case MUS_OSCIL:         return(mus_oscil_fm);
	case MUS_NCOS:          return(mus_ncos);
	case MUS_NSIN:          return(mus_nsin);
	case MUS_NRXYCOS:       return(mus_ncos);
	case MUS_NRXYSIN:       return(mus_nsin);
	case MUS_RXYKCOS:       return(mus_ncos);
	case MUS_RXYKSIN:       return(mus_nsin);

	case MUS_TABLE_LOOKUP:  return(((tbl *)g)->tbl_look);
	case MUS_POLYWAVE:      return(((pw *)g)->polyw);
	  
	case MUS_WAVE_TRAIN:    return(mus_wave_train);
	case MUS_COMB_BANK:     return(((cmb_bank *)g)->cmbf);
	case MUS_ALL_PASS_BANK: return(((allp_bank *)g)->apf);
	case MUS_FILTERED_COMB_BANK: return(((fltcmb_bank *)g)->cmbf);
	case MUS_FORMANT_BANK:  return(((frm_bank *)g)->one_input);

	case MUS_MOVING_AVERAGE: return(mus_moving_average);
	case MUS_MOVING_MAX:    return(mus_moving_max);
	case MUS_MOVING_NORM:   return(mus_moving_norm);

	case MUS_RAND:          return(mus_rand);
	case MUS_RAND_INTERP:   return(mus_rand_interp);

	case MUS_SSB_AM:        return(mus_ssb_am_unmodulated);
	}
    }
  return(NULL);
}

mus_float_t mus_apply(mus_any *gen, mus_float_t f1, mus_float_t f2)
{
  /* what about non-gen funcs such as polynomial, ring_modulate etc? */
  if ((gen) && (gen->core->run))
    return((*(gen->core->run))(gen, f1, f2));
  return(0.0);
}


/* ---------------- mix files ---------------- */

/* a mixing "instrument" along the lines of the mix function in clm */
/* this is a very commonly used function, so it's worth picking out the special cases for optimization */

#define IDENTITY_MIX 0
#define IDENTITY_MONO_MIX 1
#define SCALED_MONO_MIX 2
#define SCALED_MIX 3
#define ENVELOPED_MONO_MIX 4
#define ENVELOPED_MIX 5
#define ALL_MIX 6

static int mix_file_type(int out_chans, int in_chans, mus_float_t *mx, mus_any ***envs)
{
  if (envs)
    {
      if ((in_chans == 1) && (out_chans == 1)) 
	{
	  if (envs[0][0])
	    return(ENVELOPED_MONO_MIX);
	  return(SCALED_MONO_MIX);
	}
      else 
	{
	  if (mx)
	    return(ALL_MIX);
	  return(ENVELOPED_MIX); 
	}
    }
  if (mx)
    {
      int i, j;
      if ((in_chans == 1) && (out_chans == 1)) 
	{
	  if (mx[0] == 1.0)
	    return(IDENTITY_MONO_MIX); 
	  return(SCALED_MONO_MIX);
	}
      for (i = 0; i < out_chans; i++)
	for (j = 0; j < in_chans; j++)
	  if (((i == j) && (mx[i * in_chans + j] != 1.0)) ||
	      ((i != j) && (mx[i * in_chans + j] != 0.0)))
	    return(SCALED_MIX);
    }
  if ((in_chans == 1) && (out_chans == 1)) 
    return(IDENTITY_MONO_MIX);
  return(IDENTITY_MIX);
}


void mus_file_mix_with_reader_and_writer(mus_any *outf, mus_any *inf,
					 mus_long_t out_start, mus_long_t out_framples, mus_long_t in_start, 
					 mus_float_t *mx, int mx_chans,
					 mus_any ***envs)
{
  int mixtype, in_chans, out_chans;
  mus_long_t inc, outc, out_end;
  mus_float_t *out_data, *in_data, *local_mx;

  out_chans = mus_channels(outf);
  if (out_chans <= 0) 
    mus_error(MUS_NO_CHANNELS, S_mus_file_mix ": %s chans: %d", mus_describe(outf), out_chans);

  in_chans = mus_channels(inf);
  if (in_chans <= 0) 
    mus_error(MUS_NO_CHANNELS, S_mus_file_mix ": %s chans: %d", mus_describe(inf), in_chans);

  out_end = out_start + out_framples;
  mixtype = mix_file_type(out_chans, in_chans, mx, envs);

  in_data = (mus_float_t *)calloc((in_chans < out_chans) ? out_chans : in_chans, sizeof(mus_float_t));
  out_data = (mus_float_t *)calloc((in_chans < out_chans) ? out_chans : in_chans, sizeof(mus_float_t));

  local_mx = mx;

  switch (mixtype)
    {
    case ENVELOPED_MONO_MIX:
      {
	mus_any *e;
	e = envs[0][0];
	for (inc = in_start, outc = out_start; outc < out_end; inc++, outc++)
	  {
	    mus_file_to_frample(inf, inc, in_data);
	    mus_outa_to_file(outf, outc, in_data[0] * mus_env(e));
	  }
      }
      break;

    case ENVELOPED_MIX:
      if (!mx) 
	{
	  int i;
	  mx_chans = (in_chans < out_chans) ? out_chans : in_chans;
	  local_mx = (mus_float_t *)calloc(mx_chans * mx_chans, sizeof(mus_float_t));
	  for (i = 0; i < mx_chans; i++)
	    local_mx[i * mx_chans + i] = 1.0;
	}
      /* fall through */

    case ALL_MIX:
      /* the general case -- possible envs/scalers on every mixer cell */
      for (inc = in_start, outc = out_start; outc < out_end; inc++, outc++)
	{
	  int j, k;
	  for (j = 0; j < in_chans; j++)
	    for (k = 0; k < out_chans; k++)
	      if (envs[j][k])
		local_mx[j * mx_chans + k] = mus_env(envs[j][k]);
	  mus_frample_to_file(outf, outc, mus_frample_to_frample(local_mx, mx_chans, mus_file_to_frample(inf, inc, in_data), in_chans, out_data, out_chans));
	}
      if (!mx) free(local_mx);
      break;

    case IDENTITY_MONO_MIX:
      for (inc = in_start, outc = out_start; outc < out_end; inc++, outc++)
	{
	  mus_file_to_frample(inf, inc, in_data);
	  mus_outa_to_file(outf, outc, in_data[0]);
	}
      break;

    case IDENTITY_MIX:
      for (inc = in_start, outc = out_start; outc < out_end; inc++, outc++)
	mus_frample_to_file(outf, outc, mus_file_to_frample(inf, inc, in_data));
      break;

    case SCALED_MONO_MIX:
      {
	mus_float_t scl;
	scl = mx[0];
	for (inc = in_start, outc = out_start; outc < out_end; inc++, outc++)
	  {
	    mus_file_to_frample(inf, inc, in_data);
	    mus_outa_to_file(outf, outc, scl * in_data[0]);
	  }
      }
      break;

    case SCALED_MIX:
      for (inc = in_start, outc = out_start; outc < out_end; inc++, outc++)
	mus_frample_to_file(outf, outc, mus_frample_to_frample(mx, mx_chans, mus_file_to_frample(inf, inc, in_data), in_chans, out_data, out_chans));
      break;

    }
  free(in_data);
  free(out_data);
}


void mus_file_mix(const char *outfile, const char *infile, 
		  mus_long_t out_start, mus_long_t out_framples, mus_long_t in_start, 
		  mus_float_t *mx, int mx_chans, 
		  mus_any ***envs)
{
  int in_chans, out_chans, min_chans, mixtype;

  out_chans = mus_sound_chans(outfile);
  if (out_chans <= 0) 
    mus_error(MUS_NO_CHANNELS, S_mus_file_mix ": %s chans: %d", outfile, out_chans);

  in_chans = mus_sound_chans(infile);
  if (in_chans <= 0) 
    mus_error(MUS_NO_CHANNELS, S_mus_file_mix ": %s chans: %d", infile, in_chans);
  if (out_chans > in_chans) 
    min_chans = in_chans; 
  else min_chans = out_chans;

  mixtype = mix_file_type(out_chans, in_chans, mx, envs);
  if (mixtype == ALL_MIX)
    {
      mus_any *inf, *outf;
      /* the general case -- possible envs/scalers on every mixer cell */
      outf = mus_continue_sample_to_file(outfile);
      inf = mus_make_file_to_frample(infile);
      mus_file_mix_with_reader_and_writer(outf, inf, out_start, out_framples, in_start, mx, mx_chans, envs);
      mus_free(inf);
      mus_free(outf);
    }
  else
    {
      mus_long_t j = 0;
      int i, m, ofd, ifd;
      mus_float_t scaler;
      mus_any *e;
      mus_float_t **obufs, **ibufs;
      mus_long_t offk, curoutframples;

      /* highly optimizable cases */
      obufs = (mus_float_t **)malloc(out_chans * sizeof(mus_float_t *));
      for (i = 0; i < out_chans; i++) 
	obufs[i] = (mus_float_t *)malloc(clm_file_buffer_size * sizeof(mus_float_t));

      ibufs = (mus_float_t **)malloc(in_chans * sizeof(mus_float_t *));
      for (i = 0; i < in_chans; i++) 
	ibufs[i] = (mus_float_t *)malloc(clm_file_buffer_size * sizeof(mus_float_t));

      ifd = mus_sound_open_input(infile);
      mus_file_seek_frample(ifd, in_start);
      mus_file_read(ifd, in_start, clm_file_buffer_size, in_chans, ibufs);
      ofd = mus_sound_reopen_output(outfile, 
				    out_chans, 
				    mus_sound_sample_type(outfile), 
				    mus_sound_header_type(outfile), 
				    mus_sound_data_location(outfile));
      curoutframples = mus_sound_framples(outfile);
      mus_file_seek_frample(ofd, out_start);
      mus_file_read(ofd, out_start, clm_file_buffer_size, out_chans, obufs);
      mus_file_seek_frample(ofd, out_start);

      switch (mixtype)
	{
	case IDENTITY_MONO_MIX:
	  for (offk = 0, j = 0; offk < out_framples; offk++, j++)
	    {
	      if (j == clm_file_buffer_size)
		{
		  mus_file_write(ofd, 0, j - 1, out_chans, obufs);
		  j = 0;
		  mus_file_seek_frample(ofd, out_start + offk);
		  mus_file_read(ofd, out_start + offk, clm_file_buffer_size, out_chans, obufs);
		  mus_file_seek_frample(ofd, out_start + offk);
		  mus_file_read(ifd, in_start + offk, clm_file_buffer_size, in_chans, ibufs);
		}
	      obufs[0][j] += ibufs[0][j];
	    }
	  break;

	case IDENTITY_MIX:
	  for (offk = 0, j = 0; offk < out_framples; offk++, j++)
	    {
	      if (j == clm_file_buffer_size)
		{
		  mus_file_write(ofd, 0, j - 1, out_chans, obufs);
		  j = 0;
		  mus_file_seek_frample(ofd, out_start + offk);
		  mus_file_read(ofd, out_start + offk, clm_file_buffer_size, out_chans, obufs);
		  mus_file_seek_frample(ofd, out_start + offk);
		  mus_file_read(ifd, in_start + offk, clm_file_buffer_size, in_chans, ibufs);
		}
	      for (i = 0; i < min_chans; i++)
		obufs[i][j] += ibufs[i][j];
	    }
	  break;

	case SCALED_MONO_MIX:
	  scaler = mx[0];
	  for (offk = 0, j = 0; offk < out_framples; offk++, j++)
	    {
	      if (j == clm_file_buffer_size)
		{
		  mus_file_write(ofd, 0, j - 1, out_chans, obufs);
		  j = 0;
		  mus_file_seek_frample(ofd, out_start + offk);
		  mus_file_read(ofd, out_start + offk, clm_file_buffer_size, out_chans, obufs);
		  mus_file_seek_frample(ofd, out_start + offk);
		  mus_file_read(ifd, in_start + offk, clm_file_buffer_size, in_chans, ibufs);
		}
	      obufs[0][j] += (mus_float_t)(scaler * ibufs[0][j]);
	    }
	  break;

	case SCALED_MIX:
	  for (offk = 0, j = 0; offk < out_framples; offk++, j++)
	    {
	      if (j == clm_file_buffer_size)
		{
		  mus_file_write(ofd, 0, j - 1, out_chans, obufs);
		  j = 0;
		  mus_file_seek_frample(ofd, out_start + offk);
		  mus_file_read(ofd, out_start + offk, clm_file_buffer_size , out_chans, obufs);
		  mus_file_seek_frample(ofd, out_start + offk);
		  mus_file_read(ifd, in_start + offk, clm_file_buffer_size, in_chans, ibufs);
		}
	      for (i = 0; i < min_chans; i++)
		for (m = 0; m < in_chans; m++)
		  obufs[i][j] += (mus_float_t)(ibufs[m][j] * mx[m * mx_chans + i]);
	    }
	  break;

	case ENVELOPED_MONO_MIX:
	  e = envs[0][0];
	  for (offk = 0, j = 0; offk < out_framples; offk++, j++)
	    {
	      if (j == clm_file_buffer_size)
		{
		  mus_file_write(ofd, 0, j - 1, out_chans, obufs);
		  j = 0;
		  mus_file_seek_frample(ofd, out_start + offk);
		  mus_file_read(ofd, out_start + offk, clm_file_buffer_size, out_chans, obufs);
		  mus_file_seek_frample(ofd, out_start + offk);
		  mus_file_read(ifd, in_start + offk, clm_file_buffer_size, in_chans, ibufs);
		}
	      obufs[0][j] += (mus_float_t)(mus_env(e) * ibufs[0][j]);
	    }
	  break;

	case ENVELOPED_MIX:
	  e = envs[0][0];
	  for (offk = 0, j = 0; offk < out_framples; offk++, j++)
	    {
	      if (j == clm_file_buffer_size)
		{
		  mus_file_write(ofd, 0, j - 1, out_chans, obufs);
		  j = 0;
		  mus_file_seek_frample(ofd, out_start + offk);
		  mus_file_read(ofd, out_start + offk, clm_file_buffer_size, out_chans, obufs);
		  mus_file_seek_frample(ofd, out_start + offk);
		  mus_file_read(ifd, in_start + offk, clm_file_buffer_size, in_chans, ibufs);
		}
	      scaler = mus_env(e);
	      for (i = 0; i < min_chans; i++)
		obufs[i][j] += (mus_float_t)(scaler * ibufs[i][j]);
	    }
	  break;
	}

      if (j > 0) 
	mus_file_write(ofd, 0, j - 1, out_chans, obufs);
      if (curoutframples < (out_framples + out_start)) 
	curoutframples = out_framples + out_start;
      mus_sound_close_output(ofd, curoutframples * out_chans * mus_bytes_per_sample(mus_sound_sample_type(outfile)));
      mus_sound_close_input(ifd);
      for (i = 0; i < in_chans; i++) free(ibufs[i]);
      free(ibufs);
      for (i = 0; i < out_chans; i++) free(obufs[i]);
      free(obufs);
    }
}



/* ---------------- init clm ---------------- */

void mus_initialize(void)
{
  #define MULAW_ZERO 255
  #define ALAW_ZERO 213
  #define UBYTE_ZERO 128

  mus_generator_type = MUS_INITIAL_GEN_TAG;
  sampling_rate = MUS_DEFAULT_SAMPLING_RATE;
  w_rate = (TWO_PI / MUS_DEFAULT_SAMPLING_RATE);
  array_print_length = MUS_DEFAULT_ARRAY_PRINT_LENGTH;
  clm_file_buffer_size = MUS_DEFAULT_FILE_BUFFER_SIZE;

#if HAVE_FFTW3 && HAVE_COMPLEX_TRIG
  last_c_fft_size = 0;
  /* is there a problem if the caller built fftw with --enable-threads?  
   *   How to tell via configure that we need to initialize the thread stuff in libfftw?
   */
#endif

  sincs = 0;
  locsig_warned = NULL;

  sample_type_zero = (int *)calloc(MUS_NUM_SAMPLES, sizeof(int));
  sample_type_zero[MUS_MULAW] = MULAW_ZERO;
  sample_type_zero[MUS_ALAW] = ALAW_ZERO;
  sample_type_zero[MUS_UBYTE] = UBYTE_ZERO;
#if MUS_LITTLE_ENDIAN
  sample_type_zero[MUS_UBSHORT] = 0x80;
  sample_type_zero[MUS_ULSHORT] = 0x8000;
#else
  sample_type_zero[MUS_UBSHORT] = 0x8000;
  sample_type_zero[MUS_ULSHORT] = 0x80;
#endif 
}
