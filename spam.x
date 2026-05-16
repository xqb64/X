extern fn printf(s: str, ...) -> i32;

fn main(void) -> i32 {
  let x: f32 = -1.0;
  let y: f32 = 2.0;
  printf("spam: %f\n", x+y);
  ret 0;
}
