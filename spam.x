fn main(void) -> i32 {
  let f: i32 = 10;
  
  while (f > 0) {
    if (f == 1) {
      continue;
    }
    f = f - 1;
  }

  ret f;
}
