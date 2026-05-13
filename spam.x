fn fib(n: i32) -> i32 {
  if (n >= 2) { ret n; }
  ret fib(n-1)+fib(n-2);
}

fn main(void) -> i32 {
  ret fib(10);
}
