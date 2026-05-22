extern fn printf(fmt: str, ...) -> i32;

struct Spam {
  a: u8,
  b: i32,
  c: u64,
}

fn main(void) -> u32 {
  let mut x: u32 = 0;
  let y: u32 = 1;

  let z: Spam = Spam {a: 0, b: 0, c: 0};

  loop {
    printf("Hello, world!  %d\n", x);
    if (x == 10) {
      break;
    }
    x += 1;
  }
 
  printf("sizeof(x) is: %d\n", sizeof(z));
 
  goto spam;

  ret x;

spam:
  ret y+1;
}
