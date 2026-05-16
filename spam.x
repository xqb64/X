extern fn puts(s: str) -> i32;

fn main(void) -> i32 {
  let x: i32 = 0;
  {
    let z: str = "Hello world!";
  }
  ret x;
}
