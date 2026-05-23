extern fn printf(f: str, ...) -> i32;

fn live_across_call(a: f64, b: f64) -> f64 {
  let x: f64 = a + b;
  printf("inside\n");
  ret x + 1.0;
}

fn main() -> i32 {
  let r: f64 = live_across_call(2.5, 3.5);
  printf("%f\n", r);
  ret 0;
}
