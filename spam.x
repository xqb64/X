fn main(void) -> i32 {
  let f: i32 = 10;
  let poison: i32 = 0;  
  while (f > 0) {
    f = f - 1;
    if (f == 1) {
      continue;
      poison = 255;
    }
  }

  ret poison;
}
