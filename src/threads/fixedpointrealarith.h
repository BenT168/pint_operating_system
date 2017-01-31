#include <stdint.h>

#define F 1 << 14

#define MAX_32BIT_INT ((1 << 31) - 1)

#define MIN_32BIT_INT (-(1 << 31))


int convert_to_fixed_point(int n);
int convert_to_int_zero(int x);
int convert_to_int_nearest(int x);
int add_x_y(int x, int y);
int sub_x_y(int x, int y);
int add_x_n(int x, int n);
int sub_x_n(int x, int n);
int mul_x_y(int x, int y);
int mul_x_n(int x, int n);
int div_x_y(int x, int y);
int div_x_n(int x, int n);


//Convert n to fixed point: n * f
 int convert_to_fixed_point(int n) {
  return n * F;
}

//Convert x to integer (rounding toward zero): x / f
int convert_to_int_zero(int x) {
  return x / F;
}

/* Convert x to integer (rounding to nearest):
   (x + f / 2) / f if x >= 0,
   (x - f / 2) / f if x <= 0.
*/
int convert_to_int_nearest(int x) {
  if(x >= 0) {
    return (x + (F / 2)) / F;
  }
  return (x - (F / 2)) / F;
}

// Add x and y: x + y
int add_x_y(int x, int y) {
  return x + y;
}

// Subtract y from x: x - y
int sub_x_y(int x, int y) {
  return x - y;
}

// Add x and n: x + n * f

int add_x_n(int x, int n) {
  return x + (n * F);
}

// Subtract n from x: x - n * f
int sub_x_n(int x, int n) {
  return x - (n * F);
}

// Multiply x by y: ((int64_t) x) * y / f
int mul_x_y(int x, int y) {
  return ((int64_t) x) * y / F;
}

// Multiply x by n: x * n
int mul_x_n(int x, int n) {
  return x * n;
}

// Divide x by y: ((int64_t) x) * f / y
int div_x_y(int x, int y) {
  return ((int64_t) x) * F / y;
}

// Divide x by n: x / n
 int div_x_n(int x, int n) {
  return x / n;
}
