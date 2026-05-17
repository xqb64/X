extern fn printf(s: str, ...) -> i32;

fn main(void) -> i32 {
  let x: i32 = 1;
  let z: i32 = 2;
  if (x < z && z > x) {
    printf("true\n");
  } else {
    printf("false\n");
  }
  ret 0;
}
