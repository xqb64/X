extern fn printf(fmt: str, ...) -> i32;

fn main(void) -> u32 {
  let mut x: u32 = 0;
  let y: u32 = 1;
  
  loop {
    printf("Hello, world!  %d\n", x);
    if (x == 10) {
      break;
    }
    x += 1;
  }
  
  goto spam;

  ret x;

spam:
  ret y+1;
}
