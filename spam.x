extern fn puts(s: str) -> i32;

fn main(void) -> i32 {
  let f: i32 = 10;
  while (f >= 0) {
    puts("Hello, world!");
    f = f-1;
  }
  ret 0;
}
