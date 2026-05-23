extern fn putchar(c: u32) -> u32;

fn main(void) -> u32 {
  let hot: u32 = 1;

  let a: u32 = 2;
  let b: u32 = 3;
  let c: u32 = 4;
  let d: u32 = 5;
  let e: u32 = 6;
  let f: u32 = 7;
  let g: u32 = 8;
  let h: u32 = 9;
  let i: u32 = 10;
  let j: u32 = 11;
  let k: u32 = 12;
  let l: u32 = 13;
  let m: u32 = 14;
  let n: u32 = 15;
  let o: u32 = 16;
  let p: u32 = 17;

  putchar(65);

  ret hot + hot + hot + hot + hot + hot + hot + hot
    + a + b + c + d + e + f + g + h + i + j + k + l + m + n + o + p;
}
