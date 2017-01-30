#ifndef THREADS_FIXEDPOINTREALARITH_H
#define THREADS_FIXEDPOINTREALARITH_H

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

#endif
