fn fib(n: i32) -> i32 {
  if (n >= 2) { return n; }
  return 0;
}

fn main(void) -> i32 {
  let f: i32 = fib(10);
  return f;
}
