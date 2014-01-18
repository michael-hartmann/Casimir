#ifndef __GIVENS_H
#define __GIVENS_H
#include <stdio.h>
#include <quadmath.h>

#define __float __float128
#define __abs fabsq
#define __sqrt sqrtq
#define __log logq
#define __copysign copysignq

typedef struct {
    size_t size;
    __float *M;
} matrix_t;

matrix_t *matrix_alloc(size_t size);
void matrix_free(matrix_t *m);
void matrix_fprintf(const matrix_t *m, FILE *stream, const char *format, const char *lines, const char *rows);

__float inline matrix_get(const matrix_t *m, size_t i, size_t j);
void inline matrix_set(matrix_t *m, size_t i, size_t j, __float x);

__float matrix_logdet(matrix_t *M);

void matrix_info(FILE *stream, matrix_t *M);

#endif