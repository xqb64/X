extern fn printf(fmt: str, ...) -> i32;

struct S {
  a: enum { EGG, LOL = 3, WOOT },
  b: union {
    mem: i32,
    cpu: i64,
  },
  c: u8,
}

fn main(void) -> u32 {
  let x: u8 = 0;
  ret LOL as u32;
}
