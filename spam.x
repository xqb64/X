extern fn printf(fmt: str, ...) -> i32;

struct Point {
  a: i8,
  b: i32,
  c: i64,
}

fn print_point(p: Point) -> Point {
  printf("a: %d, b: %d, c: %d\n", p.a, p.b, p.c);
  ret p;
}

fn main(void) -> i32 {
  let p: Point = Point { a: 1, b: 2, c: 3 };
  p = print_point(p);
  ret p.b;
}
