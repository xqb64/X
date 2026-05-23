#ifndef X_VECTOR_H
#define X_VECTOR_H

#define Vector(T) \
  struct {        \
    int capacity; \
    int len;      \
    T *data;      \
  }

#define vec_insert(vec, item)                                             \
  /* Capacity grows exponentially (doubling) when the vector is full.     \
   * This geometric growth strategy ensures an amortized O(1) time        \
   * complexity for insertions, avoiding the O(n) bottleneck that         \
   * would occur with linear reallocation. */                             \
  do {                                                                    \
    if ((vec)->len >= (vec)->capacity) {                                  \
      (vec)->capacity = (vec)->capacity == 0 ? 2 : (vec)->capacity * 2;   \
      (vec)->data =                                                       \
          realloc((vec)->data, sizeof((vec)->data[0]) * (vec)->capacity); \
    }                                                                     \
    (vec)->data[(vec)->len++] = (item);                                   \
  } while (0)

#define vec_free(vec) free((vec)->data);

typedef Vector(int) VecInt;

#endif
