extern fn printf(s: str, ...) -> i32;

fn main(void) -> i32 {
  let x: bool = true;
  let z: bool = false;
  if (x && z) {
    printf("%d\n", x && z);
  } else {
    printf("%d\n", x && z);
  }
  ret 0;
}
