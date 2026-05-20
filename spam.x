extern fn printf(fmt: str, ...) -> i32;

struct Point {
  a: i8,
  b: i32,
  c: i64,
}

fn main(void) -> i32 {
  let p: Point = Point { a: 1, b: 2, c: 3 };
  let ptr: *Point = &p;

  let a: i32 = ptr->c;

  ret a;
}
