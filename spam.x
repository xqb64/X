extern fn printf(fmt: str, ...) -> i32;

struct Point {
  a: i8,
  b: i32,
  c: i64,
  union {
    mem: i32,
    cpu: i64,
  } as,
}

fn main(void) -> i32 {
  let p: Point = Point { .as.mem: 64 };
  let ptr: *Point = &p;

  let mem: i32 = ptr->as.mem;

  ret mem;
}
