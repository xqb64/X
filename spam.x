extern fn printf(s: str, ...) -> i32;

fn main(void) -> i32 {
  let x: f32 = -1.0;
  let y: f32 = 2.5;
  printf("%f", x+y);
  ret 0;
}
