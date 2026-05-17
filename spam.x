extern fn printf(s: str, ...) -> i32;

fn main(void) -> i32 {
  let x: bool = false;
  if (!x) {
    printf("not x: %d\n", 1);
  } else {
    printf("x: %d", 1);
  }
  ret 0;
}
