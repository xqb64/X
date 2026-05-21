extern fn printf(fmt: str, ...) -> i32;

fn main(void) -> u32 {
  let x: u32 = 0;
  let y: u32 = 1;

  goto spam;

  ret x;

spam:  
  ret y; 
}
