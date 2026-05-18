extern fn printf(s: str, ...) -> i32;
extern fn malloc(n: i32) -> *void;
extern fn free(ptr: *void) -> void;

fn main(void) -> i32 {
  let x: *i32 = malloc(4);
  *x = 32;
  printf("%d\n", *x);
  free(x);
  printf("yay!\n");
  ret 0;
}

