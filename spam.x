extern fn putchar(c: u32) -> u32;

fn main(void) -> u32 {
  let x: u32 = 40;
  let y: u32 = 2;

  putchar(65);

  ret x + y;
}
