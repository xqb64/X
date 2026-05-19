extern fn printf(fmt: str, ...) -> i32;

struct Point {
  a: i8,
  b: i32,
  c: i64,
}

fn main(void) -> i32 {
  let p: Point = Point { a: 1, b: 2, c: 3 };
  printf("a: %d, b: %d, c: %d\n", p.a, p.b, p.c);
  ret 0;
}
