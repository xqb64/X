fn main(void) -> u32 {
  let x: u32 = 10;
  let y: u32 = 20;

  if (x == 10) {
    ret x + y;
  }

  ret y;
}
