fn fib(n: i32) -> i32 {
  if (n < 2) { return n; }
  return fib(n-1)+fib(n-2);
}

fn main(void) -> i32 {
  let f: i32 = fib(10);
  return f;
}
